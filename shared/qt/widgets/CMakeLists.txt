cmake_minimum_required(VERSION 3.22...3.25)

find_package(Qt6 REQUIRED Core Widgets)

add_library(extraqt STATIC)
add_library(OBS::extraqt ALIAS extraqt)

target_sources(
  extraqt
  PRIVATE extraqt/plain-text-edit.cpp extraqt/slider-ignorewheel.cpp extraqt/vertical-scroll-area.cpp
)
target_sources(
  extraqt
  PUBLIC extraqt/icon-label.hpp extraqt/plain-text-edit.hpp extraqt/slider-ignorewheel.hpp extraqt/vertical-scroll-area.hpp
)
target_include_directories(extraqt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(extraqt PROPERTIES AUTOMOC ON)

target_link_libraries(extraqt PUBLIC Qt::Core Qt::Widgets)
