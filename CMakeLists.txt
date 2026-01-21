cmake_minimum_required(VERSION 3.23)

project(CaveWhere VERSION 1.0.0 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
qt_standard_project_setup()
find_package(absl REQUIRED)

#Add address sanitizer for clang build only
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_options($<$<CONFIG:Debug>:-fsanitize=address>)
    add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON CACHE BOOL "Generate QMLLS INI file")

# Include the GenerateExportHeader module
include(GenerateExportHeader)

# Work around Conan Abseil CMakeDeps missing libs by explicitly linking absl libs.
function(cw_link_absl_static target)
    if(NOT (WIN32 OR (UNIX AND NOT APPLE)))
        return()
    endif()

    if(WIN32)
        set(_absl_glob_patterns "abseil_dll.lib" "absl_*.lib" "libabsl_*.lib")
    else()
        set(_absl_glob_patterns "libabsl_*.a")
    endif()

    set(_absl_configs "")
    if(CMAKE_CONFIGURATION_TYPES)
        set(_absl_configs ${CMAKE_CONFIGURATION_TYPES})
    elseif(CMAKE_BUILD_TYPE)
        set(_absl_configs ${CMAKE_BUILD_TYPE})
    endif()

    if(NOT _absl_configs)
        message(WARNING "No build configuration detected; skipping absl static linking for ${target}")
        return()
    endif()

    foreach(_absl_config IN LISTS _absl_configs)
        string(TOUPPER "${_absl_config}" _absl_config_upper)
        set(_absl_lib_dirs "${abseil_LIB_DIRS_${_absl_config_upper}}")

        if(_absl_lib_dirs)
            list(GET _absl_lib_dirs 0 _absl_lib_dir)
            set(_absl_static_libs "")
            foreach(_absl_pattern IN LISTS _absl_glob_patterns)
                file(GLOB _absl_pattern_libs "${_absl_lib_dir}/${_absl_pattern}")
                list(APPEND _absl_static_libs ${_absl_pattern_libs})
            endforeach()
            list(REMOVE_DUPLICATES _absl_static_libs)

            if(_absl_static_libs)
                if(UNIX AND NOT APPLE)
                    target_link_libraries(
                        ${target}
                        PRIVATE
                        "$<$<CONFIG:${_absl_config}>:-Wl,--start-group>"
                        "$<$<CONFIG:${_absl_config}>:${_absl_static_libs}>"
                        "$<$<CONFIG:${_absl_config}>:-Wl,--end-group>"
                    )
                else()
                    target_link_libraries(${target} PRIVATE "$<$<CONFIG:${_absl_config}>:${_absl_static_libs}>")
                endif()
            else()
                message(WARNING "Abseil static libs not found in ${_absl_lib_dir} for ${_absl_config}")
            endif()
        else()
            message(WARNING "Abseil lib directories not found for ${_absl_config}; skipping absl static linking for ${target}")
        endif()
    endforeach()
endfunction()

# All subdirectoriesb
add_subdirectory(QMath3d)
add_subdirectory(asyncfuture)
add_subdirectory(dewalls)
add_subdirectory(cavewherelib)
add_subdirectory(qml-test-recorder)
add_subdirectory(monad)
add_subdirectory(QQuickGit)

#Allows QtCreator to find qml import directories
#https://doc.qt.io/qtcreator/creator-qml-modules-with-plugins.html#importing-qml-modules
# message(STATUS "Import directory: ${CMAKE_BINARY_DIR}/qml-test-recorder")
# set(QML_IMPORT_PATH ${CMAKE_BINARY_DIR} CACHE STRING "" FORCE)

set(SURVEX_CONAN OFF CACHE BOOL "Use survex Conan to manage dependencies" FORCE)
option(MOBILE_BUILD "Builds as mobile: for ios or android" OFF)
if(ANDROID OR IOS)
    set(MOBILE_BUILD ON CACHE BOOL "Builds as mobile: for ios or android" FORCE)
endif()

if(MOBILE_BUILD)
    # Mobile builds are fully static (including protobuf); tests are disabled to avoid duplicate protobuf runtimes via plugins.
    set(BUILD_TESTING OFF CACHE BOOL "Disable tests for mobile builds" FORCE)
endif()

add_subdirectory(MarkScope)
add_subdirectory(QuickQanava)
# add_subdirectory(SignalSpyChecker)

