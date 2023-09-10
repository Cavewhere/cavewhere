cmake_minimum_required(VERSION 3.5.0)

project(cavewhere VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Include the GenerateExportHeader module
include(GenerateExportHeader)
include(UpdateVersion.cmake)

# All subdirectories
add_subdirectory(QMath3d)
add_subdirectory(asyncfuture)
add_subdirectory(qt-qml-models)
add_subdirectory(dewalls)
add_subdirectory(survex)

# All packages we need to find
find_package(Qt5
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
    Pdf
    REQUIRED)

find_package(Protobuf REQUIRED)
find_package(libsquish REQUIRED)
find_package(Catch2 REQUIRED)

# Add dynamic library
add_library(cavewhere-lib SHARED)
target_link_libraries(cavewhere-lib
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
    Qt::Pdf
    QtQmlModels
    qtasyncfuture
    dewalls
    QMath3d
    libsquish::libsquish
    PRIVATE
    protobuf::protobuf
)

qt_add_resources(QT_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/fonts.qrc
)
target_sources(cavewhere-lib PRIVATE ${QT_RESOURCES})

# Generate the export header
generate_export_header(cavewhere-lib
    BASE_NAME CAVEWHERE_LIB
    EXPORT_MACRO_NAME CAVEWHERE_LIB_EXPORT
    EXPORT_FILE_NAME CaveWhereLibExport.h
)

# Generate cavewhereVersion.h
update_git_version(cavewhere-lib ${CMAKE_CURRENT_SOURCE_DIR}/src/cavewhereVersion.h.in)

target_include_directories(cavewhere-lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/versionInfo
)

# Collect source files cavewhereLib
file(GLOB cavewherelib_CPP_FILES "src/*.cpp" "src/utils/*.cpp")
file(GLOB cavewherelib_H_FILES "src/*.h" "src/utils/*.h")
target_sources(cavewhere-lib PRIVATE ${cavewherelib_CPP_FILES} ${cavewherelib_H_FILES})

# Protobuf files
file(GLOB protobuf_FILES "src/*.proto")
set(Protobuf_IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src)
message(STATUS "CMAKE_CURRENT_SOURCE_DIR/src is set to: ${CMAKE_CURRENT_SOURCE_DIR}/src ${PROTOBUF_IMPORT_DIRS}")
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${protobuf_FILES})
target_sources(cavewhere-lib PRIVATE ${protobuf_FILES} ${protobuf_FILES})

# Add the generated files to your target
set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS} PROPERTIES SKIP_AUTOMOC TRUE SKIP_AUTOUIC TRUE)
target_sources(cavewhere-lib PRIVATE ${PROTO_SRCS} ${PROTO_HDRS})

# Handle .ui files
qt5_wrap_ui(UI_HEADERS
    cwImportTreeDataDialog.ui
    cwTaskProgressDialog.ui
)

target_compile_definitions(cavewhere-lib PRIVATE "CAVEWHERE_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
target_compile_definitions(cavewhere-lib PRIVATE "WITH_PDF_SUPPORT")

target_sources(cavewhere-lib PRIVATE ${UI_HEADERS})

# Copy local dependancies to the build directory
add_custom_command(TARGET cavewhere-lib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:dewalls>
        $<TARGET_FILE_DIR:cavewhere-lib>
)
add_custom_command(TARGET cavewhere-lib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:QMath3d>
    $<TARGET_FILE_DIR:cavewhere-lib>
)

#### cavewhere-test ####
add_executable(cavewhere-test)

file(GLOB cavewhere_test_CPP_FILES "testcases/*.cpp")
file(GLOB cavewhere_test_H_FILES "testcases/*.h")
target_sources(cavewhere-test PRIVATE ${cavewhere_test_CPP_FILES} ${cavewhere_test_H_FILES})

file(GLOB cavewhere_test_qrc_FILES "testcases/*.qrc")
qt_add_resources(cavewhere_test_QT_RESOURCES ${cavewhere_test_qrc_FILES})
target_sources(cavewhere-test PRIVATE ${cavewhere_test_QT_RESOURCES})

target_link_libraries(cavewhere-test
    PRIVATE
    Qt::Test
    Catch2::Catch2
    cavewhere-lib
)

#### CaveWhere ####
add_executable(CaveWhere
    main.cpp
)

target_link_libraries(CaveWhere
    PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::Quick
    Qt::Sql
    Qt::Test
    cavewhere-lib
)

file(GLOB cavewhere_QML_FILES "qml/*.qml" "qml/*.js")
target_sources(CaveWhere PRIVATE ${cavewhere_QML_FILES})




