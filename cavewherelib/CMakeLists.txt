cmake_minimum_required(VERSION 3.23)

include(${CMAKE_CURRENT_SOURCE_DIR}/UpdateVersion.cmake)

#### cavewherelib ######
# All packages we need to find
find_package(Protobuf REQUIRED)
find_package(Catch2 REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Qt6
    COMPONENTS
    Core
    Gui
    Widgets
    Quick
    Sql
    OpenGL
    Xml
    Concurrent
    Svg
    Test
    ShaderTools
    REQUIRED)

qt_add_library(cavewherelib SHARED)

#RESOURCE_PREFIX defaults to /qt/qml/ which is the default import path of the QML engine
# qmake_policy(SET QTP0001 NEW)

set_target_properties(cavewherelib PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

target_link_libraries(cavewherelib
    PUBLIC
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::Quick
    Qt::Sql
    Qt::OpenGL
    Qt::Xml
    Qt::Concurrent
    Qt::Svg
    Qt::GuiPrivate
    asyncfuture
    dewalls
    QMath3d
    MarkScope
    PRIVATE
    protobuf::protobuf
    SQLite::SQLite3
)

# Conditionally find the package and include directories
if(WITH_PDF)
    find_package(Qt6 COMPONENTS Pdf REQUIRED)
    target_link_libraries(cavewherelib PRIVATE Qt::Pdf)
    target_compile_definitions(cavewherelib PRIVATE "WITH_PDF_SUPPORT")
endif()

qt_add_resources(QT_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/fonts.qrc
)
target_sources(cavewherelib PRIVATE ${QT_RESOURCES})

# Generate the export header
generate_export_header(cavewherelib
    BASE_NAME CAVEWHERE_LIB
    EXPORT_MACRO_NAME CAVEWHERE_LIB_EXPORT
    EXPORT_FILE_NAME CaveWhereLibExport.h
)

# Generate cavewhereVersion.h
update_git_version(cavewherelib ${CMAKE_CURRENT_SOURCE_DIR}/src/cavewhereVersion.h.in)

target_include_directories(cavewherelib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/versionInfo
)

# Collect source files cavewhereLib
file(GLOB cavewherelib_CPP_FILES "src/*.cpp" "src/utils/*.cpp")
file(GLOB cavewherelib_H_FILES "src/*.h" "src/utils/*.h")

# file(GLOB cavewhere_QML_FILES "qml/*.qml" "qml/*.js")
# target_sources(CaveWhere PRIVATE ${cavewhere_QML_FILES})

# Make the file paths relative to the project directory
set(cavewhere_RELATIVE_CPP_FILES "")
foreach(file ${cavewhere_CPP_FILES})
    file(RELATIVE_PATH relFile "${CMAKE_CURRENT_SOURCE_DIR}" "${file}")
    list(APPEND cavewhere_RELATIVE_CPP_FILES "${relFile}")
endforeach()

set(cavewhere_RELATIVE_H_FILES "")
foreach(file ${cavewhere_H_FILES})
    file(RELATIVE_PATH relFile "${CMAKE_CURRENT_SOURCE_DIR}" "${file}")
    list(APPEND cavewhere_RELATIVE_H_FILES "${relFile}")
endforeach()

# Get all QML and JS files
file(GLOB cavewhere_QML_FILES "*.qml" "*.js")

# Define the list of QML singletons
set(qml_singletons
    "UnitDefaults.qml"
    "GlobalShadowTextInput.qml"
    "RootPopupItem.qml"
)

# Remove the singleton files from the regular QML file list
foreach(singleton ${qml_singletons})
    list(REMOVE_ITEM cavewhere_QML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${singleton}")
endforeach()

# Make the remaining QML files relative to the project directory
set(cavewhere_RELATIVE_QML_FILES "")
foreach(file ${cavewhere_QML_FILES})
    file(RELATIVE_PATH relFile "${CMAKE_CURRENT_SOURCE_DIR}" "${file}")
    message(STATUS "Adding QML file: ${relFile}")
    list(APPEND cavewhere_RELATIVE_QML_FILES "${relFile}")
endforeach()

# Set the singleton QML files as singletons
set_source_files_properties(${qml_singletons} PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Add QML module with the regular QML files and the singleton files
# if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
#     # macOS
#     set(cavewherelib_ouput_dir "${CMAKE_BINARY_DIR}/CaveWhere.app/Contents/MacOS/cavewherelib")
# else()
#     # Other systems (e.g., Linux, Windows)
#     set(cavewherelib_ouput_dir "${CMAKE_BINARY_DIR}/cavewherelib")
# endif()

qt_add_qml_module(cavewherelib
    URI cavewherelib
    VERSION 1.0
    RESOURCE_PREFIX /cavewherelib
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/cavewherelib
    NO_CACHEGEN #Make sure this is comment out for release mode
    DEPENDENCIES
        QtQuick
    SOURCES
        ${cavewhere_RELATIVE_CPP_FILES}
        ${cavewhere_RELATIVE_H_FILES}
    QML_FILES
        ${cavewhere_RELATIVE_QML_FILES}
        ${qml_singletons}
)

target_sources(cavewherelib PRIVATE ${cavewherelib_CPP_FILES} ${cavewherelib_H_FILES})

# Handle shaders
qt_add_shaders(cavewherelib "cavewherelib_shaders"
    PREFIX "/"
    FILES
        "shaders/grid.frag"
        "shaders/grid.vert"
        "shaders/LinePlot.frag"
        "shaders/LinePlot.vert"
        "shaders/scrap.vert"
        "shaders/scrap.frag"
        "shaders/radial_gradient.vert"
        "shaders/radial_gradient.frag"
        "shaders/compass/compass.vert"
        "shaders/compass/compass.frag"
        "shaders/toColor.frag"
    )

#This is a hack to find the conan protoc and not the system protoc
#If homebrew is installed on macos conan finds the homebrew version even though
#protoc is in the build.requirements. https://github.com/conan-io/conan-center-index/issues/21737
# Get the current IMPORTED_LOCATION of protobuf::protoc
get_target_property(CURRENT_PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION)

# Check if the path contains ".conan2"
if (CURRENT_PROTOC_EXECUTABLE MATCHES "\\.conan2")
    message(STATUS "Using Conan's protoc: ${CURRENT_PROTOC_EXECUTABLE}")
else()
    message(STATUS "System protoc detected. Applying Conan protoc hack.")

    # Define the Conan protoc path
    set(CONAN_PROTOC "${Protobuf_INCLUDE_DIR}/../bin/protoc${CMAKE_EXECUTABLE_SUFFIX}")

    # Force the use of Conan's protoc
    set(Protobuf_PROTOC_EXECUTABLE ${CONAN_PROTOC} CACHE FILEPATH "The protoc compiler" FORCE)
    set_property(TARGET protobuf::protoc PROPERTY IMPORTED_LOCATION ${Protobuf_PROTOC_EXECUTABLE})

    message(STATUS "Overriding protoc with: ${Protobuf_PROTOC_EXECUTABLE}")
endif()

# Protobuf files
file(GLOB protobuf_FILES "src/*.proto")
set(Protobuf_IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${protobuf_FILES})
target_sources(cavewherelib PRIVATE ${protobuf_FILES} ${protobuf_FILES})

# Add the generated files to your target
set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS} PROPERTIES SKIP_AUTOMOC TRUE SKIP_AUTOUIC TRUE)
target_sources(cavewherelib PRIVATE ${PROTO_SRCS} ${PROTO_HDRS})

# Handle .ui files
qt_wrap_ui(UI_HEADERS
    cwImportTreeDataDialog.ui
    cwTaskProgressDialog.ui
)


target_compile_definitions(cavewherelib PRIVATE "CAVEWHERE_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

target_sources(cavewherelib PRIVATE ${UI_HEADERS})

# Copy local dependancies to the build directory
# add_custom_command(TARGET cavewherelib POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         $<TARGET_FILE:dewalls>
#         $<TARGET_FILE_DIR:cavewherelib>
# )
# add_custom_command(TARGET cavewherelib POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     $<TARGET_FILE:QMath3d>
#     $<TARGET_FILE_DIR:cavewherelib>
# )
