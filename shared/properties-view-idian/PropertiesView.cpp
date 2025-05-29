#include "PropertiesView.hpp"

#include <util/dstr.hpp>
#include <QLineEdit>
#include <QPlainTextEdit>

using namespace idian;

namespace properties_view {
PropertiesView::PropertiesView(GetProperties getPropertiesFunction, GetSettings getSettingsFunction, QWidget *parent)
	: VScrollArea(parent),
	  getProperties(getPropertiesFunction),
	  getSettings(getSettingsFunction),
	  properties(nullptr, obs_properties_destroy),
	  settings(nullptr, obs_data_release),
	  group(new idian::Group(this))
{
	group->setTitle("Properties");
	setWidgetResizable(true);
	setFrameShape(Shape::NoFrame);
	QWidget *widget = new QWidget();
	QLayout *layout = new QVBoxLayout(widget);
	layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
	layout->addWidget(group);
	widget->setLayout(layout);

	setWidget(widget);

	QMetaObject::invokeMethod(this, &PropertiesView::reloadProperties, Qt::QueuedConnection);
}

namespace {
template<typename T> T getPropertyValue(obs_property_t *property, obs_data_t *settings)
{
	const char *name = obs_property_name(property);
	if constexpr (std::is_same_v<T, bool>) {
		return obs_data_get_bool(settings, name);
	} else if constexpr (std::is_same_v<T, int>) {
		return obs_data_get_int(settings, name);
	} else if constexpr (std::is_same_v<T, double>) {
		return obs_data_get_double(settings, name);
	} else if constexpr (std::is_same_v<T, const char *>) {
		return obs_data_get_string(settings, name);
	} else {
		static_assert(false, "Not implemented");
	}
}
} // namespace

Row *PropertiesView::createPropertyBool(obs_property_t *prop)
{
	const bool value = getPropertyValue<bool>(prop, settings.get());
	Row *row = new Row();
	ToggleSwitch *toggle = new ToggleSwitch(value);
	row->setSuffix(toggle);

	connect(toggle, &QAbstractButton::toggled, this, [this, prop](bool checked) {
		obs_data_set_bool(settings.get(), obs_property_name(prop), checked);
		controlChanged(prop);
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
	const int value = getPropertyValue<int>(prop, settings.get());

	Row *row = new Row();
	// TODO Slider vs Spinbox, see type
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
		controlChanged(prop);
	});
	return row;
}

Row *PropertiesView::createPropertyDouble(obs_property_t *prop)
{
	const double min = obs_property_float_min(prop);
	const double max = obs_property_float_max(prop);
	const double step = obs_property_float_step(prop);
	const enum obs_number_type type = obs_property_float_type(prop);
	const char *suffix = obs_property_int_suffix(prop);
	const double value = getPropertyValue<double>(prop, settings.get());

	Row *row = new Row();
	// TODO Slider vs Spinbox, see type
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
		controlChanged(prop);
	});
	return row;
}

Row *PropertiesView::createPropertyText(obs_property_t *prop)
{
	const enum obs_text_type type = obs_property_text_type(prop);
	const char *value = getPropertyValue<const char *>(prop, settings.get());
	switch (type) {
	case OBS_TEXT_DEFAULT:
	case OBS_TEXT_PASSWORD: {
		Row *row = new Row();
		QLineEdit *lineEdit = new QLineEdit(row);
		lineEdit->setText(value);
		row->setLargeContent(lineEdit);
		connect(lineEdit, &QLineEdit::textChanged, this, [this, prop](const QString &text) {
			obs_data_set_string(settings.get(), obs_property_name(prop), text.toStdString().c_str());
			controlChanged(prop);
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
			QString text = textEdit->toPlainText();
			obs_data_set_string(settings.get(), obs_property_name(prop), text.toStdString().c_str());
			controlChanged(prop);
		});
		return row;
	}
	case OBS_TEXT_INFO: {
		blog(LOG_INFO, "Info text type not yet supported"); // TODO: add.
		return createPropertyNull(prop);
	}
	}
}

// TODO: Everything in this namespace is copied 1:1 from properties-view and should be rewritten.
namespace {
QVariant propertyListToQVariant(obs_property_t *prop, size_t idx)
{
	obs_combo_format format = obs_property_list_format(prop);

	QVariant var;
	if (format == OBS_COMBO_FORMAT_INT) {
		long long val = obs_property_list_item_int(prop, idx);
		var = QVariant::fromValue<long long>(val);
	} else if (format == OBS_COMBO_FORMAT_FLOAT) {
		double val = obs_property_list_item_float(prop, idx);
		var = QVariant::fromValue<double>(val);
	} else if (format == OBS_COMBO_FORMAT_STRING) {
		var = QByteArray(obs_property_list_item_string(prop, idx));
	} else if (format == OBS_COMBO_FORMAT_BOOL) {
		bool val = obs_property_list_item_bool(prop, idx);
		var = QVariant::fromValue<bool>(val);
	}
	return var;
}
template<long long get_int(obs_data_t *, const char *), double get_double(obs_data_t *, const char *),
	 const char *get_string(obs_data_t *, const char *), bool get_bool(obs_data_t *, const char *)>
static QVariant from_obs_data(obs_data_t *data, const char *name, obs_combo_format format)
{
	switch (format) {
	case OBS_COMBO_FORMAT_INT:
		return QVariant::fromValue(get_int(data, name));
	case OBS_COMBO_FORMAT_FLOAT:
		return QVariant::fromValue(get_double(data, name));
	case OBS_COMBO_FORMAT_STRING:
		return QByteArray(get_string(data, name));
	case OBS_COMBO_FORMAT_BOOL:
		return QVariant::fromValue(get_bool(data, name));
	default:
		return QVariant();
	}
}

static QVariant from_obs_data(obs_data_t *data, const char *name, obs_combo_format format)
{
	return from_obs_data<obs_data_get_int, obs_data_get_double, obs_data_get_string, obs_data_get_bool>(data, name,
													    format);
}
} // namespace

