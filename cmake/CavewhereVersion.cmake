# Marketing-version derivation for CaveWhere.
#
# Exposes:
#
#   cavewhere_compute_marketing_version(<out_version> <out_build_number> <out_regex>)
#       Derives the CFBundleShortVersionString from the most recent git tag
#       reachable from HEAD, plus a fallback build number (commits-since-tag + 1).
#       Writes three results back to the caller via PARENT_SCOPE:
#         out_version       — "MAJOR[.MINOR[.PATCH]]" (fallback "0.0.0")
#         out_build_number  — integer string (fallback "1")
#         out_regex         — the regex used to validate marketing-version strings
#
# MUST be called BEFORE project() so the value can be fed to
# project(... VERSION ...). The module file is safe to include() before
# project() — it only defines a function, no top-level side effects.

function(cavewhere_compute_marketing_version out_version out_build_number out_regex)
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

    set(${out_version} "${_cw_marketing_version}" PARENT_SCOPE)
    set(${out_build_number} "${_cw_default_build_number}" PARENT_SCOPE)
    set(${out_regex} "${_cw_marketing_version_regex}" PARENT_SCOPE)
endfunction()
