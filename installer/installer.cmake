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

set(CAVEWHERE_NAME "Cavewhere")

if(WIN32)
    set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")

    include(InstallRequiredSystemLibraries)

    get_target_property(BINARY_DIR CaveWhere RUNTIME_OUTPUT_DIRECTORY)
    set(CAVEWHER_BINARY_PATH "${DEPLOY_DIR}/CaveWhere${CMAKE_EXECUTABLE_SUFFIX}")
    get_target_property(CAVEWHERE_SOURCE_DIR CaveWhere SOURCE_DIR)

    # Define the destination directory
    set(QML_DIR "${CMAKE_BINARY_DIR}/cavewherelib")
    set(DEPLOY_SURVEX_DIR "${DEPLOY_DIR}/survex")
    set(SURVEX_BIN_DIR "${BINARY_DIR}/survex")


    # Define the list of files to copy
    set(ROOT_FILES_TO_COPY
        "${BINARY_DIR}/${CAVEWHERE_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
        "${BINARY_DIR}/cavewhere-test${CMAKE_EXECUTABLE_SUFFIX}"
        "${BINARY_DIR}/cavewherelib${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${BINARY_DIR}/cavewhere-testlib${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${BINARY_DIR}/dewalls${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${BINARY_DIR}/QMath3d${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${BINARY_DIR}/QmlTestRecorder${CMAKE_SHARED_LIBRARY_SUFFIX}"

        #"${CONAN_BIN_DIRS_ZLIB}/zlib1.dll"
    )

    set(PLUGIN_DIR $<TARGET_FILE_DIR:cavewherelibplugin>)
    SET(cavewherelib_FILES_TO_COPY
        "${PLUGIN_DIR}/qmldir"
        "${PLUGIN_DIR}/cavewherelib.qmltypes"
        "${PLUGIN_DIR}/Theme.js"
        "${PLUGIN_DIR}/Utils.js"
        "${PLUGIN_DIR}/VectorMath.js"
        "${PLUGIN_DIR}/cavewherelibplugin${CMAKE_SHARED_LIBRARY_SUFFIX}"
    )


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
        cavewhere-test
        survex
        cavewhere-qml-test
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
