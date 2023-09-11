function(update_git_version TARGET_NAME VERSION_IN_FILE)
    add_custom_command(
        OUTPUT always_rebuild
        COMMAND cmake -E echo
    )

    add_custom_target(
        update_version ALL
        DEPENDS always_rebuild
    )

    add_custom_command(
        TARGET update_version
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "Checking for Git version change..."
        COMMAND git describe HEAD > ${CMAKE_BINARY_DIR}/current_git_hash.txt
        COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "Git version checked."
        COMMAND ${CMAKE_COMMAND} -DVERSION_IN_FILE=${VERSION_IN_FILE} -DOUTPUT_DIR="${CMAKE_BINARY_DIR}" -P ${CMAKE_SOURCE_DIR}/UpdateVersionHelper.cmake
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/current_git_hash.txt ${CMAKE_BINARY_DIR}/last_git_hash.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    add_dependencies(${TARGET_NAME} update_version)
endfunction()
