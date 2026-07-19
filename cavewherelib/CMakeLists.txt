cmake_minimum_required(VERSION 3.23)

include(${CMAKE_CURRENT_SOURCE_DIR}/UpdateVersion.cmake)

#### cavewherelib ######
# All packages we need to find
find_package(protobuf REQUIRED)
find_package(Catch2 REQUIRED)
find_package(Qt6
    COMPONENTS
    Core
    Gui
    GuiPrivate
    Widgets
    Quick
    QuickPrivate
    Sql
    Xml
    Concurrent
    Svg
    Test
    ShaderTools
    CanvasPainter
    REQUIRED)
find_package(xxHash REQUIRED)
find_package(TinyGLTF REQUIRED)
find_package(minizip REQUIRED)
find_package(PROJ CONFIG REQUIRED)
find_package(laslib CONFIG REQUIRED)

# Locate PROJ's runtime data directory (proj.db + grid-shift files). PROJ
# can't run without proj.db; conan's runenv only sets PROJ_DATA inside the
# dev shell, so for installed app bundles and IDE-launched test/app runs
# we need to bundle the data ourselves and point PROJ at it from C++
# (cwCoordinateTransform::setProjSearchPaths).
function(_cw_resolve_proj_data_dir out_var)
    set(${out_var} "" PARENT_SCOPE)

    set(_hints "")
    # Conan's CMakeDeps generator exposes <pkg>_PACKAGE_FOLDER_<CONFIG> /
    # <pkg>_RES_DIRS_<CONFIG>. Conan's PROJ recipe puts proj.db in res/.
    foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        if(DEFINED proj_RES_DIRS_${_cfg})
            list(APPEND _hints ${proj_RES_DIRS_${_cfg}})
        endif()
        if(DEFINED proj_PACKAGE_FOLDER_${_cfg})
            list(APPEND _hints
                "${proj_PACKAGE_FOLDER_${_cfg}}/res"
                "${proj_PACKAGE_FOLDER_${_cfg}}/share/proj")
        endif()
    endforeach()
    if(TARGET PROJ::proj)
        foreach(_cfg RELEASE DEBUG RELWITHDEBINFO MINSIZEREL "")
            set(_prop IMPORTED_LOCATION)
            if(NOT _cfg STREQUAL "")
                set(_prop IMPORTED_LOCATION_${_cfg})
            endif()
            get_target_property(_loc PROJ::proj ${_prop})
            if(_loc AND NOT _loc STREQUAL "_loc-NOTFOUND")
                get_filename_component(_libdir "${_loc}" DIRECTORY)
                list(APPEND _hints
                    "${_libdir}/../res"
                    "${_libdir}/../share/proj")
            endif()
        endforeach()
    endif()
    if(_hints)
        list(REMOVE_DUPLICATES _hints)
    endif()

    foreach(_h ${_hints})
        if(EXISTS "${_h}/proj.db")
            set(${out_var} "${_h}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

_cw_resolve_proj_data_dir(_cw_proj_data_dir)
set(CW_PROJ_DATA_DIR "${_cw_proj_data_dir}" CACHE PATH
    "Source directory containing PROJ runtime data (proj.db, grid shifts)." FORCE)
if(CW_PROJ_DATA_DIR)
    message(STATUS "CaveWhere: PROJ data dir: ${CW_PROJ_DATA_DIR}")
else()
    message(WARNING
        "CaveWhere: could not locate PROJ data dir (proj.db). Coordinate "
        "transforms will fail at runtime unless PROJ_DATA is set in the "
        "environment.")
endif()

# Copies PROJ's runtime data dir (proj.db, grid-shift files, schemas) into
# `<dest_subdir>` next to or inside `target` so cwCoordinateTransform's
# resolution in cwGlobals::projDataPath() finds it. dest_subdir is appended
# to $<TARGET_FILE_DIR:target>, e.g. "proj" → "<exe_dir>/proj/proj.db".
# The copy is idempotent (BundleProjData.cmake skips when proj.db is up to
# date) so 8.4 MB of files don't re-copy on every relink.
set(CW_BUNDLE_PROJ_DATA_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/BundleProjData.cmake"
    CACHE INTERNAL "Path to BundleProjData.cmake helper")
function(cw_bundle_proj_data target dest_subdir)
    if(NOT CW_PROJ_DATA_DIR)
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DSRC=${CW_PROJ_DATA_DIR}
            -DDST=$<TARGET_FILE_DIR:${target}>/${dest_subdir}
            -P ${CW_BUNDLE_PROJ_DATA_SCRIPT}
        COMMENT "Bundling PROJ data into $<TARGET_FILE_DIR:${target}>/${dest_subdir}"
        VERBATIM)
