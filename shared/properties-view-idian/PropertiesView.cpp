#include "PropertiesView.hpp"

#include <double-slider.hpp>
#include <Idian/Idian.hpp>
#include <obs.hpp>
#include <slider-ignorewheel.hpp>
#include <util/dstr.hpp>

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSlider>

#include "moc_PropertiesView.cpp"

using namespace idian;

namespace properties_view {
PropertiesView::PropertiesView(GetProperties getProperties, GetSettings getSettings, void *obsObject, QWidget *parent)
	: VScrollArea(parent),
	  getProperties(getProperties),
	  getSettings(getSettings),
	  obsObject(obsObject),
	  properties(nullptr, obs_properties_destroy),
	  settings(nullptr, obs_data_release),
	  originalSettings(nullptr, obs_data_release),
	  group(new Group(this))
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

	QMetaObject::invokeMethod(this, [this]() { updateProperties(true); }, Qt::QueuedConnection);
}

GenericRow *PropertiesView::createProperty(obs_property_t *property)
{
	if (!obs_property_visible(property)) {
		return nullptr;
	}

	GenericRow *row = nullptr;
	switch (obs_property_get_type(property)) {
	case OBS_PROPERTY_INVALID:
		row = createPropertyInvalid(property, "Property has type OBS_PROPERTY_INVALID.");
		break;
	case OBS_PROPERTY_BOOL:
		row = createPropertyBool(property);
		break;
	case OBS_PROPERTY_INT:
		row = createPropertyInt(property);
		break;
	case OBS_PROPERTY_FLOAT:
		row = createPropertyDouble(property);
		break;
	case OBS_PROPERTY_TEXT:
		row = createPropertyText(property);
		break;
	case OBS_PROPERTY_PATH:
		row = createPropertyPath(property);
		break;
	case OBS_PROPERTY_LIST:
		row = createPropertyList(property);
		break;
	case OBS_PROPERTY_COLOR:
		//row = createPropertyColor(property); // TODO: Implement.
		row = createPropertyInvalid(property, "Property type 'OBS_PROPERTY_COLOR' is not yet implemented.");
		break;
	case OBS_PROPERTY_BUTTON:
		row = createPropertyButton(property);
		break;
	case OBS_PROPERTY_FONT:
		//row = createPropertyFont(property); // TODO: Implement.
		row = createPropertyInvalid(property, "Property type 'OBS_PROPERTY_FONT' is not yet implemented.");
		break;
	case OBS_PROPERTY_EDITABLE_LIST:
		//row = createPropertyEditableList(property); // TODO: Implement.
		row = createPropertyInvalid(property,
					    "Property type 'OBS_PROPERTY_EDITABLE_LIST' is not yet implemented.");
		break;
	case OBS_PROPERTY_FRAME_RATE:
		//row = createPropertyFrameRate(property); // TODO: Implement.
		row = createPropertyInvalid(property,
					    "Property type 'OBS_PROPERTY_FRAME_RATE' is not yet implemented.");
		break;
	case OBS_PROPERTY_GROUP:
		row = createPropertyGroup(property);
		break;
	case OBS_PROPERTY_COLOR_ALPHA:
		//row = createPropertyColorAlpha(property); // TODO: Implement.
		row = createPropertyInvalid(property,
					    "Property type 'OBS_PROPERTY_COLOR_ALPHA' is not yet implemented.");
		break;
	default:
		DStr str;
		dstr_printf(str, "Property type of index '%d' is unknown.", obs_property_get_type(property));
		row = createPropertyInvalid(property, str->array);
		break;
	}

	row->setTitle(obs_property_description(property));
	const char *long_desc = obs_property_long_description(property);
	if (long_desc) {
		row->setDescription(long_desc);
	}

	row->setEnabled(obs_property_enabled(property));

	return row;
}

