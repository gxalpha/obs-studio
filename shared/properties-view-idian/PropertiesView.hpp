#pragma once

#include <QWidget>
#include <Idian.hpp>
#include <obs.hpp>
#include <vertical-scroll-area.hpp>

namespace properties_view {

class PropertiesView : public VScrollArea {
	Q_OBJECT
public:
	using GetProperties = std::function<obs_properties_t *()>;
	using GetSettings = std::function<obs_data_t *()>;

	PropertiesView(GetProperties getProperties, GetSettings getSettings, QWidget *parent = nullptr);

signals:
	void propertiesRefreshed();
	void settingsChanged(obs_data_t *);

private:
	const GetProperties getProperties;
	const GetSettings getSettings;
	std::unique_ptr<obs_properties_t, decltype(&obs_properties_destroy)> properties;
	std::unique_ptr<obs_data_t, decltype(&obs_data_release)> settings;
	idian::Group *group;

	void reloadProperties();

	idian::Row *createPropertyBool(obs_property_t *prop);
	idian::Row *createPropertyInt(obs_property_t *prop);
	idian::Row *createPropertyDouble(obs_property_t *prop);
	idian::Row *createPropertyText(obs_property_t *prop);
	idian::Row *createPropertyList(obs_property_t *prop);
	idian::CollapsibleRow *createPropertyGroup(obs_property_t *prop);
	idian::Row *createPropertyNull(obs_property_t *prop);

	idian::GenericRow *createProperty(obs_property_t *prop);

	void controlChanged(obs_property_t *prop);
};

} // namespace properties_view

// For simpler usage.
// TODO: Think about whether this is a good idea, or whether the class shouldn't be in the namespace at all
// or whether it should be, but the using statement should be removed.
using properties_view::PropertiesView;
