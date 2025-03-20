include("GitHash.cmake")

function(configure_version CURRENT_GIT_HASH OUTPUT_FILE)
    set(CAVEWHERE_VERSION ${CURRENT_GIT_HASH})
    configure_file(${VERSION_IN_FILE} ${OUTPUT_FILE})
endfunction()

# Extract just the filename from the full path
get_filename_component(VERSION_FILENAME ${VERSION_IN_FILE} NAME)

# Remove the ".in" suffix from the filename
string(REPLACE ".in" "" VERSION_FILENAME ${VERSION_FILENAME})

set(OUTPUT_FILE ${OUTPUT_DIR}/versionInfo/${VERSION_FILENAME})

#message(STATUS "Output version filename:${OUTPUT_FILE}" )

# Read the current git hash
get_hash("${OUTPUT_DIR}/current_git_hash.txt" CURRENT_GIT_HASH)

# UpdateVersionHelper.cmake
if(EXISTS "${OUTPUT_DIR}/current_git_hash.txt"
        AND EXISTS "${OUTPUT_DIR}/last_git_hash.txt"
        AND EXISTS "${OUTPUT_FILE}")

    # Read the last git hash
    get_hash("${OUTPUT_DIR}/last_git_hash.txt" LAST_GIT_HASH)

    if(NOT "${CURRENT_GIT_HASH}" STREQUAL "${LAST_GIT_HASH}")
        message("Git version has changed ${LAST_GIT_HASH} -> ${CURRENT_GIT_HASH}. Updating ${OUTPUT_FILE}...")
        configure_version(${CURRENT_GIT_HASH} ${OUTPUT_FILE})
    else()
        message(STATUS "Git version has not changed ${LAST_GIT_HASH} -> ${CURRENT_GIT_HASH}.")
    endif()
else()
    message("Generating version info for ${CURRENT_GIT_HASH}. Creating ${OUTPUT_FILE}...")
    configure_version(${CURRENT_GIT_HASH} ${OUTPUT_FILE})
endif()