if(NOT MOBILE_BUILD)
    add_subdirectory(survex)
endif()

if(UNIX AND NOT APPLE AND NOT MOBILE_BUILD)
    include(ExternalProject)

    set(LINUXDEPLOY_TOOLS_DIR "${CMAKE_BINARY_DIR}/linuxdeploy-tools")
    set(LINUXDEPLOY_BUILD_DIR "${LINUXDEPLOY_TOOLS_DIR}/linuxdeploy-build")
    set(LINUXDEPLOY_QT_BUILD_DIR "${LINUXDEPLOY_TOOLS_DIR}/linuxdeploy-qt-build")
    set(LINUXDEPLOY_APPIMAGE_BUILD_DIR "${LINUXDEPLOY_TOOLS_DIR}/linuxdeploy-appimage-build")
    set(APPIMAGETOOL_BUILD_DIR "${LINUXDEPLOY_TOOLS_DIR}/appimagetool-build")

    set(APPIMAGETOOL_EXECUTABLE "" CACHE FILEPATH "Path to appimagetool")

    find_program(PATCHELF_EXECUTABLE patchelf)
    if(PATCHELF_EXECUTABLE)
        set(PATCHELF "${PATCHELF_EXECUTABLE}" CACHE FILEPATH "Path to patchelf")
    else()
        message(STATUS "patchelf not found; linuxdeploy build may fail at build time.")
    endif()

    set(LINUXDEPLOY_CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DBUILD_TESTING=OFF
    )

    if(PATCHELF_EXECUTABLE)
        list(APPEND LINUXDEPLOY_CMAKE_ARGS -DPATCHELF=${PATCHELF_EXECUTABLE})
    endif()

    ExternalProject_Add(linuxdeploy_ep
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/installer/linux/linuxdeploy
        BINARY_DIR ${LINUXDEPLOY_BUILD_DIR}
        CMAKE_ARGS ${LINUXDEPLOY_CMAKE_ARGS}
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target linuxdeploy
        INSTALL_COMMAND ""
    )

    ExternalProject_Add(linuxdeploy_plugin_qt_ep
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/installer/linux/linuxdeploy-plugin-qt
        BINARY_DIR ${LINUXDEPLOY_QT_BUILD_DIR}
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DBUILD_TESTING=OFF
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target linuxdeploy-plugin-qt
        INSTALL_COMMAND ""
    )

    ExternalProject_Add(linuxdeploy_plugin_appimage_ep
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/installer/linux/linuxdeploy-plugin-appimage
        BINARY_DIR ${LINUXDEPLOY_APPIMAGE_BUILD_DIR}
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DBUILD_TESTING=OFF
            -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${LINUXDEPLOY_APPIMAGE_BUILD_DIR}/bin
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target linuxdeploy-plugin-appimage
        INSTALL_COMMAND ""
    )

    if(APPIMAGETOOL_EXECUTABLE AND NOT EXISTS "${APPIMAGETOOL_EXECUTABLE}")
        set(APPIMAGETOOL_EXECUTABLE "" CACHE FILEPATH "Path to appimagetool" FORCE)
    endif()

    if(NOT APPIMAGETOOL_EXECUTABLE)
        find_program(APPIMAGETOOL_EXECUTABLE appimagetool)
    endif()

    set(APPIMAGETOOL_BUILD_AVAILABLE OFF)
    find_program(PKG_CONFIG_EXECUTABLE pkg-config)
    if(PKG_CONFIG_EXECUTABLE)
        execute_process(
            COMMAND ${PKG_CONFIG_EXECUTABLE} --exists gpgme libgcrypt glib-2.0 gio-2.0 libcurl
            RESULT_VARIABLE APPIMAGETOOL_PKG_RESULT
        )
        if(APPIMAGETOOL_PKG_RESULT EQUAL 0)
            set(APPIMAGETOOL_BUILD_AVAILABLE ON)
        endif()
    endif()

    if(NOT APPIMAGETOOL_EXECUTABLE AND APPIMAGETOOL_BUILD_AVAILABLE)
        ExternalProject_Add(appimagetool_ep
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/installer/linux/appimagetool
            BINARY_DIR ${APPIMAGETOOL_BUILD_DIR}
            CMAKE_ARGS
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DBUILD_STATIC=OFF
                -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${APPIMAGETOOL_BUILD_DIR}/bin
            BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target appimagetool
            INSTALL_COMMAND ""
        )
        set(APPIMAGETOOL_EXECUTABLE "${APPIMAGETOOL_BUILD_DIR}/bin/appimagetool" CACHE FILEPATH "Path to appimagetool" FORCE)
    elseif(NOT APPIMAGETOOL_EXECUTABLE AND NOT APPIMAGETOOL_BUILD_AVAILABLE)
        message(STATUS "appimagetool build dependencies not found; install gpgme, libgcrypt, glib-2.0, gio-2.0, and libcurl, or set APPIMAGETOOL_EXECUTABLE.")
    endif()

    if(NOT LINUXDEPLOY_EXECUTABLE)
        set(LINUXDEPLOY_EXECUTABLE "${LINUXDEPLOY_BUILD_DIR}/bin/linuxdeploy" CACHE FILEPATH "Path to linuxdeploy" FORCE)
    endif()
    if(NOT LINUXDEPLOY_QT_PLUGIN_EXECUTABLE)
        set(LINUXDEPLOY_QT_PLUGIN_EXECUTABLE "${LINUXDEPLOY_QT_BUILD_DIR}/bin/linuxdeploy-plugin-qt" CACHE FILEPATH "Path to linuxdeploy-plugin-qt" FORCE)
    endif()
    if(NOT LINUXDEPLOY_APPIMAGE_PLUGIN_EXECUTABLE)
        set(LINUXDEPLOY_APPIMAGE_PLUGIN_EXECUTABLE "${LINUXDEPLOY_APPIMAGE_BUILD_DIR}/bin/linuxdeploy-plugin-appimage" CACHE FILEPATH "Path to linuxdeploy-plugin-appimage" FORCE)
    endif()
