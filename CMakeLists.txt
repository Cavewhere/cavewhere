cmake_minimum_required(VERSION 3.23)

# Derive the marketing version (CFBundleShortVersionString) from the most recent
# git tag reachable from HEAD. Real releases tag as e.g. 2026.4.2, which is the
# value Apple expects in CFBundleShortVersionString. Fallback "0.0.0" for source
# tarballs / checkouts with no tags — the iOS configure-time check rejects this
# fallback so archive builds fail loudly instead of shipping with a bogus version.
set(_cw_marketing_version "0.0.0")
set(_cw_default_build_number "1")
set(_cw_latest_tag "")
# CFBundleShortVersionString: 1–3 dot-separated non-negative integers.
# CMake regex doesn't support {n,m}, so spell the form out.
set(_cw_marketing_version_regex "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?$")

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _cw_latest_tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _cw_tag_rc
        ERROR_QUIET)
    if(_cw_tag_rc EQUAL 0 AND _cw_latest_tag)
        string(REGEX REPLACE "^v" "" _cw_stripped_tag "${_cw_latest_tag}")
        if(_cw_stripped_tag MATCHES "${_cw_marketing_version_regex}")
            set(_cw_marketing_version "${_cw_stripped_tag}")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} rev-list ${_cw_latest_tag}..HEAD --count
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE _cw_commits_since_tag
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE _cw_count_rc
                ERROR_QUIET)
            if(_cw_count_rc EQUAL 0 AND _cw_commits_since_tag)
                math(EXPR _cw_default_build_number "${_cw_commits_since_tag} + 1")
            endif()
        else()
            message(STATUS
                "CaveWhere: latest git tag '${_cw_latest_tag}' is not a valid "
                "Apple marketing version; falling back to ${_cw_marketing_version}.")
        endif()
    endif()
endif()

project(CaveWhere VERSION ${_cw_marketing_version} LANGUAGES CXX)

# Mobile build number — iOS CFBundleVersion and Android android:versionCode.
# Cache default is the sentinel string "auto", which is resolved below to
# commits-since-tag + 1 every reconfigure (iOS only) so normal dev builds
# track git state. For TestFlight iteration (App Store Connect requires a
# unique, strictly-increasing build number per marketing version) pass
# -DCAVEWHERE_BUILD_NUMBER=N; that value replaces the sentinel in the cache
# and sticks across reconfigures. Android rejects "auto" — see the guard
# below.
set(CAVEWHERE_BUILD_NUMBER "auto"
    CACHE STRING "Mobile build number (iOS CFBundleVersion / Android versionCode); 'auto' = commits-since-tag + 1 (iOS only)")

# Android android:versionCode is a single strictly-increasing integer across
# every Play Store upload for the package — it does NOT reset per
# versionName the way iOS CFBundleVersion resets per
# CFBundleShortVersionString. So commits-since-tag + 1 can produce the same
# number across tag cycles (e.g. first commit after tag A, first commit
# after tag B), which would cause silent Play Store rejections. Force the
# user to pick an explicit integer instead.
if(ANDROID AND CAVEWHERE_BUILD_NUMBER STREQUAL "auto")
    message(FATAL_ERROR
        "CaveWhere: Android builds require an explicit "
        "-DCAVEWHERE_BUILD_NUMBER=<N>. The 'auto' default is iOS-only — on "
        "Android, android:versionCode must be strictly increasing across all "
        "Play Store uploads for the package and does not reset per "
        "versionName, so commits-since-tag + 1 can collide across tag cycles. "
        "For local dev builds any integer (e.g. 1) will do; for Play Store "
        "uploads pick the next unused integer greater than every previous "
        "upload.")
endif()

if(CAVEWHERE_BUILD_NUMBER STREQUAL "auto")
    # Shadow the cache with a local variable so @CAVEWHERE_BUILD_NUMBER@ in
    # ios/Info.plist.in and XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION see the
    # effective numeric value, while the cache entry stays "auto" and keeps
    # tracking git state on each reconfigure.
    set(CAVEWHERE_BUILD_NUMBER "${_cw_default_build_number}")
endif()

message(STATUS
    "CaveWhere marketing version: ${PROJECT_VERSION} (build ${CAVEWHERE_BUILD_NUMBER})")

# Detect iOS builds. The Xcode generator doesn't set CMAKE_CROSSCOMPILING or CMAKE_SYSTEM_NAME=iOS,
# so we detect iOS via CMAKE_OSX_SYSROOT instead.
if(ANDROID OR IOS OR CMAKE_OSX_SYSROOT MATCHES "iphoneos|iphonesimulator")
    set(CW_MOBILE ON)
else()
    set(CW_MOBILE OFF)
endif()

# MOBILE_BUILD is the cache-exposed twin of CW_MOBILE that subdirectories
# gate on (e.g. cavewherelib adds the CW_MOBILE_BUILD compile define under
# it). Force it here — before any add_subdirectory() — so subdirs see the
# right value when they're evaluated.
option(MOBILE_BUILD "Builds as mobile: for ios or android" OFF)
if(CW_MOBILE)
    set(MOBILE_BUILD ON CACHE BOOL "Builds as mobile: for ios or android" FORCE)