endfunction()

set(BUILD_WITH_QT6 ON CACHE BOOL "Build qtkeychain with Qt 6" FORCE)
set(BUILD_TRANSLATIONS OFF CACHE BOOL "Disable qtkeychain translations" FORCE)
set(BUILD_TRANSLATIONS_AS_RESOURCES OFF CACHE BOOL "" FORCE)
set(BUILD_TEST_APPLICATION OFF CACHE BOOL "" FORCE)

set(cw_restore_build_shared_libs OFF)
if(DEFINED BUILD_SHARED_LIBS)
    set(cw_restore_build_shared_libs ON)
    set(cw_prev_build_shared_libs ${BUILD_SHARED_LIBS})
endif()

set(cw_restore_pic OFF)
if(DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
    set(cw_restore_pic ON)
    set(cw_prev_pic ${CMAKE_POSITION_INDEPENDENT_CODE})
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build qtkeychain as static library" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Build qtkeychain with -fPIC" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/qtkeychain ${CMAKE_BINARY_DIR}/qtkeychain_build)

if(cw_restore_build_shared_libs)
    set(BUILD_SHARED_LIBS ${cw_prev_build_shared_libs} CACHE BOOL "Build dynamic library" FORCE)
else()
    unset(BUILD_SHARED_LIBS CACHE)
endif()

if(cw_restore_pic)
    set(CMAKE_POSITION_INDEPENDENT_CODE ${cw_prev_pic} CACHE BOOL "Restore PIC setting" FORCE)
else()
    unset(CMAKE_POSITION_INDEPENDENT_CODE CACHE)
endif()

