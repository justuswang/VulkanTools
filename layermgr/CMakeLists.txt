
cmake_minimum_required(VERSION 3.1)
project(layermgr)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if (WIN32)
    add_custom_target(copy_qt5_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt5::Core>"    "$<TARGET_FILE_DIR:vklayermanager>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt5::Gui>"     "$<TARGET_FILE_DIR:vklayermanager>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt5::Widgets>" "$<TARGET_FILE_DIR:vklayermanager>"
        VERBATIM)
endif()

set(LAYERMGR_SRCS
    active_layers_widget.cpp
    layer_locations_widget.cpp
    layer_manager.cpp
    layer_manifest.cpp
    layer_settings_widget.cpp
    override_settings.cpp
    data.qrc
)

add_executable(vklayermanager ${LAYERMGR_SRCS})
target_link_libraries(vklayermanager Qt5::Core Qt5::Gui Qt5::Widgets)
