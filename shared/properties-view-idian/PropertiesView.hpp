#pragma once

#include <obs.hpp>
#include <vertical-scroll-area.hpp>

#include <memory>
#include <functional>

namespace idian {
class Group;
class Row;
class CollapsibleRow;
class GenericRow;
} // namespace idian

namespace properties_view {
template<typename T, typename... Types> constexpr bool is_one_of_v = (std::is_same_v<T, Types> || ...);

class PropertiesView : public VScrollArea {
	Q_OBJECT
public:
	using GetProperties = std::function<obs_properties_t *()>;
	using GetSettings = std::function<obs_data_t *()>;

	// TODO: Replace with concepts once we have C++20.
	template<typename T, typename = std::enable_if_t<is_one_of_v<T, obs_canvas_t *, obs_source_t *, obs_output_t *,
								     obs_encoder_t *, obs_service_t *, std::nullptr_t>>>
	PropertiesView(GetProperties getProperties, GetSettings getSettings, T obsObject, QWidget *parent = nullptr)
		: PropertiesView(getProperties, getSettings, static_cast<void *>(obsObject), parent)
	{
	}

	bool isDeferredUpdate() const;
	void publishSettings();
	void updateProperties(bool reset);

	bool propertiesHaveChanges() const { return !modifiedProperties.empty(); }
	void resetPropertiesToDefaults();

signals:
	void propertiesRefreshed();
	void settingsChanged(obs_data_t *);

private:
	PropertiesView(GetProperties getProperties, GetSettings getSettings, void *obsObject,
		       QWidget *parent = nullptr);

	const GetProperties getProperties;
	const GetSettings getSettings;
	// TODO: Remove obsObject (and everything related to it) once v1 of obs_properties_add_button is gone.
	void *obsObject;
	std::unique_ptr<obs_properties_t, decltype(&obs_properties_destroy)> properties;
	std::unique_ptr<obs_data_t, decltype(&obs_data_release)> settings;
	std::unique_ptr<obs_data_t, decltype(&obs_data_release)> originalSettings;
	std::unordered_set<std::string> modifiedProperties;
	idian::Group *group;

	idian::Row *createPropertyInvalid(obs_property_t *prop, const char *reason);
	idian::Row *createPropertyBool(obs_property_t *prop);
	idian::Row *createPropertyInt(obs_property_t *prop);
	idian::Row *createPropertyDouble(obs_property_t *prop);
	idian::Row *createPropertyText(obs_property_t *prop);
	idian::Row *createPropertyPath(obs_property_t *prop);
	idian::Row *createPropertyList(obs_property_t *prop);
	idian::Row *createPropertyColor(obs_property_t *prop);
	idian::Row *createPropertyButton(obs_property_t *prop);
	idian::Row *createPropertyFont(obs_property_t *prop);
	idian::Row *createPropertyEditableList(obs_property_t *prop);
	idian::Row *createPropertyFrameRate(obs_property_t *prop);
	idian::CollapsibleRow *createPropertyGroup(obs_property_t *prop);
	idian::Row *createPropertyColorAlpha(obs_property_t *prop);

	idian::GenericRow *createProperty(obs_property_t *prop);

	template<typename T, bool useCurrentSettings = true> T getPropertyValue(obs_property_t *property)
	{
		const char *name = obs_property_name(property);
		OBSData data;
		if constexpr (useCurrentSettings) {
			data = settings.get();
		} else {
			data = originalSettings.get();
		}
		if constexpr (std::is_same_v<T, bool>) {
			return obs_data_get_bool(data, name);
		} else if constexpr (std::is_same_v<T, int>) {
			return obs_data_get_int(data, name);
		} else if constexpr (std::is_same_v<T, double>) {
			return obs_data_get_double(data, name);
		} else if constexpr (std::is_same_v<T, const char *>) {
			return obs_data_get_string(data, name);
		} else {
			// TODO: Replace with concepts once C++20, like https://lists.isocpp.org/std-proposals/2021/06/2727.php
			static_assert(false, "Type not implemented");
		}
	}

	template<typename ValueType> void controlChanged(obs_property_t *property, ValueType newValue)
	{
		ValueType originalValue = getPropertyValue<ValueType, false>(property);

		bool changed = false;
		if constexpr (is_one_of_v<ValueType, bool, int, double>) {
			changed = (originalValue != newValue);
		} else if constexpr (std::is_same_v<ValueType, const char *>) {
			changed = (strcmp(originalValue, newValue) != 0);
		} else {
			// TODO: Replace with concepts once C++20, like https://lists.isocpp.org/std-proposals/2021/06/2727.php
			static_assert(false, "Type not implemented");
		}

		if (changed) {
			modifiedProperties.emplace(obs_property_name(property));
		} else {
			modifiedProperties.erase(obs_property_name(property));
		}

		if (!isDeferredUpdate()) {
			emit settingsChanged(settings.get());
		}

		if (obs_property_modified(property, settings.get())) {
			updateProperties(false);
		}
	}
};
} // namespace properties_view
