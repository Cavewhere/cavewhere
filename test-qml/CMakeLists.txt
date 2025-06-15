#Putting this module in a subdirectory prevents a cmake qml module warning
# qt_add_library(cavewhere_simplemodule STATIC)
# set_target_properties(cavewhere_simplemodule PROPERTIES
#     AUTOMOC ON
#     AUTORCC ON
#     AUTOUIC ON
# )

qt_add_qml_module(cavewhere_simplemodule
    URI cw.Simple
    VERSION 1.0
    STATIC
    RESOURCE_PREFIX /qml
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/cw/Simple
    DEPENDENCIES
        QtQuick
        QtQuick.Controls
    QML_FILES
        Simple.qml
)
