#pragma once

#include <QWidget>
#include <Idian/Idian.hpp>
#include <obs.hpp>
#include <vertical-scroll-area.hpp>

namespace properties_view {

class PropertiesView : public VScrollArea {
	Q_OBJECT
public:
	using GetProperties = std::function<obs_properties_t *()>;
	using GetSettings = std::function<obs_data_t *()>;

	PropertiesView(GetProperties getProperties, GetSettings getSettings, QWidget *parent = nullptr);

	// TODO: These function names were copied from OBSPropertiesView, but aren't great.
	// They only exist to keep OBSBasicProperties.cpp changes minimal for now.
	bool DeferUpdate() const;
	void UpdateSettings();
	inline void ReloadProperties() { updateProperties(true); }

signals:
	void propertiesRefreshed();
	void settingsChanged(obs_data_t *);

private:
	const GetProperties getProperties;
	const GetSettings getSettings;
	std::unique_ptr<obs_properties_t, decltype(&obs_properties_destroy)> properties;
	std::unique_ptr<obs_data_t, decltype(&obs_data_release)> settings;
	std::unique_ptr<obs_data_t, decltype(&obs_data_release)> defaults;
	idian::Group *group;

	idian::Row *createPropertyBool(obs_property_t *prop);
	idian::Row *createPropertyInt(obs_property_t *prop);
	idian::Row *createPropertyDouble(obs_property_t *prop);
	idian::Row *createPropertyText(obs_property_t *prop);
	idian::Row *createPropertyList(obs_property_t *prop);
	idian::CollapsibleRow *createPropertyGroup(obs_property_t *prop);
	idian::Row *createPropertyNull(obs_property_t *prop);

	idian::GenericRow *createProperty(obs_property_t *prop);

	void controlChanged(obs_property_t *prop);

	void updateProperties(bool reset);
};
} // namespace properties_view
