cmake_minimum_required(VERSION 3.21.1)

project(cavewhere VERSION 1.0.0 LANGUAGES CXX)


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

set(SURVEX_CONAN OFF CACHE BOOL "Use survex Conan to manage dependencies" FORCE)


set(CW_BUNDLE_DIR "${CMAKE_BINARY_DIR}")
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    # macOS
    set(CW_BUNDLE_DIR "${CMAKE_BINARY_DIR}/CaveWhere.app/Contents/MacOS/survex")
    set(SURVEX_OUTPUT_DIR "${CW_BUNDLE_DIR}" CACHE PATH "" FORCE)
# else()
    # Other systems (e.g., Linux, Windows)
#     set(SURVEX_OUTPUT_DIR "${CMAKE_BINARY_DIR}/survex")
endif()
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
    REQUIRED)



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

file(GLOB cavewhere_test_qrc_FILES "testcases/*.qrc")
qt_add_resources(cavewhere_test_QT_RESOURCES ${cavewhere_test_qrc_FILES})
target_sources(cavewhere-test PRIVATE ${cavewhere_test_QT_RESOURCES})

set_target_properties(cavewhere-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CW_BUNDLE_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CW_BUNDLE_DIR}
    ARCHIVE_OUTPUT_DIRECTORY ${CW_BUNDLE_DIR}
)

target_link_libraries(cavewhere-test
    PRIVATE
    Qt::Test
    Catch2::Catch2
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



if(WIN32 OR APPLE)
	#### Building Installer for Windows and Mac ####
#	set_target_properties(CaveWhere PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
#	set_target_properties(CaveWhere PROPERTIES SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
#        include(installer/installer.cmake)
endif()

