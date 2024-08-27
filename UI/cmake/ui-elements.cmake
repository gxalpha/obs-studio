if(NOT TARGET OBS::properties-view)
  add_subdirectory("${CMAKE_SOURCE_DIR}/shared/properties-view" "${CMAKE_BINARY_DIR}/shared/properties-view")
endif()

if(NOT TARGET OBS::extraqt)
  add_subdirectory("${CMAKE_SOURCE_DIR}/shared/qt/widgets" "${CMAKE_BINARY_DIR}/shared/qt/widgets")
endif()

target_link_libraries(obs-studio PRIVATE OBS::properties-view OBS::extraqt)

target_sources(
  obs-studio
  PRIVATE
    absolute-slider.cpp
    absolute-slider.hpp
    adv-audio-control.cpp
    adv-audio-control.hpp
    audio-encoders.cpp
    audio-encoders.hpp
    balance-slider.hpp
    basic-controls.cpp
    basic-controls.hpp
    clickable-label.hpp
    context-bar-controls.cpp
    context-bar-controls.hpp
    focus-list.cpp
    focus-list.hpp
    horizontal-scroll-area.cpp
    horizontal-scroll-area.hpp
    hotkey-edit.cpp
    hotkey-edit.hpp
    item-widget-helpers.cpp
    item-widget-helpers.hpp
    lineedit-autoresize.cpp
    lineedit-autoresize.hpp
    log-viewer.cpp
    log-viewer.hpp
    media-controls.cpp
    media-controls.hpp
    menu-button.cpp
    menu-button.hpp
    mute-checkbox.hpp
    noncheckable-button.hpp
    preview-controls.cpp
    preview-controls.hpp
    remote-text.cpp
    remote-text.hpp
    scene-tree.cpp
    scene-tree.hpp
    screenshot-obj.hpp
    source-label.cpp
    source-label.hpp
    source-tree.cpp
    source-tree.hpp
    undo-stack-obs.cpp
    undo-stack-obs.hpp
    url-push-button.cpp
    url-push-button.hpp
    visibility-item-widget.cpp
    visibility-item-widget.hpp
    volume-control.cpp
    volume-control.hpp
)
