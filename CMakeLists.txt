cmake_minimum_required(VERSION 3.23)

project(CaveWhere VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

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

# All subdirectoriesb
add_subdirectory(QMath3d)
add_subdirectory(asyncfuture)
add_subdirectory(dewalls)
add_subdirectory(cavewherelib)
add_subdirectory(qml-test-recorder)

#Allows QtCreator to find qml import directories
#https://doc.qt.io/qtcreator/creator-qml-modules-with-plugins.html#importing-qml-modules
# message(STATUS "Import directory: ${CMAKE_BINARY_DIR}/qml-test-recorder")
# set(QML_IMPORT_PATH ${CMAKE_BINARY_DIR} CACHE STRING "" FORCE)

set(SURVEX_CONAN OFF CACHE BOOL "Use survex Conan to manage dependencies" FORCE)

add_subdirectory(survex)
add_subdirectory(MarkScope)

# All packages we need to find
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
    URI cw.TestLib
    VERSION 1.0
    RESOURCE_PREFIX /qml
    OUTPUT_DIRECTORY cw/TestLib
    DEPENDENCIES
        QtQuick
        QtQuick.Controls
    SOURCES
        testlib/LoadProjectHelper.cpp
        testlib/LoadProjectHelper.h
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

file(GLOB cavewhere_qml_test_qrc_FILES "test-qml/*.qrc")
qt_add_resources(cavewhere_qml_test_QT_RESOURCES ${cavewhere_qml_test_qrc_FILES})
target_sources(cavewhere-qml-test PRIVATE ${cavewhere_qml_test_QT_RESOURCES})

qt_add_qml_module(cavewhere_simplemodule
    URI cw.Simple
    VERSION 1.0
    RESOURCE_PREFIX /qml
    OUTPUT_DIRECTORY cw/Simple
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
if(APPLE)
    set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist.in)
    set_source_files_properties(${MACOSX_BUNDLE_INFO_PLIST} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set(CMAKE_MACOSX_BUNDLE_ICON_FILE "cavewhereIcon.icns")
endif()


qt_add_executable(CaveWhere
    main.cpp
    Cavewhere.rc
    testcases/cwSignalSpy.h testcases/cwSignalSpy.cpp
    installer/mac/installMac.sh
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
    MarkScope
)

if(WIN32)
    target_link_options(CaveWhere PRIVATE
        $<$<AND:$<CONFIG:Release>,$<STREQUAL:${CMAKE_SYSTEM_NAME},Windows>>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>
    )
endif()

if(APPLE)

    set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist.in)

    set_source_files_properties(${MACOSX_BUNDLE_INFO_PLIST} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    set(CMAKE_MACOSX_BUNDLE_ICON_FILE "cavewhereIcon.icns")

    set_target_properties(CaveWhere PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_INFO_PLIST}
    )

    #Post build install for APP resources
    # Define paths
    # get_target_property(qmlModuleDir cavewherelibplugin QML_IMPORT_FOLDER)
    # message(STATUS "The QML module is copied to: ${qmlModuleDir}")

    # # Replace 'mytarget' with your target name, e.g., cavewherelibplugin
    # get_property(target_props TARGET cavewherelibplugin PROPERTY PROPERTIES)

    # message(STATUS "Listing properties:")
    # foreach(prop IN LISTS target_props)
    #     get_target_property(val cavewherelibplugin ${prop})
    #     message(STATUS "Target: ${prop} = ${val}")
    # endforeach()

    set(PLUGIN_DYLIB $<TARGET_FILE:cavewherelibplugin>)
    set(PLUGIN_DIR ${CMAKE_BINARY_DIR}/cavewherelib) #$<TARGET_FILE_DIR:cavewherelibplugin>)
    set(BUNDLE_PLUGIN_DIR $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Plugins/quick)
    set(BUNDLE_QML_DIR $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Resources/qml/cavewherelib)

    # Ensure Plugins/quick exists
    add_custom_command(TARGET CaveWhere POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BUNDLE_PLUGIN_DIR}
        COMMENT "Creating Plugins/quick directory in the app bundle."
    )

    # Copy dylib to Plugins/quick
    add_custom_command(TARGET CaveWhere POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${PLUGIN_DYLIB} ${BUNDLE_PLUGIN_DIR}
        COMMENT "Copying QML plugin dylib to Plugins/quick."
    )

    # Ensure Resources/qml exists
    add_custom_command(TARGET CaveWhere POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${BUNDLE_QML_DIR}
        COMMENT "Creating Resources/qml directory in the app bundle."
    )

    # Copy QML plugin metadata (qmldir, plugin.qmltypes) to Resources/qml
    add_custom_command(TARGET CaveWhere POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${PLUGIN_DIR}/qmldir ${BUNDLE_QML_DIR}/qmldir
        COMMAND ${CMAKE_COMMAND} -E copy ${PLUGIN_DIR}/cavewherelib.qmltypes ${BUNDLE_QML_DIR}/cavewherelib.qmltypes
        COMMAND ${CMAKE_COMMAND} -E copy ${PLUGIN_DIR}/Theme.js ${BUNDLE_QML_DIR}/Theme.js
        COMMAND ${CMAKE_COMMAND} -E copy ${PLUGIN_DIR}/Utils.js ${BUNDLE_QML_DIR}/Utils.js
        COMMAND ${CMAKE_COMMAND} -E copy ${PLUGIN_DIR}/VectorMath.js ${BUNDLE_QML_DIR}/VectorMath.js
        COMMENT "Copying qmldir and cavewherelib.qmltypes to Resources/qml."
    )

    # Create a symbolic link in Resources/qml pointing to the dylib in Plugins/quick
    add_custom_command(TARGET CaveWhere POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink ../../../Plugins/quick/$<TARGET_FILE_NAME:cavewherelibplugin> ${BUNDLE_QML_DIR}/$<TARGET_FILE_NAME:cavewherelibplugin>
        COMMENT "Creating symbolic link to cavewherelibplugin dylib in Resources/qml."
    )



    # add_custom_command(TARGET CaveWhere POST_BUILD
    #     COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:cavewherelibplugin> $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Plugins/quick/cavewherelib
    #     COMMENT "Copying QML module to application bundle."
    # )

    add_custom_command(TARGET CaveWhere POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cavewhere-test> $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/MacOS
        COMMENT "Copying cavewhere-test."
    )
    add_custom_command(TARGET CaveWhere POST_BUILD
        # COMMAND ${CMAKE_COMMOND} -E
        # COMMAND ${CMAKE_COMMAND} -E copy_directory ${SURVEX_OUTPUT_DIR} $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/MacOS/survex/
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cavern> $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/MacOS/cavern
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:survexport> $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/MacOS/survexport

        COMMENT "Copying survex module to application bundle."
    )

    # Define the icon file
    set(APP_ICON "${CMAKE_SOURCE_DIR}/${CMAKE_MACOSX_BUNDLE_ICON_FILE}")
    set(DEST_ICON "${CMAKE_BINARY_DIR}/CaveWhere.app/Contents/Resources/${CMAKE_MACOSX_BUNDLE_ICON_FILE}")

    # Ensure the Resources directory exists
    add_custom_command(TARGET CaveWhere POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CaveWhere>/../Resources"

        # Copy the icon into the app bundle's Resources folder
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${APP_ICON}" "${DEST_ICON}"

        COMMENT "Copying CaveWhere.icns into the app bundle..."
    )

    foreach(MSG_FILE ${SVX_MESSAGE_FILES})
        get_filename_component(MSG_NAME ${MSG_FILE} NAME)
        add_custom_command(TARGET CaveWhere POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${MSG_FILE} $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Resources/${MSG_NAME}
            COMMENT "Copying ${MSG_NAME} to application bundle."
        )
    endforeach()

    # Ensure installMac.sh is copied whenever it changes
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/installMac.sh
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/installer/mac/installMac.sh ${CMAKE_BINARY_DIR}/installMac.sh
        DEPENDS ${CMAKE_SOURCE_DIR}/installer/mac/installMac.sh
        COMMENT "Copying installMac.sh to the build directory when modified." ${CMAKE_BINARY_DIR}/installMac.sh
    )

    add_custom_target(copy_installMac ALL DEPENDS ${CMAKE_BINARY_DIR}/installMac.sh)

    # Ensure it runs as part of the post-build process
    add_dependencies(CaveWhere copy_installMac)
    add_dependencies(CaveWhere messageFiles) #Makes sure the message files get copied to bundle correctly

endif()




if(WIN32 OR APPLE)
	#### Building Installer for Windows and Mac ####
#	set_target_properties(CaveWhere PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
#	set_target_properties(CaveWhere PROPERTIES SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    include(installer/installer.cmake)
endif()

