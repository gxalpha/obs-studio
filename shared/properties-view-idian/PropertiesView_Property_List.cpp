#include "PropertiesView.hpp"

#include <Idian/ComboBox.hpp>
#include <Idian/Row.hpp>

using idian::Row;
using idian::ComboBox;
namespace properties_view {

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

Row *PropertiesView::createPropertyList(obs_property_t *property)
{
	const char *name = obs_property_name(property);
	enum obs_combo_type type = obs_property_list_type(property);
	enum obs_combo_format format = obs_property_list_format(property);

	if (type == OBS_COMBO_TYPE_EDITABLE) {
		// TODO: add.
		return createPropertyInvalid(property, "Subtype 'OBS_COMBO_TYPE_EDITABLE' is not yet implemented.");
	}

	if (type == OBS_COMBO_TYPE_RADIO) {
		// TODO: add.
		return createPropertyInvalid(property, "Subtype 'OBS_COMBO_TYPE_RADIO' is not yet implemented.");
	}

	Row *row = new Row();
	ComboBox *comboBox = new ComboBox(row);
	size_t count = obs_property_list_item_count(property);
	for (size_t i = 0; i < count; ++i) {
		const char *name = obs_property_list_item_name(property, i);
		comboBox->addItem(name, propertyListToQVariant(property, i));
		// TODO: Disabled items.
	}
	comboBox->setCurrentIndex(comboBox->findData(from_obs_data(settings.get(), name, format)));
	connect(comboBox, &QComboBox::currentIndexChanged, this, [this, comboBox, property]() {
		QVariant data = comboBox->currentData();
		// TODO: This was simply copied from properties-view to just get something working and could likely be much nicer.
		const char *name = obs_property_name(property);
		switch (obs_property_list_format(property)) {
		case OBS_COMBO_FORMAT_INVALID:
			return;
		case OBS_COMBO_FORMAT_INT: {
			auto value = data.value<long long>();
			obs_data_set_int(settings.get(), name, value);
			controlChanged(property, static_cast<int>(value));
			break;
		}
		case OBS_COMBO_FORMAT_FLOAT: {
			auto value = data.value<double>();
			obs_data_set_double(settings.get(), name, value);
			controlChanged(property, value);
			break;
		}
		case OBS_COMBO_FORMAT_STRING: {
			auto value = data.toByteArray().constData();
			obs_data_set_string(settings.get(), name, value);
			controlChanged(property, value);
			break;
		}
		case OBS_COMBO_FORMAT_BOOL: {
			auto value = data.value<bool>();
			obs_data_set_bool(settings.get(), name, value);
			controlChanged(property, value);
			break;
		}
		}
	});
	row->setSuffix(comboBox);
	return row;
}
} // namespace properties_view