if(TARGET qt6keychain)
    set_target_properties(qt6keychain PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(MOBILE_BUILD)
    qt_add_library(cavewherelib STATIC)
else()
    qt_add_library(cavewherelib SHARED)
endif()

#RESOURCE_PREFIX defaults to /qt/qml/ which is the default import path of the QML engine
# qmake_policy(SET QTP0001 NEW)

set_target_properties(cavewherelib PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

if(MSVC)
    target_compile_options(cavewherelib PRIVATE
        /FI${CMAKE_CURRENT_BINARY_DIR}/CaveWhereLibExport.h
        /bigobj
    )
endif()

target_link_libraries(cavewherelib
    PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::Sql
    Qt6::Xml
    Qt6::Concurrent
    Qt6::Svg
    Qt6::GuiPrivate
    Qt6::QuickPrivate
    Qt6::CanvasPainter
    asyncfuture
    dewalls
    QMath3d
    MarkScope
    monad
    QuickQanava
    qquickgit
    qt6keychain
    PRIVATE
    TinyGLTF::TinyGLTF
    protobuf::libprotobuf
    abseil::abseil
    xxHash::xxhash
    MINIZIP::minizip
    surveximg
    cavernlib
    PROJ::proj
    laslib::laslib
)

cw_link_absl_static(cavewherelib)

# Conditionally find the package and include directories
find_package(Qt6 COMPONENTS Pdf QUIET)
if(TARGET Qt6::Pdf)
    message(STATUS "Qt Pdf supported")
    target_link_libraries(cavewherelib PUBLIC Qt6::Pdf)
    target_compile_definitions(cavewherelib PUBLIC CW_WITH_PDF_SUPPORT)
else()
    message(STATUS "Qt Pdf not supported")
endif()

qt_add_resources(QT_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cwResources.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/fonts/cwFonts.qrc
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

target_include_directories(cavewherelib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/versionInfo
)

# Collect source files cavewhereLib
file(GLOB cavewherelib_CPP_FILES CONFIGURE_DEPENDS "src/*.cpp" "src/utils/*.cpp" "src/rendering/*.cpp")
file(GLOB cavewherelib_H_FILES CONFIGURE_DEPENDS "src/*.h" "src/utils/*.h" "src/rendering/*.h")

if(IOS)
    list(APPEND cavewherelib_CPP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/cwClipboard_ios.mm")
endif()

# file(GLOB cavewhere_QML_FILES "qml/*.qml" "qml/*.js")
# target_sources(CaveWhere PRIVATE ${cavewhere_QML_FILES})

# Make the file paths relative to the project directory
set(cavewhere_RELATIVE_CPP_FILES "")
foreach(file ${cavewherelib_CPP_FILES})
    file(RELATIVE_PATH relFile "${CMAKE_CURRENT_SOURCE_DIR}" "${file}")
    list(APPEND cavewhere_RELATIVE_CPP_FILES "${relFile}")
endforeach()

set(cavewhere_RELATIVE_H_FILES "")
foreach(file ${cavewherelib_H_FILES})
    file(RELATIVE_PATH relFile "${CMAKE_CURRENT_SOURCE_DIR}" "${file}")
    list(APPEND cavewhere_RELATIVE_H_FILES "${relFile}")
endforeach()

# Get all QML and JS files
file(GLOB cavewhere_QML_FILES CONFIGURE_DEPENDS "qml/*.qml" "qml/*.js")

# Define the list of QML singletons
set(qml_singletons
    "qml/GlobalShadowTextInput.qml"
    "qml/RootPopupItem.qml"
	"qml/Theme.qml"
    "qml/SelectableValueGroup.qml"
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

# JS files are only used via relative `import "X.js" as X` imports, never as
# module-level scripts. Skip the qmldir entry so the Android QML engine
# doesn't try to resolve them through the android_rcc_bundle path (the module
# is statically linked, so the bundle has no copy).
set(cavewhere_JS_FILES "")
foreach(file ${cavewhere_RELATIVE_QML_FILES})
    if(file MATCHES "\\.js$")
        list(APPEND cavewhere_JS_FILES "${file}")
    endif()
endforeach()
if(cavewhere_JS_FILES)
    set_source_files_properties(${cavewhere_JS_FILES} PROPERTIES QT_QML_SKIP_QMLDIR_ENTRY TRUE)
endif()

# Add QML module with the regular QML files and the singleton files
# if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
#     # macOS
#     set(cavewherelib_ouput_dir "${CMAKE_BINARY_DIR}/CaveWhere.app/Contents/MacOS/cavewherelib")
# else()
#     # Other systems (e.g., Linux, Windows)
#     set(cavewherelib_ouput_dir "${CMAKE_BINARY_DIR}/cavewherelib")
# endif()

set(cavewherelib_qml_cachegen_args "")
if(NOT ANDROID AND NOT IOS)
    if(CMAKE_CONFIGURATION_TYPES)
        set(cavewherelib_qml_cachegen_args "$<$<CONFIG:Debug>:NO_CACHEGEN>")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(cavewherelib_qml_cachegen_args "NO_CACHEGEN")
    endif()
endif()

qt_add_qml_module(cavewherelib
    URI cavewherelib
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml/
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/cavewherelib
    ${cavewherelib_qml_cachegen_args}
    DEPENDENCIES
        QtQuick
        QQuickGit
        QuickQanava
    SOURCES
        ${cavewhere_RELATIVE_CPP_FILES}
        ${cavewhere_RELATIVE_H_FILES}
    QML_FILES
        ${cavewhere_RELATIVE_QML_FILES}
        ${qml_singletons}

)

target_sources(cavewherelib PRIVATE ${cavewherelib_CPP_FILES} ${cavewherelib_H_FILES})
qt_import_qml_plugins(cavewherelib)

# Handle shaders
qt_add_shaders(cavewherelib "cavewherelib_shaders"
    PREFIX "/"
    FILES
        "shaders/grid.frag"
        "shaders/grid.vert"
        "shaders/PointCloud.frag"
        "shaders/PointCloud.vert"
        "shaders/EDL.vert"
        "shaders/EDL.frag"
        "shaders/scrap.vert"
        "shaders/scrap.frag"
        "shaders/radial_gradient.vert"
        "shaders/radial_gradient.frag"
        "shaders/compass/compass.vert"
        "shaders/compass/compass.frag"
        "shaders/toColor.frag"
        "shaders/unlit.frag"
        "shaders/unlit.vert"
    )

# LinePlot reads a per-vertex unsigned-integer visibility attribute in the
# vertex shader, which the legacy 120 / 100es targets reject (integer vertex
# inputs need GLSL 150 / GLSL ES 300). Target those plus the usual HLSL SM5 /
# MSL 1.2; Vulkan SPIR-V is always emitted.
qt_add_shaders(cavewherelib "cavewherelib_shaders_lineplot"
    PREFIX "/"
    GLSL "150,300es"
    HLSL 50
    MSL 12
    FILES
        "shaders/LinePlot.frag"
        "shaders/LinePlot.vert"
    )

# Second EDL.frag variant: same source, compiled with MSAA=1 so it samples the
# multisample offscreen textures per-sample. Aliased to a distinct .qsb name
# (:/shaders/EDL_MSAA.frag.qsb) that cwEDLEffect loads when the offscreen is MSAA.
# gl_SampleID needs sample-variable support, so target GLSL 410 / GLSL ES 320
# (the legacy 120/150/100es targets reject it) and shader models that have it
# (HLSL SM5, MSL 1.2); Vulkan SPIR-V is always emitted.
qt_add_shaders(cavewherelib "cavewherelib_shaders_msaa"
    PREFIX "/"
    GLSL "410,320es"
    HLSL 50
    MSL 12
    DEFINES "MSAA=1"
    FILES
        "shaders/EDL.frag"
    OUTPUTS
        "shaders/EDL_MSAA.frag.qsb"
    )

#This is a hack to find the conan protoc and not the system protoc
#If homebrew is installed on macos conan finds the homebrew version even though
#protoc is in the build.requirements. https://github.com/conan-io/conan-center-index/issues/21737
# Get the current IMPORTED_LOCATION of protobuf::protoc
get_target_property(CURRENT_PROTOC_EXECUTABLE protobuf::protoc IMPORTED_LOCATION)

# Check if the path contains ".conan2"
message(STATUS "CMAKE_PROGRAM_PATH:" ${CMAKE_PROGRAM_PATH} )
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
if(WIN32)
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS EXPORT_MACRO CAVEWHERE_LIB_EXPORT ${protobuf_FILES})
else()
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${protobuf_FILES})
endif()
target_sources(cavewherelib PRIVATE ${protobuf_FILES} ${protobuf_FILES})

# Add the generated files to your target
set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS} PROPERTIES SKIP_AUTOMOC TRUE SKIP_AUTOUIC TRUE)
target_sources(cavewherelib PRIVATE ${PROTO_SRCS} ${PROTO_HDRS})

# Handle .ui files
qt_wrap_ui(UI_HEADERS
    cwImportTreeDataDialog.ui
    cwTaskProgressDialog.ui
)

target_compile_definitions(cavewherelib
    PRIVATE "CAVEWHERE_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\""
)

target_compile_definitions(cavewherelib
    PUBLIC
        $<$<CONFIG:Release,RelWithDebInfo,MinSizeRel>:QT_NO_DEBUG_OUTPUT>
        $<$<CONFIG:Release,RelWithDebInfo,MinSizeRel>:QT_NO_INFO_OUTPUT>
)

if(MOBILE_BUILD)
    message(STATUS "Building as a mobile build")
    target_compile_definitions(cavewherelib
        PRIVATE "CW_MOBILE_BUILD"
    )
else()
    message(STATUS "Building as a desktop build")
endif()


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