endif()

if(MOBILE_BUILD)
    # Mobile builds are fully static (including protobuf); tests are disabled to avoid duplicate protobuf runtimes via plugins.
    set(BUILD_TESTING OFF CACHE BOOL "Disable tests for mobile builds" FORCE)
endif()

# App Store format validation (iOS only). Catches typos like
# -DCAVEWHERE_BUILD_NUMBER=not-a-number before Xcode starts.
if(CW_MOBILE)
    if(PROJECT_VERSION STREQUAL "0.0.0")
        message(FATAL_ERROR
            "CaveWhere: no git tag found; refusing to build iOS with fallback "
            "version 0.0.0. Create a release tag (e.g. 2026.4.2) first.")
    endif()
    if(NOT PROJECT_VERSION MATCHES "${_cw_marketing_version_regex}")
        message(FATAL_ERROR
            "CaveWhere: CFBundleShortVersionString '${PROJECT_VERSION}' is not "
            "a valid Apple marketing version (1–3 dot-separated integers).")
    endif()
    string(LENGTH "${CAVEWHERE_BUILD_NUMBER}" _cw_bn_len)
    # CFBundleVersion: dot-separated non-negative integers, ≤18 chars.
    if(NOT CAVEWHERE_BUILD_NUMBER MATCHES "^[0-9]+(\\.[0-9]+)*$"
            OR _cw_bn_len GREATER 18)
        message(FATAL_ERROR
            "CaveWhere: CAVEWHERE_BUILD_NUMBER '${CAVEWHERE_BUILD_NUMBER}' is "
            "not a valid CFBundleVersion (dot-separated integers, ≤18 chars).")
    endif()

    # Android android:versionCode is a single 32-bit signed integer, and Play
    # Store caps it at 2,100,000,000. It must be strictly increasing across
    # every upload under the same package name (does NOT reset per
    # versionName), so dot-separated values are invalid here even though iOS
    # accepts them.
    if(ANDROID)
        if(NOT CAVEWHERE_BUILD_NUMBER MATCHES "^[0-9]+$")
            message(FATAL_ERROR
                "CaveWhere: CAVEWHERE_BUILD_NUMBER '${CAVEWHERE_BUILD_NUMBER}' is "
                "not a valid Android versionCode (single non-negative integer).")
        endif()
        if(CAVEWHERE_BUILD_NUMBER GREATER 2100000000)
            message(FATAL_ERROR
                "CaveWhere: CAVEWHERE_BUILD_NUMBER '${CAVEWHERE_BUILD_NUMBER}' "
                "exceeds the Play Store versionCode cap of 2100000000.")
        endif()
    endif()
endif()

include(${CMAKE_SOURCE_DIR}/cmake/CavewhereConanBuildEnv.cmake)
cw_prepend_conan_buildenv_path()

find_package(Qt6 REQUIRED COMPONENTS Core)
qt_standard_project_setup()
find_package(absl REQUIRED)
find_package(libgit2)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#Add address sanitizer for clang build only
option(ASAN_ENABLED "Enable AddressSanitizer on macOS Debug builds" ON)
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND ASAN_ENABLED)
    add_compile_options($<$<CONFIG:Debug>:-fsanitize=address>)
    add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON CACHE BOOL "Generate QMLLS INI file")

# Include the GenerateExportHeader module
include(GenerateExportHeader)

include(${CMAKE_SOURCE_DIR}/cmake/CavewhereAbseilLink.cmake)

# All subdirectoriesb
add_subdirectory(QMath3d)
add_subdirectory(asyncfuture)
add_subdirectory(dewalls)
add_subdirectory(qlibssh2)
add_subdirectory(cavewherelib)
add_subdirectory(qml-test-recorder)
add_subdirectory(monad)
add_subdirectory(QQuickGit)

#Allows QtCreator to find qml import directories
#https://doc.qt.io/qtcreator/creator-qml-modules-with-plugins.html#importing-qml-modules
# message(STATUS "Import directory: ${CMAKE_BINARY_DIR}/qml-test-recorder")
# set(QML_IMPORT_PATH ${CMAKE_BINARY_DIR} CACHE STRING "" FORCE)

set(SURVEX_CONAN OFF CACHE BOOL "Use survex Conan to manage dependencies" FORCE)

add_subdirectory(MarkScope)
add_subdirectory(QuickQanava)
# add_subdirectory(SignalSpyChecker)

add_subdirectory(survex)

if(UNIX AND NOT APPLE AND NOT MOBILE_BUILD)
    include(${CMAKE_SOURCE_DIR}/cmake/CavewhereLinuxDeploy.cmake)
    cavewhere_setup_linux_appimage(CaveWhere)
endif()

