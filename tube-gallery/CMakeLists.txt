#### cavewhere-tube-gallery ####
# A developer tool for checking passage tube generation by eye: renders the
# cwPassageTubeCases catalog with the real builder and render objects. It ships
# with the build tree, not the installer.

qt_add_executable(cavewhere-tube-gallery
    MANUAL_FINALIZATION
    main.cpp
)

# Next to cavewherelib/ in the build root, so the default QML import path finds it.
set_target_properties(cavewhere-tube-gallery PROPERTIES
    AUTOMOC ON
    MACOSX_BUNDLE FALSE
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

qt_add_qml_module(cavewhere-tube-gallery
    URI cw.TubeGallery
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml/
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/cw/TubeGallery
    DEPENDENCIES
        QtQuick
        cavewherelib
    SOURCES
        cwPassageTubeGalleryFilterModel.h
        cwPassageTubeGalleryFilterModel.cpp
        cwTubeGalleryGrabber.h
        cwTubeGalleryGrabber.cpp
    QML_FILES
        qml/TubeGalleryWindow.qml
        qml/TubeSandboxPanel.qml
)

target_link_libraries(cavewhere-tube-gallery
    PRIVATE
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2
    cavewherelib
)

if(TARGET CaveWhereStyleplugin)
    target_link_libraries(cavewhere-tube-gallery PRIVATE CaveWhereStyleplugin)
endif()

qt_import_qml_plugins(cavewhere-tube-gallery)
qt_finalize_executable(cavewhere-tube-gallery)
