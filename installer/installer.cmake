# Determine the target OS
get_target_property(QtCore_location Qt::Core LOCATION)
get_filename_component(Qt_bin_dir ${QtCore_location} DIRECTORY)

if(WIN32)
    get_target_property(BINARY_DIR CaveWhere RUNTIME_OUTPUT_DIRECTORY)
    set(CAVEWHER_BINARY_PATH "${BINARY_DIR}/${TARGET_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
    get_target_property(CAVEWHERE_SOURCE_DIR CaveWhere SOURCE_DIR)

    set(DEPLOYMENT_APP "${Qt_bin_dir}/windeployqt.exe")
    set(DEPLOYMENT_ARGS "--qmldir"
                         "${CAVEWHERE_SOURCE_DIR}/qml"
                         "-sql"
                         "-xml"
                         "-opengl"
                         "-concurrent"
                         "-test"
                         "${CAVEWHER_BINARY_PATH}")
elseif(APPLE)
    set(DEPLOYMENT_APP "${Qt_bin_dir}/macdeployqt")
    set(BUNDLE_NAME "CaveWhere.app")
    set(BUNDLE_PATH "${CMAKE_INSTALL_PREFIX}/${BUNDLE_NAME}")
    set(DEPLOYMENT_ARGS "${BUNDLE_PATH}"
                         "-qmldir=${CMAKE_INSTALL_PREFIX}/${BUNDLE_NAME}/Contents/Resources/qml"
                         "-appstore-compliant")
else()
    message(FATAL_ERROR "Unsupported OS")
endif()

# Custom command to run deployment app
add_custom_command(OUTPUT deploy_timestamp.txt
                   COMMAND ${DEPLOYMENT_APP} ${DEPLOYMENT_ARGS}
                   COMMAND ${CMAKE_COMMAND} -E touch deploy_timestamp.txt
                   COMMENT "Running ${DEPLOYMENT_APP}"
                   VERBATIM)

# Custom target to trigger the custom command
add_custom_target(deployCavewhere EXCLUDE_FROM_ALL
                  DEPENDS deploy_timestamp.txt)

## Make sure your main target depends on this custom target
add_dependencies(deployCavewhere CaveWhere cavewhere-test)

file(READ "${CMAKE_BINARY_DIR}/current_git_hash.txt" GIT_VERSION)

if(WIN32)
    # Determine the architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH "64bit")
        set(ARCHITECTURES_ALLOWED "x64")
        set(REDIST_EXE "vc_redist.x64.exe")
        set(REDIST_VERSION "VC_2017_REDIST_X64_ADD")
    else()
        set(ARCH "32bit")
        set(ARCHITECTURES_ALLOWED "")
        set(REDIST_EXE "vc_redist.x86.exe")
        set(REDIST_VERSION "VC_2017_REDIST_X86_ADD")
    endif()

    # Custom command to generate Inno Setup script
    add_custom_command(OUTPUT cavewhere-withDefines.iss
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/windows/cavewhere.iss cavewhere-withDefines.iss
        COMMAND ${CMAKE_COMMAND} -E echo_append "#define MyAppVersion \\\"${CURRENT_GIT_HASH}\\\"" >> cavewhere-withDefines.iss
        COMMAND ${CMAKE_COMMAND} -E echo_append "#define MyArchitecturesAllowed \\\"x64\\\"" >> cavewhere-withDefines.iss
        COMMENT "Generating Inno Setup script"
        VERBATIM)

    # Custom command to run Inno Setup compiler
    add_custom_command(OUTPUT Cavewhere_${GIT_VERSION}_${ARCH}.exe
        COMMAND "C:/Program Files (x86)/Inno Setup 6/ISCC.exe" cavewhere-withDefines.iss
        DEPENDS cavewhere-withDefines.iss
        COMMENT "Running Inno Setup compiler"
        VERBATIM)

    # Custom target to trigger the custom command
    add_custom_target(windowsInstaller EXCLUDE_FROM_ALL
        DEPENDS Cavewhere_${GIT_VERSION}_${ARCH}.exe)

    # Make sure your main target depends on this custom target
    add_dependencies(windowsInstaller deployCavewhere) # update_version)

endif()
