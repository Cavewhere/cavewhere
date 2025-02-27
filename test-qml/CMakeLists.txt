#Putting this module in a subdirectory prevents a cmake qml module warning
qt_add_qml_module(cavewhere_simplemodule
    URI cw.Simple
    VERSION 1.0
    RESOURCE_PREFIX /qml
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/cw/Simple
    DEPENDENCIES
        QtQuick
        QtQuick.Controls
    QML_FILES
        Simple.qml
)

#Ignore moc for qt 6.7?
set_source_files_properties(cavewhere_simplemoduleplugin_cw_SimplePlugin_in.cpp PROPERTIES SKIP_AUTOMOC ON)