namespace {
// TODO: Yeah...
void ApplyOBSDataWithDefaults(OBSData target, OBSData applyData)
{
	OBSDataAutoRelease defaults = obs_data_get_defaults(applyData);
	obs_data_item_t *item = obs_data_first(defaults);
	while (item) {
		enum obs_data_type type = obs_data_item_gettype(item);
		const char *name = obs_data_item_get_name(item);
		switch (type) {
		case OBS_DATA_NULL:
			break;
		case OBS_DATA_STRING:
			obs_data_set_default_string(target, name, obs_data_item_get_string(item));
			break;
		case OBS_DATA_NUMBER: {
			enum obs_data_number_type numtype = obs_data_item_numtype(item);
			switch (numtype) {
			case OBS_DATA_NUM_INVALID:
				break;
			case OBS_DATA_NUM_INT:
				obs_data_set_default_int(target, name, obs_data_item_get_int(item));
				break;
			case OBS_DATA_NUM_DOUBLE:
				obs_data_set_default_double(target, name, obs_data_item_get_double(item));
				break;
			}
			break;
		}
		case OBS_DATA_BOOLEAN:
			obs_data_set_default_bool(target, name, obs_data_item_get_bool(item));
			break;
		case OBS_DATA_OBJECT: {
			// TODO: Does this even work?
			OBSDataAutoRelease obj = obs_data_item_get_default_obj(item);
			obs_data_set_default_obj(target, name, obj);
			break;
		}
		case OBS_DATA_ARRAY: {
			// TODO: Check if this works as well.
			OBSDataArrayAutoRelease arr = obs_data_item_get_array(item);
			obs_data_set_default_array(target, name, arr);
			break;
		}
		}

		obs_data_item_next(&item);
	}

	obs_data_apply(target, applyData);
}

// :(
std::unordered_set<std::string> CompareOBSData(OBSData first, OBSData second)
{
	std::unordered_set<std::string> changes;
	for (auto [left, right] : {std::make_pair(first, second), std::make_pair(second, first)}) {
		obs_data_item_t *item = obs_data_first(left.Get());
		while (item) {
			enum obs_data_type type = obs_data_item_gettype(item);
			auto name = obs_data_item_get_name(item);
			bool different = false;
			switch (type) {
			case OBS_DATA_NULL:
				break;
			case OBS_DATA_STRING:
				different =
					(strcmp(obs_data_item_get_string(item), obs_data_get_string(right, name)) != 0);

				break;
			case OBS_DATA_NUMBER: {
				enum obs_data_number_type numtype = obs_data_item_numtype(item);
				switch (numtype) {
				case OBS_DATA_NUM_INVALID:
					break;
				case OBS_DATA_NUM_INT:
					different = (obs_data_item_get_int(item) != obs_data_get_int(right, name));
					break;
				case OBS_DATA_NUM_DOUBLE:
					different = (obs_data_item_get_double(item) != obs_data_get_int(right, name));
					break;
				}
				break;
			}
			case OBS_DATA_BOOLEAN:
				different = (obs_data_item_get_bool(item) != obs_data_get_bool(right, name));
				break;
			case OBS_DATA_OBJECT: {
				OBSDataAutoRelease leftObj = obs_data_item_get_obj(item);
				OBSDataAutoRelease rightObj = obs_data_get_obj(right, name);
				different = !CompareOBSData(leftObj.Get(), rightObj.Get()).empty();
				break;
			}
			case OBS_DATA_ARRAY: {
				// TODO: I really can't be bothered right now. All this needs something better anyways.
				different = true;
				break;
			}
			}

			if (different) {
				changes.emplace(name);
			}

			obs_data_item_next(&item);
		}
	}
	return changes;
}
} // namespace

void PropertiesView::updateProperties(bool reset)
{
	group->properties()->clear();

	if (reset) {
		properties.reset(getProperties());
		settings.reset(getSettings());
		if (!originalSettings) {
			originalSettings.reset(obs_data_create());
			ApplyOBSDataWithDefaults(originalSettings.get(), settings.get());
		}
	}

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
	// this to do cursed stuff which arguably they shouldn't.
	// We can avoid all that by creating a new obs_data_t object instead of using the
	// original one owned by the source. Let's see what breaks.
	OBSData copy = obs_data_create();
	ApplyOBSDataWithDefaults(copy, settings.get());
	settings.reset(copy);

	emit propertiesRefreshed();
}

void PropertiesView::resetPropertiesToDefaults()
{
	OBSDataAutoRelease oldSettings = obs_data_create();
	ApplyOBSDataWithDefaults(oldSettings.Get(), settings.get());
	obs_data_clear(settings.get());

	auto propertiesThatNeedModifiedCallback = CompareOBSData(oldSettings.Get(), settings.get());
	obs_property_t *property = obs_properties_first(properties.get());
	while (property) {
		const char *name = obs_property_name(property);
		// TODO: .contains() is C++20...
		if (propertiesThatNeedModifiedCallback.count(name) != 0) {
			obs_property_modified(property, settings.get());
		}
		obs_property_next(&property);
	}

	modifiedProperties = CompareOBSData(originalSettings.get(), settings.get());

	updateProperties(false);
}

bool PropertiesView::isDeferredUpdate() const
{
	const uint32_t flags = obs_properties_get_flags(properties.get());
	return (flags & OBS_PROPERTIES_DEFER_UPDATE) != 0;
}

void PropertiesView::publishSettings()
{
	emit settingsChanged(settings.get());
}

} // namespace properties_view
