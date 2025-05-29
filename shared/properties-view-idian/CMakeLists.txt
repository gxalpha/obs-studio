cmake_minimum_required(VERSION 3.28...3.30)

find_package(Qt6 REQUIRED Core Widgets)

if(NOT TARGET OBS::idian)
  add_subdirectory("${CMAKE_SOURCE_DIR}/shared/qt/idian" "${CMAKE_BINARY_DIR}/shared/qt/idian")
endif()

add_library(properties-view-idian INTERFACE)
add_library(OBS::properties-view-idian ALIAS properties-view-idian)

target_sources(properties-view-idian INTERFACE PropertiesView.cpp PropertiesView.hpp)
target_include_directories(properties-view-idian INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(
  properties-view-idian
  INTERFACE OBS::frontend-api OBS::libobs OBS::idian Qt::Core Qt::Widgets
)
