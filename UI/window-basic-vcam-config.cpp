#include "window-basic-vcam-config.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"

#include <util/util.hpp>
#include <util/platform.h>

#include <QStandardItem>
#import <CoreGraphics/CoreGraphics.h>

OBSBasicVCamConfig::OBSBasicVCamConfig(const VCamConfig &_config,
				       bool _vcamActive, QWidget *parent)
	: config(_config),
	  vcamActive(_vcamActive),
	  activeType(_config.type),
	  QDialog(parent),
	  ui(new Ui::OBSBasicVCamConfig)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->setupUi(this);

	ui->outputType->addItem(QTStr("Basic.VCam.OutputType.Program"),
				(int)VCamOutputType::ProgramView);
	ui->outputType->addItem(QTStr("StudioMode.Preview"),
				(int)VCamOutputType::PreviewOutput);
	ui->outputType->addItem(QTStr("Basic.Scene"),
				(int)VCamOutputType::SceneOutput);
	ui->outputType->addItem(QTStr("Basic.Main.Source"),
				(int)VCamOutputType::SourceOutput);

	ui->outputType->setCurrentIndex(
		ui->outputType->findData((int)config.type));
	OutputTypeChanged();
	connect(ui->outputType, &QComboBox::currentIndexChanged, this,
		&OBSBasicVCamConfig::OutputTypeChanged);

	connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
		&OBSBasicVCamConfig::UpdateConfig);

	connect(ui->placeholderOptions, &QPushButton::pressed, [this]() {
		QMenu *menu = new QMenu(this);
		menu->setAttribute(Qt::WA_DeleteOnClose);
		menu->addAction("Set", [&]() {
			obs_output_t *output =
				obs_get_output_by_name("virtualcam_output");
			proc_handler_t *handler =
				obs_output_get_proc_handler(output);

			calldata_t cd;
			uint8_t stack[128];
			calldata_init_fixed(&cd, stack, sizeof(stack));

		});
		menu->addAction("Get", [this]() {
			obs_output_t *output =
				obs_get_output_by_name("virtualcam_output");
			proc_handler_t *handler =
				obs_output_get_proc_handler(output);

			calldata_t cd;
			uint8_t stack[128];
			calldata_init_fixed(&cd, stack, sizeof(stack));
			proc_handler_call(handler, "get_placeholder", &cd);

			CGImageRef cgImage =
				(CGImageRef)calldata_ptr(&cd, "image_data");
			const size_t width = CGImageGetWidth(cgImage);
			const size_t height = CGImageGetHeight(cgImage);
			QImage qImage(width, height,
				      QImage::Format_ARGB32_Premultiplied);

			CGColorSpaceRef colorSpace =
				CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
			CGContextRef context = CGBitmapContextCreate(
				qImage.bits(), width, height, 8,
				qImage.bytesPerLine(), colorSpace,
				kCGImageAlphaPremultipliedFirst |
					kCGBitmapByteOrder32Host);

			CGRect rect = CGRectMake(0, 0, width, height);
			CGContextDrawImage(context, rect, cgImage);

			CFRelease(colorSpace);
			CFRelease(context);

			ui->placeholderPreview->setPixmap(
				QPixmap::fromImage(qImage).scaled(
					320, 180, Qt::KeepAspectRatio,
					Qt::SmoothTransformation));
		});
		menu->addAction("Reset", []() {
			obs_output_t *output =
				obs_get_output_by_name("virtualcam_output");
			proc_handler_t *handler =
				obs_output_get_proc_handler(output);

			calldata_t cd;
			uint8_t stack[128];
			calldata_init_fixed(&cd, stack, sizeof(stack));
		});
		menu->popup(QCursor::pos());
	});
}

void OBSBasicVCamConfig::OutputTypeChanged()
{
	VCamOutputType type =
		(VCamOutputType)ui->outputType->currentData().toInt();
	ui->outputSelection->setDisabled(false);

	auto list = ui->outputSelection;
	list->clear();

	switch (type) {
	case VCamOutputType::Invalid:
	case VCamOutputType::ProgramView:
	case VCamOutputType::PreviewOutput:
		ui->outputSelection->setDisabled(true);
		list->addItem(QTStr("Basic.VCam.OutputSelection.NoSelection"));
		break;
	case VCamOutputType::SceneOutput: {
		// Scenes in default order
		BPtr<char *> scenes = obs_frontend_get_scene_names();
		for (char **temp = scenes; *temp; temp++) {
			list->addItem(*temp);

			if (config.scene.compare(*temp) == 0)
				list->setCurrentIndex(list->count() - 1);
		}
		break;
	}
	case VCamOutputType::SourceOutput: {
		// Sources in alphabetical order
		std::vector<std::string> sources;
		auto AddSource = [&](obs_source_t *source) {
			auto name = obs_source_get_name(source);

			if (!(obs_source_get_output_flags(source) &
			      OBS_SOURCE_VIDEO))
				return;

			sources.push_back(name);
		};
		using AddSource_t = decltype(AddSource);

		obs_enum_sources(
			[](void *data, obs_source_t *source) {
				auto &AddSource =
					*static_cast<AddSource_t *>(data);
				if (!obs_source_removed(source))
					AddSource(source);
				return true;
			},
			static_cast<void *>(&AddSource));

		// Sort and select current item
		sort(sources.begin(), sources.end());
		for (auto &&source : sources) {
			list->addItem(source.c_str());

			if (config.source == source)
				list->setCurrentIndex(list->count() - 1);
		}
		break;
	}
	}

	if (!vcamActive)
		return;

	requireRestart = (activeType == VCamOutputType::ProgramView &&
			  type != VCamOutputType::ProgramView) ||
			 (activeType != VCamOutputType::ProgramView &&
			  type == VCamOutputType::ProgramView);

	ui->warningLabel->setVisible(requireRestart);
}

void OBSBasicVCamConfig::UpdateConfig()
{
	VCamOutputType type =
		(VCamOutputType)ui->outputType->currentData().toInt();
	switch (type) {
	case VCamOutputType::ProgramView:
	case VCamOutputType::PreviewOutput:
		break;
	case VCamOutputType::SceneOutput:
		config.scene = ui->outputSelection->currentText().toStdString();
		break;
	case VCamOutputType::SourceOutput:
		config.source =
			ui->outputSelection->currentText().toStdString();
		break;
	default:
		// unknown value, don't save type
		return;
	}

	config.type = type;

	if (requireRestart) {
		emit AcceptedAndRestart(config);
	} else {
		emit Accepted(config);
	}
}