Row *PropertiesView::createPropertyList(obs_property_t *prop)
{
	const char *name = obs_property_name(prop);
	enum obs_combo_type type = obs_property_list_type(prop);
	enum obs_combo_format format = obs_property_list_format(prop);

	if (type == OBS_COMBO_TYPE_EDITABLE) {
		blog(LOG_INFO, "Editable list type not yet supported"); // TODO: add.
		return createPropertyNull(prop);
	}

	Row *row = new Row();
	// TODO: Honor type
	ComboBox *comboBox = new ComboBox(row);
	size_t count = obs_property_list_item_count(prop);
	for (size_t i = 0; i < count; ++i) {
		const char *name = obs_property_list_item_name(prop, i);
		comboBox->addItem(name, propertyListToQVariant(prop, i));
		// TODO: Disabled items.
	}
	comboBox->setCurrentIndex(comboBox->findData(from_obs_data(settings.get(), name, format)));
	connect(comboBox, &QComboBox::currentIndexChanged, this, [this, comboBox, prop]() {
		QVariant data = comboBox->currentData();
		// TODO: This was simply copied from properties-view to just get something working and could likely be much nicer.
		const char *name = obs_property_name(prop);
		switch (obs_property_list_format(prop)) {
		case OBS_COMBO_FORMAT_INVALID:
			return;
		case OBS_COMBO_FORMAT_INT:
			obs_data_set_int(settings.get(), name, data.value<long long>());
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			obs_data_set_double(settings.get(), name, data.value<double>());
			break;
		case OBS_COMBO_FORMAT_STRING:
			obs_data_set_string(settings.get(), name, data.toByteArray().constData());
			break;
		case OBS_COMBO_FORMAT_BOOL:
			obs_data_set_bool(settings.get(), name, data.value<double>());
			break;
		}

		controlChanged(prop);
	});
	row->setSuffix(comboBox);
	return row;
}

CollapsibleRow *PropertiesView::createPropertyGroup(obs_property_t *prop)
{
	CollapsibleRow *container = new CollapsibleRow();
	enum obs_group_type type = obs_property_group_type(prop);
	if (type == OBS_GROUP_CHECKABLE) {
		container->setCheckable(true);
		container->setChecked(getPropertyValue<bool>(prop, settings.get()));
		connect(container, &CollapsibleRow::toggled, this, [this, prop](bool checked) {
			obs_data_set_bool(settings.get(), obs_property_name(prop), checked);
			controlChanged(prop);
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

Row *PropertiesView::createPropertyNull(obs_property_t *prop)
{
	Row *row = new Row();
	DStr str;
	dstr_printf(str, "<em>Unsupported property '%s' of type '%d'.</em>", obs_property_name(prop),
		    obs_property_get_type(prop));
	QLabel *label = new QLabel(str->array, row);
	label->setStyleSheet("color: red;");
	row->setSuffix(label);
	return row;
}

GenericRow *PropertiesView::createProperty(obs_property_t *prop)
{
	if (!obs_property_visible(prop)) {
		return nullptr;
	}

	GenericRow *row = nullptr;
	switch (obs_property_get_type(prop)) {
	case OBS_PROPERTY_INVALID:
		row = createPropertyNull(prop);
		break;
	case OBS_PROPERTY_BOOL:
		row = createPropertyBool(prop);
		break;
	case OBS_PROPERTY_INT:
		row = createPropertyInt(prop);
		break;
	case OBS_PROPERTY_FLOAT:
		row = createPropertyDouble(prop);
		break;
	case OBS_PROPERTY_TEXT:
		row = createPropertyText(prop);
		break;
		//case OBS_PROPERTY_PATH:
	case OBS_PROPERTY_LIST:
		row = createPropertyList(prop);
		break;
		//case OBS_PROPERTY_COLOR:
		//case OBS_PROPERTY_BUTTON:
		//case OBS_PROPERTY_FONT:
		//case OBS_PROPERTY_EDITABLE_LIST:
		//case OBS_PROPERTY_FRAME_RATE:
	case OBS_PROPERTY_GROUP:
		row = createPropertyGroup(prop);
		break;
		//case OBS_PROPERTY_COLOR_ALPHA:
	default:
		row = createPropertyNull(prop);
		break;
	}

	row->setTitle(obs_property_description(prop));
	const char *long_desc = obs_property_long_description(prop);
	if (long_desc) {
		row->setDescription(long_desc);
	}

	row->setEnabled(obs_property_enabled(prop));

	return row;
}

void PropertiesView::reloadProperties()
{
	group->properties()->clear();

	properties.reset(getProperties());
	settings.reset(getSettings());
	obs_property_t *prop = obs_properties_first(properties.get());

	if (!prop) {
		blog(LOG_INFO, "no properties");
		return;
	}

	do {
		GenericRow *row = createProperty(prop);
		if (row) {
			group->addRow(row);
		}
	} while (obs_property_next(&prop));

	// This might break things, but I want to try. The old properties view accesses
	// the source's internal settings pointer, which leads to settings being updated
	// even in cases where for example the update is deferred. Some sources (ab)use
	// this to do cursed stuff which arguably they shouldn't. Lets see what breaks.
	settings.reset(obs_data_create());

	emit propertiesRefreshed();
}

void PropertiesView::controlChanged(obs_property_t *property)
{
	emit settingsChanged(settings.get());
	if (obs_property_modified(property, settings.get())) {
		reloadProperties();
	}
}
} // namespace properties_view