# All packages we need to find
find_package(Catch2 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
# laslib is also a private dep of cavewherelib; we re-find it here so the
# testlib's LazFixtureHelper can link against it directly without dragging
# the LASlib include path into cavewherelib's PUBLIC interface.
find_package(laslib CONFIG REQUIRED)

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
            testlib/cwSketchTestHelpers.cpp
            testlib/cwSketchTestHelpers.h
            testlib/LazFixtureHelper.cpp
            testlib/LazFixtureHelper.h
            testlib/BoulderFixtureHelper.cpp
            testlib/BoulderFixtureHelper.h
            testlib/MockAuthProvider.cpp
            testlib/MockAuthProvider.h
            testlib/SvgOverlapAnalyzer.cpp
            testlib/SvgOverlapAnalyzer.h
            QQuickGit/tests/lfs/LfsServer.cpp
            QQuickGit/tests/lfs/LfsServer.h
        QML_FILES
            testlib/TesterAssistedGateDialog.qml
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
        qt6keychain
        cavewherelib
        laslib::laslib
    )

    target_compile_definitions(cavewhere-testlib PUBLIC
        CW_TESTCASES_DATASET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testcases/datasets"
        CW_QML_TEST_DATASET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test-qml/datasets"
    )

    target_include_directories(cavewhere-testlib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/testlib
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    target_include_directories(cavewhere-testlib PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/QQuickGit/tests/lfs
        $<TARGET_PROPERTY:libgit2::libgit2,INTERFACE_INCLUDE_DIRECTORIES>
    )

    #### cavewhere-test ####
    qt_add_executable(cavewhere-test)

    set_target_properties(cavewhere-test PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        AUTOUIC ON
    )

    file(GLOB cavewhere_test_CPP_FILES CONFIGURE_DEPENDS "testcases/*.cpp")
    file(GLOB cavewhere_test_H_FILES CONFIGURE_DEPENDS "testcases/*.h")
    target_sources(cavewhere-test PRIVATE ${cavewhere_test_CPP_FILES} ${cavewhere_test_H_FILES})

    target_link_libraries(cavewhere-test
        PRIVATE
        Qt6::Test
        Qt6::Network
        Catch2::Catch2
        cavewhere-testlib
        cavewherelib
        qquickgit-lfs-test-server
        protobuf::libprotobuf
        SignalSpyChecker
        QuickQanavaplugin
    )

    target_include_directories(cavewhere-test PRIVATE
        $<TARGET_PROPERTY:libgit2::libgit2,INTERFACE_INCLUDE_DIRECTORIES>
    )

    qt_import_qml_plugins(cavewhere-test)

    cw_link_absl_static(cavewhere-test)

    cw_bundle_proj_data(cavewhere-test proj)

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

    # Make the remaining QML files relative to the project directory
    file(GLOB cavewhere_qmltest_QML_FILES CONFIGURE_DEPENDS "test-qml/*.qml")
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

    # Add the cw.simple module before linking/importing plugins.
    add_subdirectory(test-qml)

    target_link_libraries(cavewhere-qml-test
        PRIVATE
        Qt6::Test
        Qt6::Quick
        Qt6::Widgets
        Qt6::Qml
        Qt6::QuickTest
        cavewhere_simplemodule
        cavewhere-testlib
        cavewherelib
        QuickQanavaplugin
    )

    # Ensure QML modules are imported without relying on QML2_IMPORT_PATH.
    qt_import_qml_plugins(cavewhere-qml-test)

    cw_bundle_proj_data(cavewhere-qml-test proj)
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

# Must be set before qt_finalize_executable — Qt processes the launch screen
# and Android manifest values during finalization.
if(MOBILE_BUILD)
    set_target_properties(CaveWhere PROPERTIES
        QT_IOS_LAUNCH_SCREEN "${CMAKE_SOURCE_DIR}/ios/LaunchScreen.storyboard"
    )
    if(ANDROID)
        # androiddeployqt substitutes these into AndroidManifest.xml.
        set_target_properties(CaveWhere PROPERTIES
            QT_ANDROID_VERSION_CODE "${CAVEWHERE_BUILD_NUMBER}"
            QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        )

        include(${CMAKE_SOURCE_DIR}/cmake/CavewhereAndroid.cmake)
        cavewhere_setup_android_openssl(CaveWhere)
    endif()
endif()

qt_finalize_executable(CaveWhere)

# Non-mac desktop: copy PROJ data to <exe_dir>/proj/. macOS handles it
# inside the APPLE block above (Contents/Resources/proj/).
if(NOT APPLE AND NOT ANDROID AND NOT IOS AND NOT MOBILE_BUILD)
    cw_bundle_proj_data(CaveWhere proj)
endif()

if(WIN32)
    include(${CMAKE_SOURCE_DIR}/cmake/CavewhereWindowsRuntime.cmake)
    cavewhere_setup_windows_runtime(CaveWhere)
endif()

if(APPLE)
    include(${CMAKE_SOURCE_DIR}/cmake/CavewhereAppleBundle.cmake)
    cavewhere_setup_apple_bundle(CaveWhere)
endif()


if((WIN32 OR APPLE OR UNIX) AND NOT CW_MOBILE)
	#### Building Installer for Windows, Mac, and Linux ####
#	set_target_properties(CaveWhere PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
#	set_target_properties(CaveWhere PROPERTIES SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    include(installer/installer.cmake)
endif()