endif()

# All packages we need to find
find_package(Catch2 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Qt6
    COMPONENTS
    Core
    Gui
    Widgets
    Quick
    Sql
    Xml
    Concurrent
    Svg
    Test
    QuickTest
    QuickControls2
    REQUIRED)

set(CMAKE_AUTOMOC ON)

if(BUILD_TESTING)
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
        # STATIC
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
        Qt6::Test
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Quick
        Qt6::Qml
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
        Qt6::Test
        Catch2::Catch2
        cavewhere-testlib
        cavewherelib
        protobuf::libprotobuf
        SignalSpyChecker
        QuickQanavaplugin
    )

    qt_import_qml_plugins(cavewhere-test)

    cw_link_absl_static(cavewhere-test)

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

    # Make the remaining QML files relative to the project directory
    file(GLOB cavewhere_qmltest_QML_FILES "test-qml/*.qml")
    set(cavewhere_qmltest_RELATIVE_QML_FILES "")
    foreach(file ${cavewhere_qmltest_QML_FILES})
        file(RELATIVE_PATH relFile "${CMAKE_CURRENT_SOURCE_DIR}" "${file}")
        message(STATUS "Adding QML file: ${relFile}")
        list(APPEND cavewhere_qmltest_RELATIVE_QML_FILES "${relFile}")
    endforeach()

    qt_add_resources(cavewhere-qml-test "test-qmls"
        PREFIX "/test-qmls"
        FILES
        ${cavewhere_qmltest_RELATIVE_QML_FILES})

    target_compile_definitions(cavewhere-qml-test PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test-qml")

    target_link_libraries(cavewhere-qml-test
        PRIVATE
        Qt6::Test
        Qt6::Quick
        Qt6::Widgets
        Qt6::Qml
        Qt6::QuickTest
        cavewhere-testlib
        cavewherelib
        QuickQanavaplugin
    )

    # Ensure QML modules are imported without relying on QML2_IMPORT_PATH.
    qt_import_qml_plugins(cavewhere-qml-test)

    # Add the cw.simple library
    add_subdirectory(test-qml)
endif()

#### CaveWhere ####
# if(APPLE)
#     set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist.in)
#     set_source_files_properties(${MACOSX_BUNDLE_INFO_PLIST} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
#     set(CMAKE_MACOSX_BUNDLE_ICON_FILE "cavewhereIcon.icns")
# endif()


qt_add_executable(CaveWhere
    MANUAL_FINALIZATION
    main.cpp
    Cavewhere.rc
    testcases/cwSignalSpy.h testcases/cwSignalSpy.cpp
    installer/mac/installMac.sh
)

set_target_properties(CaveWhere PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

target_link_libraries(CaveWhere
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::Sql
    Qt6::Test
    cavewherelib
    QuickQanavaplugin
    MarkScope
)

if(TARGET cavern)
    add_dependencies(CaveWhere cavern)
endif()
if(TARGET survexport)
    add_dependencies(CaveWhere survexport)
endif()

# Ensure QML modules are imported without relying on QML2_IMPORT_PATH.
qt_import_qml_plugins(CaveWhere)
qt_finalize_executable(CaveWhere)

if(WIN32)
    set(_conan_runtime_dlls "")
    set(_protobuf_bin_dirs "")
    if(DEFINED protobuf_BIN_DIRS_RELEASE)
        list(APPEND _protobuf_bin_dirs ${protobuf_BIN_DIRS_RELEASE})
    endif()
    if(DEFINED protobuf_BIN_DIRS_DEBUG)
        list(APPEND _protobuf_bin_dirs ${protobuf_BIN_DIRS_DEBUG})
    endif()
    if(DEFINED protobuf_BIN_DIRS)
        list(APPEND _protobuf_bin_dirs ${protobuf_BIN_DIRS})
    endif()
    foreach(_dir IN LISTS _protobuf_bin_dirs)
        file(GLOB _protobuf_dlls
            "${_dir}/libprotobuf*.dll"
            "${_dir}/protobuf*.dll"
            "${_dir}/libutf8_validity.dll"
        )
        list(APPEND _conan_runtime_dlls ${_protobuf_dlls})
    endforeach()

    set(_abseil_bin_dirs "")
    if(DEFINED abseil_BIN_DIRS_RELEASE)
        list(APPEND _abseil_bin_dirs ${abseil_BIN_DIRS_RELEASE})
    endif()
    if(DEFINED abseil_BIN_DIRS_DEBUG)
        list(APPEND _abseil_bin_dirs ${abseil_BIN_DIRS_DEBUG})
    endif()
    if(DEFINED abseil_BIN_DIRS)
        list(APPEND _abseil_bin_dirs ${abseil_BIN_DIRS})
    endif()
    foreach(_dir IN LISTS _abseil_bin_dirs)
        file(GLOB _abseil_dlls
            "${_dir}/abseil_dll.dll"
            "${_dir}/absl_*.dll"
            "${_dir}/libutf8_validity.dll"
        )
        list(APPEND _conan_runtime_dlls ${_abseil_dlls})
    endforeach()
    list(REMOVE_DUPLICATES _conan_runtime_dlls)

    if(_conan_runtime_dlls)
        add_custom_command(TARGET CaveWhere POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_conan_runtime_dlls}
                $<TARGET_FILE_DIR:CaveWhere>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying Conan runtime DLLs next to CaveWhere.exe."
        )
    endif()

    # add_custom_command(TARGET CaveWhere POST_BUILD
    #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #         $<TARGET_RUNTIME_DLLS:CaveWhere>
    #         $<TARGET_FILE_DIR:CaveWhere>
    #     COMMAND_EXPAND_LISTS
    #     COMMENT "Copying CaveWhere runtime DLLs next to the executable."
    # )

    target_link_options(CaveWhere PRIVATE
        $<$<AND:$<CONFIG:Release>,$<STREQUAL:${CMAKE_SYSTEM_NAME},Windows>>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>
    )
endif()

if(APPLE)
    set(CMAKE_MACOSX_BUNDLE_ICON_FILE "cavewhereIcon.icns")
    set_source_files_properties("${CMAKE_SOURCE_DIR}/cavewhereIcon.icns"
        PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )

    set_target_properties(CaveWhere PROPERTIES
        MACOSX_BUNDLE TRUE
        # MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_INFO_PLIST}
        MACOSX_BUNDLE_BUNDLE_VERSION 1 #The build version
        # MACOSX_BUNDLE_BUNDLE_NAME "CaveWhere"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.cavewhere.CaveWhere"
        #Enables instruments to run correctly in macos
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/entitlements.plist"
        #XCODE_ATTRIBUTE_DEVELOPMENT_TEAM
        XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--deep"
        #XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.cavewhere.CaveWhere"

        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist.in"
        RESOURCE "${CMAKE_SOURCE_DIR}/cavewhereIcon.icns"
    )

    # set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist.in)
    # set(CMAKE_MACOSX_BUNDLE_ICON_FILE "cavewhereIcon.icns")

    # set_source_files_properties(${MACOSX_BUNDLE_INFO_PLIST} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

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

    if(NOT MOBILE_BUILD)
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
            COMMAND ${CMAKE_COMMAND} -E copy ${PLUGIN_DIR}/qml/Utils.js ${BUNDLE_QML_DIR}/qml/Utils.js
            COMMAND ${CMAKE_COMMAND} -E copy ${PLUGIN_DIR}/qml/VectorMath.js ${BUNDLE_QML_DIR}/qml/VectorMath.js
            COMMENT "Copying qmldir and cavewherelib.qmltypes to Resources/qml."
        )

        # Copy the QuickQanava qml plugin
        set(QuickQanavaPluginDir ${CMAKE_BINARY_DIR}/QuickQanava/src/QuickQanava)
        set(QuickQanava_BUNDLE $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Resources/qml/QuickQanava)

        add_custom_command(TARGET CaveWhere POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${QuickQanavaPluginDir}/qmldir ${QuickQanava_BUNDLE}/qmldir
            COMMAND ${CMAKE_COMMAND} -E copy ${QuickQanavaPluginDir}/QuickQanava.qmltypes ${QuickQanava_BUNDLE}/QuickQanava.qmltypes
            COMMENT "Copying qmldir and QuickQanava.qmltypes to Resources/qml."
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

        if(BUILD_TESTING)
            add_custom_command(TARGET CaveWhere POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cavewhere-test> $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/MacOS
                COMMENT "Copying cavewhere-test."
            )
        endif()

        # if(NOT MOBILE_BUILD)
            #Disable survex

        add_custom_command(TARGET CaveWhere POST_BUILD
            # COMMAND ${CMAKE_COMMOND} -E
            # COMMAND ${CMAKE_COMMAND} -E copy_directory ${SURVEX_OUTPUT_DIR} $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/MacOS/survex/
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cavern> $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/MacOS/cavern
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:survexport> $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/MacOS/survexport

            COMMENT "Copying survex module to application bundle."
        )

        foreach(MSG_FILE ${SVX_MESSAGE_FILES})
            get_filename_component(MSG_NAME ${MSG_FILE} NAME)
            add_custom_command(TARGET CaveWhere POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${MSG_FILE} $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Resources/${MSG_NAME}
                COMMENT "Copying ${MSG_NAME} to application bundle."
            )
        endforeach()

        add_dependencies(CaveWhere messageFiles) #Makes sure the message files get copied to bundle correctly

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


        # if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            ## This allows for profiling to work correctly for macos
            # Query the available code signing identities.
            execute_process(
              COMMAND security find-identity -v -p codesigning
              OUTPUT_VARIABLE IDENTITY_OUTPUT
              OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            message(STATUS "Available code signing identities:\n${IDENTITY_OUTPUT}")

            # Match the first identity's hash.
             # Expected output line example:
             #   1) ABCDEF1234567890 "Apple Development: John Doe (TEAMID)"
             # This regex captures the hash (the first token after the index).
             string(REGEX MATCH "[0-9]+\\) ([A-F0-9]+) \"[^\"]+\"" dummy "${IDENTITY_OUTPUT}")
             set(CODESIGN_IDENTITY "${CMAKE_MATCH_1}")

            if(NOT CODESIGN_IDENTITY)
               message(WARNING "No valid code signing identity found!")
            else()
               # Prefix with "0x" so codesign recognizes it as a certificate hash.
               # set(CODESIGN_IDENTITY "0x${CODESIGN_KEY}")
               message(STATUS "Using code signing identity: ${CODESIGN_IDENTITY}")
               add_custom_command(TARGET CaveWhere POST_BUILD
                 COMMAND codesign --force --deep --sign "${CODESIGN_IDENTITY}"
                         --entitlements "${CMAKE_SOURCE_DIR}/entitlements.plist"
                         "$<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/.."
                 COMMENT "Signing MyApp with get-task-allow entitlement"
               )
            endif()

        endif()
    endif()

endif()




if(WIN32 OR APPLE OR UNIX)
	#### Building Installer for Windows, Mac, and Linux ####
#	set_target_properties(CaveWhere PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
#	set_target_properties(CaveWhere PROPERTIES SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    include(installer/installer.cmake)
endif()
