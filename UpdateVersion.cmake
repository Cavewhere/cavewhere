function(update_git_version TARGET_NAME VERSION_IN_FILE)
    # Custom command to always run (it doesn't produce an output)
    add_custom_command(
        OUTPUT always_rebuild
        COMMAND cmake -E echo
    )

    # Custom target that depends on the custom command
    add_custom_target(
        update_version ALL
        DEPENDS always_rebuild
    )

    # Custom command to re-run CMake if Git HEAD changes
    add_custom_command(
        TARGET update_version
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "Checking for Git version change..."
        COMMAND git rev-parse HEAD > ${CMAKE_BINARY_DIR}/current_git_hash.txt
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/current_git_hash.txt ${CMAKE_BINARY_DIR}/last_git_hash.txt
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "Git version checked."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    # Check if the Git hash has changed
    if(EXISTS "${CMAKE_BINARY_DIR}/current_git_hash.txt" AND EXISTS "${CMAKE_BINARY_DIR}/last_git_hash.txt")
        file(READ "${CMAKE_BINARY_DIR}/current_git_hash.txt" CURRENT_GIT_HASH)
        file(READ "${CMAKE_BINARY_DIR}/last_git_hash.txt" LAST_GIT_HASH)

        if(NOT "${CURRENT_GIT_HASH}" STREQUAL "${LAST_GIT_HASH}")
            # Code to update cavewhereVersion.h
            message("Git version has changed. Updating ${VERSION_IN_FILE}...")
            set(CAVEWHERE_VERSION ${CURRENT_GIT_HASH})
            configure_file(${VERSION_IN_FILE} ${CMAKE_BINARY_DIR}/generated/${VERSION_IN_FILE})
        endif()
    endif()

    add_dependencies(${TARGET_NAME} update_version)
endfunction()
