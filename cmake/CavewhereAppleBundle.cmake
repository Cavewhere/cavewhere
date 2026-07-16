# Apple (macOS + iOS) bundle setup for CaveWhere.
#
# Exposes:
#
#   cavewhere_setup_apple_bundle(<target>)
#       Configures bundle metadata (Info.plist, icon, identifiers), assembles
#       the macOS .app's internal layout (QML plugins, survex binaries, PROJ
#       data, message files), and applies build-type-specific codesigning.
#       Branches on MOBILE_BUILD for iOS vs macOS specifics.
#
# Body is intentionally unchanged from the inline form that previously lived
# in the root CMakeLists.txt — the function wrapper is purely structural.
# The target argument is documentation; many internal references are
# CaveWhere-specific (target deps like `messageFiles`, `cavern`,
# `cavewherelibplugin`, and literal "CaveWhere.app" path strings).

function(cavewhere_setup_apple_bundle target)
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
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.cavewhere.cavewhere"
        #Enables instruments to run correctly in macos
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/entitlements.plist"
        #XCODE_ATTRIBUTE_DEVELOPMENT_TEAM
        XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--deep"
        #XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.cavewhere.cavewhere"

        RESOURCE "${CMAKE_SOURCE_DIR}/cavewhereIcon.icns"
    )

    if(MOBILE_BUILD)
        set_target_properties(CaveWhere PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/ios/Info.plist.in"
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
            XCODE_ATTRIBUTE_MARKETING_VERSION "${PROJECT_VERSION}"
            XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${CAVEWHERE_BUILD_NUMBER}")
        target_sources(CaveWhere PRIVATE "${CMAKE_SOURCE_DIR}/ios/Assets.xcassets")
        set_source_files_properties("${CMAKE_SOURCE_DIR}/ios/Assets.xcassets"
            PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

        # Cross-check the built Info.plist's CFBundleShortVersionString against the
        # CavewhereVersion string compiled into cavewhereVersion.h. The plist comes
        # from the latest reachable tag (configure time); the header comes from
        # `git describe HEAD` (cavewherelib post-build). Drift between them surfaces
        # here instead of at App Store Connect upload time. The script resolves the
        # plist path from Xcode's script-phase env — see CheckPlistVersion.cmake.
        add_custom_command(TARGET CaveWhere POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DVERSION_HEADER=${CMAKE_BINARY_DIR}/cavewherelib/versionInfo/cavewhereVersion.h
                -P ${CMAKE_SOURCE_DIR}/cavewherelib/CheckPlistVersion.cmake
            VERBATIM
            COMMENT "Verify Info.plist marketing version matches cavewhereVersion.h")
    else()
        set_target_properties(CaveWhere PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist.in")
    endif()

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

        add_custom_command(TARGET CaveWhere POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cavern> $<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/MacOS/cavern
            COMMENT "Copying survex module to application bundle."
        )

        set(SVX_MSG_DEST "$<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Resources")
        foreach(MSG_FILE ${SVX_MESSAGE_FILES})
            get_filename_component(MSG_NAME ${MSG_FILE} NAME)
            add_custom_command(TARGET CaveWhere POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${MSG_FILE} ${SVX_MSG_DEST}/${MSG_NAME}
                COMMENT "Copying ${MSG_NAME} to application bundle."
            )
        endforeach()

        if(CW_PROJ_DATA_DIR)
            add_custom_command(TARGET CaveWhere POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    -DSRC=${CW_PROJ_DATA_DIR}
                    -DDST=$<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/Resources/proj
                    -P ${CW_BUNDLE_PROJ_DATA_SCRIPT}
                COMMENT "Bundling PROJ data into CaveWhere.app/Contents/Resources/proj"
                VERBATIM)
        endif()

        add_dependencies(CaveWhere messageFiles)

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


        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            ## Ad-hoc re-sign with get-task-allow so Instruments / lldb can attach.
            ## No developer certificate is required for ad-hoc signing (-s -).
            add_custom_command(TARGET CaveWhere POST_BUILD
                COMMAND codesign --force --sign -
                        --entitlements "${CMAKE_SOURCE_DIR}/entitlements.plist"
                        "$<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>/.."
                COMMENT "Ad-hoc signing CaveWhere.app with get-task-allow (Debug)"
            )
        endif()

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
    else()
        # iOS: copy message files into the flat app bundle so cavern_run() can find them
        set(SVX_MSG_DEST "$<TARGET_BUNDLE_CONTENT_DIR:CaveWhere>")
        foreach(MSG_FILE ${SVX_MESSAGE_FILES})
            get_filename_component(MSG_NAME ${MSG_FILE} NAME)
            add_custom_command(TARGET CaveWhere POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${MSG_FILE} ${SVX_MSG_DEST}/${MSG_NAME}
                COMMENT "Copying ${MSG_NAME} to application bundle."
            )
        endforeach()

        add_dependencies(CaveWhere messageFiles)
    endif()
endfunction()
