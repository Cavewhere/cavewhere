#Putting this module in a subdirectory prevents a cmake qml module warning
qt_add_qml_module(cavewhere_simplemodule
    URI cw.Simple
    VERSION 1.0
    RESOURCE_PREFIX /qml
    OUTPUT_DIRECTORY cw/Simple
    DEPENDENCIES
        QtQuick
        QtQuick.Controls
    QML_FILES
        Simple.qml
)
