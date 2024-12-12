cmake_minimum_required(VERSION 3.21.1)

project(cavewhere VERSION 1.0.0 LANGUAGES CXX)

#Add address sanitizer for clang build only
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_options($<$<CONFIG:Debug>:-fsanitize=address>)
    add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)
endif()

option(WITH_PDF "Build with PDF support" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON CACHE BOOL "Generate QMLLS INI file")

# Include the GenerateExportHeader module
include(GenerateExportHeader)

# All subdirectories
add_subdirectory(QMath3d)
add_subdirectory(asyncfuture)
add_subdirectory(qt-qml-models)
add_subdirectory(dewalls)
add_subdirectory(cavewherelib)
add_subdirectory(qml-test-recorder)

#Allows QtCreator to find qml import directories
#https://doc.qt.io/qtcreator/creator-qml-modules-with-plugins.html#importing-qml-modules
# message(STATUS "Import directory: ${CMAKE_BINARY_DIR}/qml-test-recorder")
# set(QML_IMPORT_PATH ${CMAKE_BINARY_DIR} CACHE STRING "" FORCE)

set(SURVEX_CONAN OFF CACHE BOOL "Use survex Conan to manage dependencies" FORCE)

add_subdirectory(survex)

# All packages we need to find
find_package(Protobuf REQUIRED)
find_package(libsquish REQUIRED)
find_package(Catch2 REQUIRED)
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
    QuickTest
    REQUIRED)

set(CMAKE_AUTOMOC ON)

#### cavewhere-testlib ####
qt_add_qml_module(cavewhere-testlib
    URI CaveWhere.TestLib
    VERSION 1.0
    RESOURCE_PREFIX /qml
    OUTPUT_DIRECTORY CaveWhere/TestLib
    DEPENDENCIES
        QtQuick
        QtQuick.Controls
    SOURCES
        testlib/TestHelper.cpp
        testlib/TestHelper.h
    #QML_FILES
)

# Generate the export header
generate_export_header(cavewhere-testlib
    BASE_NAME CAVEWHERE_TESTLIB
    EXPORT_MACRO_NAME CAVEWHERE_TESTLIB_EXPORT
    EXPORT_FILE_NAME CaveWhereTestLibExport.h
)

target_link_libraries(cavewhere-testlib
    PRIVATE
    Qt::Test
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::Quick
    Qt::Qml
    Catch2::Catch2
    cavewherelib
)

file(GLOB cavewhere_testlib_qrc_FILES "testcases/*.qrc")
qt_add_resources(cavewhere_testlib_QT_RESOURCES ${cavewhere_testlib_qrc_FILES})
target_sources(cavewhere-testlib PRIVATE ${cavewhere_testlib_QT_RESOURCES})

target_include_directories(cavewhere-testlib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/testlib
    ${CMAKE_CURRENT_BINARY_DIR}
)

#### cavewhere-test ####
qt_add_executable(cavewhere-test)

set_target_properties(cavewhere-test PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

file(GLOB cavewhere_test_CPP_FILES "testcases/*.cpp")
file(GLOB cavewhere_test_H_FILES "testcases/*.h")
target_sources(cavewhere-test PRIVATE ${cavewhere_test_CPP_FILES} ${cavewhere_test_H_FILES})

target_link_libraries(cavewhere-test
    PRIVATE
    Qt::Test
    Catch2::Catch2
    cavewherelib
    cavewhere-testlib
)

#### Cavewhere qml tests ####
# Add the executable
qt_add_executable(cavewhere-qml-test)

set_target_properties(cavewhere-qml-test PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)


file(GLOB cavewhere_qml_test_CPP_FILES "test-qml/*.cpp")
file(GLOB cavewhere_qml_test_H_FILES "test-qml/*.h")
target_sources(cavewhere-qml-test PRIVATE ${cavewhere_qml_test_CPP_FILES} ${cavewhere_qml_test_CPP_FILES})

qt_add_qml_module(cavewhere_simplemodule
    URI CaveWhere.Simple
    VERSION 1.0
    RESOURCE_PREFIX /qml
    OUTPUT_DIRECTORY CaveWhere/Simple
    DEPENDENCIES
        QtQuick
        QtQuick.Controls
    QML_FILES
        test-qml/Simple.qml
)

qt_add_resources(cavewhere-qml-test "test-qmls"
    PREFIX "/test-qmls"
    FILES
    test-qml/tst_Simple.qml)

target_compile_definitions(cavewhere-qml-test PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test-qml")

target_link_libraries(cavewhere-qml-test
    PRIVATE
    Qt::Test
    Qt::Quick
    Qt::Widgets
    Qt::Qml
    Qt::QuickTest
    cavewhere-testlib
    cavewherelib
)

#### CaveWhere ####
qt_add_executable(CaveWhere
    main.cpp
    Cavewhere.rc
    testcases/cwSignalSpy.h testcases/cwSignalSpy.cpp
)

set_target_properties(CaveWhere PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "CaveWhere"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.cavewhere.CaveWhere"
    # MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
    RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/cavewhereIcon.icns"
)

target_link_libraries(CaveWhere
    PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::Quick
    Qt::Sql
    Qt::Test
    cavewherelib
)

if(WIN32)
    target_link_options(CaveWhere PRIVATE
        $<$<AND:$<CONFIG:Release>,$<STREQUAL:${CMAKE_SYSTEM_NAME},Windows>>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>
    )
endif()

if(APPLE)
    #Post build install for APP resources
    add_custom_command(TARGET CaveWhere POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:cavewherelibplugin> $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Resources/cavewherelib
        COMMENT "Copying QML module to application bundle."
    )
    add_custom_command(TARGET CaveWhere POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cavewhere-test> $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Resources
        COMMENT "Copying cavewhere-test."
    )
    add_custom_command(TARGET CaveWhere POST_BUILD
        # COMMAND ${CMAKE_COMMOND} -E
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SURVEX_OUTPUT_DIR} $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Resources/survex/
        # COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE:cavern> $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Resources/survex
        COMMENT "Copying QML module to application bundle."
    )
endif()




if(WIN32 OR APPLE)
	#### Building Installer for Windows and Mac ####
#	set_target_properties(CaveWhere PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
#	set_target_properties(CaveWhere PROPERTIES SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
#        include(installer/installer.cmake)
endif()

