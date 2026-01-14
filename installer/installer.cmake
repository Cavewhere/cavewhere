set(CAVEWHERE_NAME "CaveWhere")
set(LINUXDEPLOY_EXECUTABLE "" CACHE FILEPATH "Path to linuxdeploy")
set(LINUXDEPLOY_QT_PLUGIN_EXECUTABLE "" CACHE FILEPATH "Path to linuxdeploy-plugin-qt")
set(LINUXDEPLOY_APPIMAGE_PLUGIN_EXECUTABLE "" CACHE FILEPATH "Path to linuxdeploy-plugin-appimage")
set(QMAKE_EXECUTABLE "" CACHE FILEPATH "Path to qmake")
set(APPIMAGETOOL_EXECUTABLE "" CACHE FILEPATH "Path to appimagetool")
set(APPIMAGE_RUNTIME_FILE "" CACHE FILEPATH "Path to AppImage runtime (optional)")

if(WIN32)
    # Determine the target OS
    find_package(Qt6 COMPONENTS Core)
    get_target_property(QtCore_location Qt6::Core LOCATION)
    get_filename_component(Qt_bin_dir ${QtCore_location} DIRECTORY)

    #set(conanbuildinfo ${CMAKE_BINARY_DIR}/conan-dependencies/conanbuildinfo.cmake)
    #include(${conanbuildinfo})

    # If Qt_bin_dir is empty
    if(NOT Qt_bin_dir)
        set(Qt_bin_dir ${CONAN_BIN_DIRS_QT})
    endif()

    # if Qt_bin_dir doesn't exist
    if(NOT Qt_bin_dir)
        message(FATAL_ERROR "Qt bin dir not found")
    endif()
    set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")

    include(InstallRequiredSystemLibraries)

    get_target_property(BINARY_DIR CaveWhere RUNTIME_OUTPUT_DIRECTORY)
    set(CAVEWHER_BINARY_PATH "${DEPLOY_DIR}/CaveWhere${CMAKE_EXECUTABLE_SUFFIX}")
    get_target_property(CAVEWHERE_SOURCE_DIR CaveWhere SOURCE_DIR)

    # Define QML source directories for linuxdeploy-plugin-qt scanning.
    set(QML_DIR "${CMAKE_BINARY_DIR}/cavewherelib")
    set(QML_SOURCE_DIRS
        "${CMAKE_SOURCE_DIR}/cavewherelib/qml"
        "${CMAKE_SOURCE_DIR}/QQuickGit/qml"
        "${CMAKE_SOURCE_DIR}/sketch/qml"
    )
    string(JOIN ":" QML_SOURCES_PATHS ${QML_SOURCE_DIRS})
    set(DEPLOY_SURVEX_DIR "${DEPLOY_DIR}/survex")
    set(SURVEX_BIN_DIR "${BINARY_DIR}/survex")

    find_package(OpenSSL REQUIRED)

    # Define the list of files to copy
    set(ROOT_FILES_TO_COPY
        "${BINARY_DIR}/${CAVEWHERE_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
        #"${CONAN_BIN_DIRS_ZLIB}/zlib1.dll"
    )

    set(_openssl_bin_dirs "")
    if(DEFINED openssl_BIN_DIRS_RELEASE)
        set(_openssl_bin_dirs ${openssl_BIN_DIRS_RELEASE})
    elseif(DEFINED openssl_BIN_DIRS)
        set(_openssl_bin_dirs ${openssl_BIN_DIRS})
    endif()

    foreach(_dir IN LISTS _openssl_bin_dirs)
        file(GLOB _openssl_dlls
            "${_dir}/libssl-*.dll"
            "${_dir}/libcrypto-*.dll"
        )
        list(APPEND ROOT_FILES_TO_COPY ${_openssl_dlls})
    endforeach()

    # Append shared library outputs only when they exist at build time.
    set(_SHARED_LIB_TARGETS
        cavewherelib
        cavewhere-testlib
        dewalls
        QMath3d
        QmlTestRecorder
        qt6keychain
    )

    foreach(_lib IN LISTS _SHARED_LIB_TARGETS)
        if(TARGET ${_lib})
            get_target_property(_lib_type ${_lib} TYPE)
            if(_lib_type STREQUAL "SHARED")
                list(APPEND ROOT_FILES_TO_COPY "$<TARGET_FILE:${_lib}>")
            endif()
        endif()
    endforeach()

    set(PLUGIN_DIR $<TARGET_FILE_DIR:cavewherelibplugin>)
    SET(cavewherelib_FILES_TO_COPY
        "${PLUGIN_DIR}/qmldir"
        "${PLUGIN_DIR}/cavewherelib.qmltypes"
    )

    if(TARGET cavewherelibplugin)
        get_target_property(_cavewherelibplugin_TYPE cavewherelibplugin TYPE)
        if(_cavewherelibplugin_TYPE STREQUAL "SHARED")
            list(APPEND cavewherelib_FILES_TO_COPY "${PLUGIN_DIR}/cavewherelibplugin${CMAKE_SHARED_LIBRARY_SUFFIX}")
        endif()
    endif()


    if(WITH_PDF)
        list(APPEND ROOT_FILES_TO_COPY "${Qt_bin_dir}/Qt5Pdf${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif()

    set(QML_FILES_TO_COPY ${cavewhere_QML_FILES})

    file(GLOB survex_dlls "${SURVEX_BIN_DIR}/*.dll")

    set(survex_files_to_copy
        "${SURVEX_BIN_DIR}/cavern${CMAKE_EXECUTABLE_SUFFIX}"
        "${SURVEX_BIN_DIR}/survexport${CMAKE_EXECUTABLE_SUFFIX}"
    )

    set(DEPOLY_COPY_TIMESTAMP "${CMAKE_CURRENT_BINARY_DIR}/deploy-copy-timestamp.txt")

    # Create a custom command to copy the files
    add_custom_command(
        OUTPUT ${DEPOLY_COPY_TIMESTAMP}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${DEPLOY_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPLOY_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${ROOT_FILES_TO_COPY} ${DEPLOY_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPLOY_DIR}/cavewherelib
        COMMAND ${CMAKE_COMMAND} -E copy ${cavewherelib_FILES_TO_COPY} ${DEPLOY_DIR}/cavewherelib
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPLOY_DIR}/cavewherelib/qml
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PLUGIN_DIR}/qml ${DEPLOY_DIR}/cavewherelib/qml
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DEPLOY_SURVEX_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${survex_files_to_copy} ${DEPLOY_SURVEX_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${SVX_MESSAGE_FILES} ${DEPLOY_SURVEX_DIR}
        #Copy to the deploy directory
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} ${DEPLOY_DIR}
        #Also copy to the survex director
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} ${DEPLOY_SURVEX_DIR}
        COMMENT "Copying files to deploy directory"
        VERBATIM
    )

    # Create a custom target that depends on the copied files
    add_custom_target(prepare_deploy
        DEPENDS
        ${DEPOLY_COPY_TIMESTAMP}
        CaveWhere
        cavern
        survexport
        #cavewhere-test #This makes the release much bigger
        #cavewhere-qml-test #This makes the release much bigger
    )

    set(DEPLOYMENT_APP "${Qt_bin_dir}/windeployqt.exe")
    set(DEPLOYMENT_ARGS
        "--release"
        "--no-compiler-runtime" #we're deploying windows with vcruntime directly through cmake
        "--qmldir" ${QML_DIR}
        "-sql"
        "-xml"
        "-concurrent"
        "-test"
        "${CAVEWHER_BINARY_PATH}")

    # Custom command to run deployment app
    add_custom_command(OUTPUT deploy_timestamp.txt
       COMMAND ${DEPLOYMENT_APP} ${DEPLOYMENT_ARGS}
       #COMMAND ${CMAKE_COMMAND} -E touch deploy_timestamp.txt
       COMMENT "Running ${DEPLOYMENT_APP}"
       VERBATIM)

    # Custom target to trigger the custom command
    add_custom_target(deployCavewhere
        DEPENDS deploy_timestamp.txt)

    ## Make sure your main target depends on this custom target
    add_dependencies(deployCavewhere prepare_deploy)

    ## Make inno install

  #  include("GitHash.cmake")
  #  get_hash("${BINARY_DIR}/current_git_hash.txt" CAVEWHERE_VERSION)
  #  message(STATUS "Current git hash: ${CAVEWHERE_VERSION}")

    # Determine the architecture
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        message(FATAL_ERROR "ARM architectures are not supported for windows installers.")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64-bit
        set(arch "64bit")
        set(architecturesAllowed "x64compatible")
        set(architecturesInstallIn64BitMode "x64compatible")
        set(redist_exe "vc_redist.x64.exe")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4) # 32-bit
        message(FATAL_ERROR "32-bit mode is not supported for windows installers.")
    else()
        message(FATAL_ERROR "Unknown architecture is not supported for windows installers.")
    endif()


    # Add custom command to run InnoConfigure.cmake and generate cavewhere.iss
    set(redist_version "v14.42.34433") #This can be queried by double clicking on the vc_redist.x64.exe
    set(inno_in_file "${CMAKE_SOURCE_DIR}/installer/windows/cavewhere.iss.in")

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/cavewhere.iss
        COMMAND ${CMAKE_COMMAND}
        -DISS_IN=${inno_in_file}
        -DISS=${CMAKE_BINARY_DIR}/cavewhere.iss
        -DGET_HASH_CMAKE=${CMAKE_CURRENT_SOURCE_DIR}/cavewherelib/GitHash.cmake
        -DCAVEWHERE_VERSION_FILE=${BINARY_DIR}/cavewherelib/current_git_hash.txt
        -DCAVEWHERE_NAME=${CAVEWHERE_NAME}
        -DARCH_ALLOWED=${architecturesAllowed}
        -DINSTALL_64BIT=${architecturesInstallIn64BitMode}
        -DARCH=${arch}
        -DREDISTRIBUTABLE_EXE=${redist_exe}
        -DREDISTRIBUTABLE_VERSION=${redist_version}
        -DDEPLOY_DIR=${DEPLOY_DIR}
        -P ${CMAKE_SOURCE_DIR}/installer/windows/InnoConfigure.cmake
        DEPENDS ${inno_in_file}
        COMMENT "Generating cavewhere.iss using InnoConfigure.cmake"
        VERBATIM
    )

    set(cavewhere_installer_name "Cavewhere ${CAVEWHERE_VERSION} ${arch}.exe")

    # Custom command to run Inno Setup compiler
    add_custom_command(OUTPUT always_build_installer #"${cavewhere_installer_name}"
        COMMAND "C:/Program Files (x86)/Inno Setup 6/ISCC.exe" ${CMAKE_BINARY_DIR}/cavewhere.iss
        DEPENDS cavewhere.iss
        COMMENT "Running Inno Setup compiler"
        VERBATIM)

    # Custom target to trigger the custom command
    add_custom_target(windowsInstaller
        DEPENDS always_build_installer) #"${cavewhere_installer_name}")

    # Make sure your main target depends on this custom target
    add_dependencies(windowsInstaller deployCavewhere)

