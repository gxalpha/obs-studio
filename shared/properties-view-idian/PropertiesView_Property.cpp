#include "PropertiesView.hpp"

#include <double-slider.hpp>
#include <Idian/Idian.hpp>
#include <obs.hpp>
#include <slider-ignorewheel.hpp>
#include <util/dstr.hpp>

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSlider>

using namespace idian;

namespace properties_view {
Row *PropertiesView::createPropertyInvalid(obs_property_t *prop, const char *reason)
{
	Row *row = new Row();
	DStr str;
	dstr_printf(str, "<em>Unsupported property '%s': %s</em>", obs_property_name(prop), reason);
	QLabel *label = new QLabel(str->array, row);
	label->setStyleSheet("color: red;");
	row->setSuffix(label);
	return row;
}

Row *PropertiesView::createPropertyBool(obs_property_t *prop)
{
	const bool value = getPropertyValue<bool>(prop);
	Row *row = new Row();
	ToggleSwitch *toggle = new ToggleSwitch(value);
	row->setSuffix(toggle);

	connect(toggle, &QAbstractButton::toggled, this, [this, prop](bool checked) {
		obs_data_set_bool(settings.get(), obs_property_name(prop), checked);
		controlChanged(prop, checked);
	});
	return row;
}

Row *PropertiesView::createPropertyInt(obs_property_t *prop)
{
	const int min = obs_property_int_min(prop);
	const int max = obs_property_int_max(prop);
	const int step = obs_property_int_step(prop);
	const enum obs_number_type type = obs_property_int_type(prop);
	const char *suffix = obs_property_int_suffix(prop);
	const int value = getPropertyValue<int>(prop);

	Row *row = new Row();

	SpinBox *idianSpinBox = new SpinBox(row);
	QSpinBox *spinBox = idianSpinBox->spinBox();
	spinBox->setMinimum(min);
	spinBox->setMaximum(max);
	spinBox->setSingleStep(step);
	spinBox->setValue(value);
	spinBox->setSuffix(suffix);
	row->setSuffix(idianSpinBox);
	connect(spinBox, &QSpinBox::valueChanged, this, [this, prop](int value) {
		obs_data_set_int(settings.get(), obs_property_name(prop), value);
		controlChanged(prop, value);
	});

	if (type == OBS_NUMBER_SLIDER) {
		QSlider *slider = new SliderIgnoreScroll(row);
		slider->setOrientation(Qt::Horizontal);
		slider->setMinimum(min);
		slider->setMaximum(max);
		slider->setSingleStep(step);
		slider->setValue(value);
		row->setLargeContent(slider);
		connect(slider, &QSlider::valueChanged, spinBox, &QSpinBox::setValue);
		connect(spinBox, &QSpinBox::valueChanged, slider, &QSlider::setValue);
	}

	return row;
}

Row *PropertiesView::createPropertyDouble(obs_property_t *prop)
{
	const double min = obs_property_float_min(prop);
	const double max = obs_property_float_max(prop);
	const double step = obs_property_float_step(prop);
	const enum obs_number_type type = obs_property_float_type(prop);
	const char *suffix = obs_property_float_suffix(prop);
	const double value = getPropertyValue<double>(prop);

	Row *row = new Row();

	DoubleSpinBox *idianSpinBox = new DoubleSpinBox(row);
	QDoubleSpinBox *spinBox = idianSpinBox->spinBox();
	spinBox->setMinimum(min);
	spinBox->setMaximum(max);
	spinBox->setSingleStep(step);
	spinBox->setValue(value);
	spinBox->setSuffix(suffix);
	row->setSuffix(idianSpinBox);
	connect(spinBox, &QDoubleSpinBox::valueChanged, this, [this, prop](double value) {
		obs_data_set_double(settings.get(), obs_property_name(prop), value);
		controlChanged(prop, value);
	});

	if (type == OBS_NUMBER_SLIDER) {
		DoubleSlider *slider = new DoubleSlider(row);
		slider->setOrientation(Qt::Horizontal);
		slider->setDoubleConstraints(min, max, step, value);
		row->setLargeContent(slider);
		connect(slider, &DoubleSlider::doubleValChanged, spinBox, &QDoubleSpinBox::setValue);
		connect(spinBox, &QDoubleSpinBox::valueChanged, slider, &DoubleSlider::setDoubleVal);
	}

	return row;
}

Row *PropertiesView::createPropertyText(obs_property_t *prop)
{
	const enum obs_text_type type = obs_property_text_type(prop);
	const char *value = getPropertyValue<const char *>(prop);
	switch (type) {
	case OBS_TEXT_DEFAULT:
	case OBS_TEXT_PASSWORD: {
		Row *row = new Row();
		QLineEdit *lineEdit = new QLineEdit(row);
		lineEdit->setText(value);
		row->setLargeContent(lineEdit);
		connect(lineEdit, &QLineEdit::textChanged, this, [this, prop](const QString &text) {
			const QByteArray array = text.toUtf8();
			const char *value = array.constData();
			obs_data_set_string(settings.get(), obs_property_name(prop), value);
			controlChanged(prop, value);
		});

		if (type == OBS_TEXT_PASSWORD) {
			QCheckBox *checkBox = new QCheckBox();
			checkBox->setText(tr("Show"));
			lineEdit->setEchoMode(QLineEdit::Password);
			connect(checkBox, &QAbstractButton::toggled, lineEdit, [lineEdit](bool checked) {
				lineEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
			});
			row->setSuffix(checkBox);
		}
		return row;
	}
	case OBS_TEXT_MULTILINE: {
		Row *row = new Row();
		QPlainTextEdit *textEdit = new QPlainTextEdit(row);
		textEdit->setPlainText(value);
		row->setLargeContent(textEdit);
		connect(textEdit, &QPlainTextEdit::textChanged, this, [this, textEdit, prop]() {
			const QByteArray array = textEdit->toPlainText().toUtf8();
			const char *value = array.constData();
			obs_data_set_string(settings.get(), obs_property_name(prop), value);
			controlChanged(prop, value);
		});
		return row;
	}
	case OBS_TEXT_INFO: {
		// TODO: add.
		return createPropertyInvalid(prop, "Subtype 'OBS_TEXT_INFO' is not yet implemented.");
	}
	}
}

Row *PropertiesView::createPropertyButton(obs_property_t *prop)
{
	const enum obs_button_type type = obs_property_button_type(prop);
	switch (type) {
	case OBS_BUTTON_DEFAULT: {
		Row *row = new Row();
		QPushButton *button = new QPushButton(row);
		button->setText("Execute"); // TODO: What should we put here?
		row->setSuffix(button);
		connect(button, &QPushButton::pressed, this,
			[this, prop]() { obs_property_button_clicked(prop, obsObject); });
		return row;
	}
	case OBS_BUTTON_URL: {
		// TODO: add. Alternatively, deprecate OBS_BUTTON_URL.
		return createPropertyInvalid(prop, "Subtype 'OBS_BUTTON_URL' is not (yet?) implemented.");
	}
	}
}

CollapsibleRow *PropertiesView::createPropertyGroup(obs_property_t *prop)
{
	CollapsibleRow *container = new CollapsibleRow();
	enum obs_group_type type = obs_property_group_type(prop);
	if (type == OBS_GROUP_CHECKABLE) {
		container->setCheckable(true);
		container->setChecked(getPropertyValue<bool>(prop));
		connect(container, &CollapsibleRow::toggled, this, [this, prop](bool checked) {
			obs_data_set_bool(settings.get(), obs_property_name(prop), checked);
			controlChanged(prop, checked);
		});
	}

	obs_properties_t *subprops = obs_property_group_content(prop);
	obs_property_t *subprop = obs_properties_first(subprops);
	do {
		GenericRow *subRow = createProperty(subprop);
		if (subRow) {
			container->addRow(subRow);
		}
	} while (obs_property_next(&subprop));

	return container;
}
} // namespace properties_view
