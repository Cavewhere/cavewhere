# The CaveWhere Qt Quick Controls style: one QML file per control.
#
# Files are globbed, so adding a control means adding one .qml file here and
# nothing else. Files named Style*.qml are shared building blocks, registered as
# internal types so they stay out of the QtQuick.Controls namespace.
file(GLOB caveWhereStyle_QML_FILES CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.qml")
file(GLOB caveWhereStyle_INTERNAL_QML_FILES CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "Style*.qml")
set_source_files_properties(${caveWhereStyle_INTERNAL_QML_FILES}
    PROPERTIES QT_QML_INTERNAL_TYPE TRUE)

qt_add_qml_module(CaveWhereStyle
    STATIC
    URI CaveWhereStyle
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml/
    IMPORT_PATH ${CMAKE_BINARY_DIR}
    QML_FILES ${caveWhereStyle_QML_FILES}
)

# The style files import cavewherelib for Theme; build its type information first.
add_dependencies(CaveWhereStyle cavewherelib)
