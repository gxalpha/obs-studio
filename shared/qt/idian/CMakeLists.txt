cmake_minimum_required(VERSION 3.28...3.30)

find_package(Qt6 REQUIRED Core Widgets Svg)

add_library(idian STATIC)
add_library(OBS::idian ALIAS idian)

target_sources(
  idian
  PRIVATE
    components/CheckBox.cpp
    components/CheckBox.hpp
    components/ComboBox.cpp
    components/ComboBox.hpp
    components/DoubleSpinBox.cpp
    components/DoubleSpinBox.hpp
    components/SpinBox.cpp
    components/SpinBox.hpp
    components/ToggleSwitch.cpp
    components/ToggleSwitch.hpp
    widgets/Group.cpp
    widgets/Group.hpp
    widgets/PropertiesList.cpp
    widgets/PropertiesList.hpp
    widgets/Row.cpp
    widgets/Row.hpp
    Utils.hpp
)
target_sources(idian PUBLIC include/Idian.hpp)

set_target_properties(idian PROPERTIES AUTOMOC ON FOLDER "frontend")

target_include_directories(idian PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(idian PUBLIC Qt::Core Qt::Widgets Qt::Svg)