endif()

if(UNIX AND NOT APPLE)
    set(APPIMAGE_ROOT_DIR "${CMAKE_BINARY_DIR}/appimage")
    set(APPIMAGE_APPDIR "${APPIMAGE_ROOT_DIR}/AppDir")
    set(APPIMAGE_USR_DIR "${APPIMAGE_APPDIR}/usr")
    set(APPIMAGE_BIN_DIR "${APPIMAGE_USR_DIR}/bin")
    set(APPIMAGE_LIB_DIR "${APPIMAGE_USR_DIR}/lib")
    set(APPIMAGE_SHARE_DIR "${APPIMAGE_USR_DIR}/share")
    set(APPIMAGE_APPLICATIONS_DIR "${APPIMAGE_SHARE_DIR}/applications")
    set(APPIMAGE_QML_DIR "${APPIMAGE_USR_DIR}/qml")
    set(APPIMAGE_ICONS_DIR "${APPIMAGE_SHARE_DIR}/icons/hicolor/512x512/apps")
    set(APPIMAGE_DESKTOP_IN "${CMAKE_SOURCE_DIR}/installer/linux/CaveWhere.desktop.in")
    set(APPIMAGE_APPRUN_IN "${CMAKE_SOURCE_DIR}/installer/linux/AppRun.in")
    set(APPIMAGE_DESKTOP_FILE "${APPIMAGE_ROOT_DIR}/CaveWhere.desktop")
    set(APPIMAGE_APPRUN_FILE "${APPIMAGE_ROOT_DIR}/AppRun")
    set(APPIMAGE_ICON_SOURCE "${CMAKE_SOURCE_DIR}/cavewherelib/icons/cave512x512.png")
    set(APPIMAGE_ICON_TARGET "${APPIMAGE_ICONS_DIR}/CaveWhere.png")
    set(APPIMAGE_STAGE_STAMP "${APPIMAGE_ROOT_DIR}/appimage-stage.stamp")

    set(QML_SOURCE_DIRS
        "${CMAKE_SOURCE_DIR}/cavewherelib/qml"
        "${CMAKE_SOURCE_DIR}/QQuickGit/qml"
        "${CMAKE_SOURCE_DIR}/sketch/qml"
    )
    string(JOIN ":" QML_SOURCES_PATHS ${QML_SOURCE_DIRS})

    if(EXISTS "${CMAKE_BINARY_DIR}/cavewherelib/current_git_hash.txt")
        include("${CMAKE_SOURCE_DIR}/cavewherelib/GitHash.cmake")
        get_hash("${CMAKE_BINARY_DIR}/cavewherelib/current_git_hash.txt" CAVEWHERE_VERSION)
    else()
        set(CAVEWHERE_VERSION "${PROJECT_VERSION}")
    endif()

    configure_file("${APPIMAGE_DESKTOP_IN}" "${APPIMAGE_DESKTOP_FILE}" @ONLY)
    configure_file("${APPIMAGE_APPRUN_IN}" "${APPIMAGE_APPRUN_FILE}" @ONLY)

    if(NOT EXISTS "${APPIMAGE_ICON_SOURCE}")
        message(FATAL_ERROR "AppImage icon not found: ${APPIMAGE_ICON_SOURCE}")
    endif()

    if(TARGET linuxdeploy)
        set(LINUXDEPLOY_EXECUTABLE "$<TARGET_FILE:linuxdeploy>")
    elseif(NOT LINUXDEPLOY_EXECUTABLE)
        find_program(LINUXDEPLOY_EXECUTABLE linuxdeploy)
    endif()

    if(TARGET linuxdeploy-plugin-qt)
        set(LINUXDEPLOY_QT_PLUGIN_EXECUTABLE "$<TARGET_FILE:linuxdeploy-plugin-qt>")
    elseif(NOT LINUXDEPLOY_QT_PLUGIN_EXECUTABLE)
        find_program(LINUXDEPLOY_QT_PLUGIN_EXECUTABLE linuxdeploy-plugin-qt)
    endif()

    if(TARGET linuxdeploy-plugin-appimage)
        set(LINUXDEPLOY_APPIMAGE_PLUGIN_EXECUTABLE "$<TARGET_FILE:linuxdeploy-plugin-appimage>")
    elseif(NOT LINUXDEPLOY_APPIMAGE_PLUGIN_EXECUTABLE)
        find_program(LINUXDEPLOY_APPIMAGE_PLUGIN_EXECUTABLE linuxdeploy-plugin-appimage)
    endif()

    if(NOT LINUXDEPLOY_EXECUTABLE)
        message(FATAL_ERROR "linuxdeploy not found; set LINUXDEPLOY_EXECUTABLE or add it to PATH.")
    endif()

    if(NOT LINUXDEPLOY_QT_PLUGIN_EXECUTABLE)
        message(FATAL_ERROR "linuxdeploy-plugin-qt not found; set LINUXDEPLOY_QT_PLUGIN_EXECUTABLE or add it to PATH.")
    endif()

    if(NOT LINUXDEPLOY_APPIMAGE_PLUGIN_EXECUTABLE)
        message(FATAL_ERROR "linuxdeploy-plugin-appimage not found; set LINUXDEPLOY_APPIMAGE_PLUGIN_EXECUTABLE or add it to PATH.")
    endif()

    if(NOT QMAKE_EXECUTABLE AND TARGET Qt6::Core)
        get_target_property(QtCore_location Qt6::Core IMPORTED_LOCATION_RELEASE)
        if(NOT QtCore_location)
            get_target_property(QtCore_location Qt6::Core IMPORTED_LOCATION)
        endif()
        if(QtCore_location)
            get_filename_component(Qt_lib_dir "${QtCore_location}" DIRECTORY)
            get_filename_component(Qt_prefix "${Qt_lib_dir}" DIRECTORY)
            if(EXISTS "${Qt_prefix}/bin/qmake")
                set(QMAKE_EXECUTABLE "${Qt_prefix}/bin/qmake")
            elseif(EXISTS "${Qt_prefix}/bin/qmake6")
                set(QMAKE_EXECUTABLE "${Qt_prefix}/bin/qmake6")
            endif()
        endif()
    endif()

    if(NOT QMAKE_EXECUTABLE)
        if(DEFINED qt_Qt6_Core_BIN_DIRS_RELEASE)
            set(Qt_bin_dir "${qt_Qt6_Core_BIN_DIRS_RELEASE}")
        elseif(DEFINED qt_Qt6_Core_BIN_DIRS)
            set(Qt_bin_dir "${qt_Qt6_Core_BIN_DIRS}")
        else()
            set(Qt_bin_dir "")
        endif()

        if(Qt_bin_dir AND EXISTS "${Qt_bin_dir}/qmake")
            set(QMAKE_EXECUTABLE "${Qt_bin_dir}/qmake")
        elseif(Qt_bin_dir AND EXISTS "${Qt_bin_dir}/qmake6")
            set(QMAKE_EXECUTABLE "${Qt_bin_dir}/qmake6")
        endif()
    endif()

    if(NOT QMAKE_EXECUTABLE)
        find_program(QMAKE_EXECUTABLE NAMES qmake qmake6)
    endif()

    set(QMAKE_ENV "")
    set(QT_LIBEXECS_DIR "")
    if(QMAKE_EXECUTABLE)
        set(QMAKE_ENV "QMAKE=${QMAKE_EXECUTABLE}")
        execute_process(
            COMMAND "${QMAKE_EXECUTABLE}" -query QT_INSTALL_LIBEXECS
            OUTPUT_VARIABLE QT_LIBEXECS_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND "${QMAKE_EXECUTABLE}" -query QT_INSTALL_QML
            OUTPUT_VARIABLE QT_INSTALL_QML_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    set(LINUXDEPLOY_QT_PLUGIN_DIR "")
    set(LINUXDEPLOY_APPIMAGE_PLUGIN_DIR "")
    set(APPIMAGETOOL_DIR "")

    if(TARGET linuxdeploy-plugin-qt)
        set(LINUXDEPLOY_QT_PLUGIN_DIR "$<TARGET_FILE_DIR:linuxdeploy-plugin-qt>")
    else()
        get_filename_component(LINUXDEPLOY_QT_PLUGIN_DIR "${LINUXDEPLOY_QT_PLUGIN_EXECUTABLE}" DIRECTORY)
    endif()

    if(TARGET linuxdeploy-plugin-appimage)
        set(LINUXDEPLOY_APPIMAGE_PLUGIN_DIR "$<TARGET_FILE_DIR:linuxdeploy-plugin-appimage>")
    else()
        get_filename_component(LINUXDEPLOY_APPIMAGE_PLUGIN_DIR "${LINUXDEPLOY_APPIMAGE_PLUGIN_EXECUTABLE}" DIRECTORY)
    endif()

    if(APPIMAGETOOL_EXECUTABLE)
        get_filename_component(APPIMAGETOOL_DIR "${APPIMAGETOOL_EXECUTABLE}" DIRECTORY)
    endif()

    set(LINUXDEPLOY_ENV_PATH "${LINUXDEPLOY_QT_PLUGIN_DIR}:${LINUXDEPLOY_APPIMAGE_PLUGIN_DIR}:${APPIMAGETOOL_DIR}:${QT_LIBEXECS_DIR}:$ENV{PATH}")

    # Define the destination directory
    set(QML_DIR "${CMAKE_BINARY_DIR}/cavewherelib")

    set(APPIMAGE_QML_COPY_COMMANDS "")
    if(QT_INSTALL_QML_DIR)
        list(APPEND APPIMAGE_QML_COPY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${QT_INSTALL_QML_DIR}" "${APPIMAGE_QML_DIR}"
        )
    endif()

    add_custom_command(
        OUTPUT ${APPIMAGE_STAGE_STAMP}
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${APPIMAGE_APPDIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${APPIMAGE_BIN_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${APPIMAGE_LIB_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${APPIMAGE_APPLICATIONS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${APPIMAGE_QML_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${APPIMAGE_ICONS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:CaveWhere>" "${APPIMAGE_BIN_DIR}/CaveWhere"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:cavern>" "${APPIMAGE_BIN_DIR}/cavern"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:survexport>" "${APPIMAGE_BIN_DIR}/survexport"
        COMMAND ${CMAKE_COMMAND} -E copy ${SVX_MESSAGE_FILES} "${APPIMAGE_LIB_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "${APPIMAGE_DESKTOP_FILE}" "${APPIMAGE_APPLICATIONS_DIR}/CaveWhere.desktop"
        COMMAND ${CMAKE_COMMAND} -E copy "${APPIMAGE_APPRUN_FILE}" "${APPIMAGE_APPDIR}/AppRun"
        COMMAND ${CMAKE_COMMAND} -E copy "${APPIMAGE_ICON_SOURCE}" "${APPIMAGE_ICON_TARGET}"
        ${APPIMAGE_QML_COPY_COMMANDS}
        COMMAND ${CMAKE_COMMAND} -E env /bin/sh -c "chmod +x \"${APPIMAGE_APPDIR}/AppRun\""
        COMMAND ${CMAKE_COMMAND} -E touch "${APPIMAGE_STAGE_STAMP}"
        DEPENDS
            CaveWhere
            cavern
            survexport
            ${SVX_MESSAGE_FILES}
            messageFiles
            "${APPIMAGE_DESKTOP_FILE}"
            "${APPIMAGE_APPRUN_FILE}"
        COMMENT "Staging AppDir for AppImage"
        VERBATIM
    )

    set(APPIMAGE_DEPENDENCIES ${APPIMAGE_STAGE_STAMP})

    if(TARGET linuxdeploy_ep)
        list(APPEND APPIMAGE_DEPENDENCIES linuxdeploy_ep)
    endif()
    if(TARGET linuxdeploy_plugin_qt_ep)
        list(APPEND APPIMAGE_DEPENDENCIES linuxdeploy_plugin_qt_ep)
    endif()
    if(TARGET linuxdeploy_plugin_appimage_ep)
        list(APPEND APPIMAGE_DEPENDENCIES linuxdeploy_plugin_appimage_ep)
    endif()
    if(TARGET appimagetool_ep)
        list(APPEND APPIMAGE_DEPENDENCIES appimagetool_ep)
    endif()

    set(APPIMAGE_OUTPUT_NAME "CaveWhere-${CAVEWHERE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}.AppImage")
    set(APPIMAGE_RUNTIME_ENV "LDAI_OUTPUT=${APPIMAGE_OUTPUT_NAME}")
    if(APPIMAGE_RUNTIME_FILE)
        list(APPEND APPIMAGE_RUNTIME_ENV "LDAI_RUNTIME_FILE=${APPIMAGE_RUNTIME_FILE}")
    endif()

    add_custom_target(appimageInstaller
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/installer/linux/CheckPatchelf.cmake"
        COMMAND ${CMAKE_COMMAND} -D QMAKE_EXECUTABLE="${QMAKE_EXECUTABLE}" -P "${CMAKE_SOURCE_DIR}/installer/linux/CheckQmake.cmake"
        COMMAND ${CMAKE_COMMAND} -E env "PATH=${LINUXDEPLOY_ENV_PATH}" ${QMAKE_ENV}
            "${LINUXDEPLOY_EXECUTABLE}"
            --appdir "${APPIMAGE_APPDIR}"
            --executable "${APPIMAGE_BIN_DIR}/CaveWhere"
            --executable "${APPIMAGE_BIN_DIR}/cavern"
            --executable "${APPIMAGE_BIN_DIR}/survexport"
            --desktop-file "${APPIMAGE_DESKTOP_FILE}"
            --icon-file "${APPIMAGE_ICON_TARGET}"
        COMMAND ${CMAKE_COMMAND} -E env "PATH=${LINUXDEPLOY_ENV_PATH}" ${QMAKE_ENV}
            "QML_SOURCES_PATHS=${QML_SOURCES_PATHS}"
            "${LINUXDEPLOY_QT_PLUGIN_EXECUTABLE}"
            --appdir "${APPIMAGE_APPDIR}"
            --exclude-library "libqsqlmimer.so"
            --exclude-library "libqsqloci.so*"
        COMMAND ${CMAKE_COMMAND} -E env "PATH=${LINUXDEPLOY_ENV_PATH}" ${APPIMAGE_RUNTIME_ENV}
            "${LINUXDEPLOY_EXECUTABLE}"
            --appdir "${APPIMAGE_APPDIR}"
            --output appimage
        DEPENDS ${APPIMAGE_DEPENDENCIES}
        WORKING_DIRECTORY "${APPIMAGE_ROOT_DIR}"
        COMMENT "Building AppImage"
        VERBATIM
    )
endif()
