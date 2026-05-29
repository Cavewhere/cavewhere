# Linux AppImage / linuxdeploy ExternalProject setup for CaveWhere.
#
# Exposes:
#
#   cavewhere_setup_linux_appimage(<target>)
#       Configures three ExternalProjects (linuxdeploy, linuxdeploy-plugin-qt,
#       linuxdeploy-plugin-appimage) and conditionally a fourth for
#       appimagetool. Resolves PATCHELF + APPIMAGETOOL_EXECUTABLE from PATH
#       or from a built copy, and stashes the resulting binary paths as
#       cache variables for installer/installer.cmake to consume.
#
# The target argument is documentation only — this block doesn't tie its
# ExternalProjects to a specific application target.

include(ExternalProject)

function(cavewhere_setup_linux_appimage target)
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
        -DLINUXDEPLOY_ENABLE_CIMG=OFF
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
endfunction()
