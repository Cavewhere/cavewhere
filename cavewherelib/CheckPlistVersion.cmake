# Post-build sanity check: verify the built bundle's Info.plist
# CFBundleShortVersionString is a prefix of the CavewhereVersion string baked
# into cavewhereVersion.h by UpdateVersion.cmake.
#
# The plist comes from `git describe --tags --abbrev=0` (at configure time).
# The header comes from `git describe HEAD` (post-build). When HEAD is exactly
# on a tag, both are equal; otherwise the header is "<tag>-<N>-g<sha>".
# So the invariant is: header_version must start with plist_version.
#
# Invoked via `cmake -P` from the CaveWhere POST_BUILD step. Inputs:
#   PLIST_PATH      Path to the built Info.plist (inside .app bundle).
#                   Optional — when absent, the script reads Xcode's
#                   $TARGET_BUILD_DIR/$INFOPLIST_PATH env vars, which is what
#                   the POST_BUILD step relies on (passing a CMake generator
#                   expression wouldn't work: VERBATIM escapes the `$` in
#                   `${EFFECTIVE_PLATFORM_NAME}`, so Xcode never substitutes it).
#   VERSION_HEADER  Path to the generated cavewhereVersion.h (required).

if(NOT PLIST_PATH)
    if(NOT DEFINED ENV{TARGET_BUILD_DIR} OR NOT DEFINED ENV{INFOPLIST_PATH})
        message(FATAL_ERROR
            "CheckPlistVersion: PLIST_PATH not set and no Xcode "
            "TARGET_BUILD_DIR/INFOPLIST_PATH environment available.")
    endif()
    set(PLIST_PATH "$ENV{TARGET_BUILD_DIR}/$ENV{INFOPLIST_PATH}")
endif()
if(NOT VERSION_HEADER)
    message(FATAL_ERROR "CheckPlistVersion: VERSION_HEADER not set")
endif()

if(NOT EXISTS "${PLIST_PATH}")
    message(FATAL_ERROR "CheckPlistVersion: plist not found: ${PLIST_PATH}")
endif()
if(NOT EXISTS "${VERSION_HEADER}")
    message(FATAL_ERROR "CheckPlistVersion: version header not found: ${VERSION_HEADER}")
endif()

find_program(PLUTIL_EXE plutil)
if(NOT PLUTIL_EXE)
    message(FATAL_ERROR "CheckPlistVersion: plutil not found on PATH")
endif()

execute_process(
    COMMAND "${PLUTIL_EXE}" -extract CFBundleShortVersionString raw -o - "${PLIST_PATH}"
    OUTPUT_VARIABLE _plist_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _plutil_rc
    ERROR_VARIABLE _plutil_err)
if(NOT _plutil_rc EQUAL 0)
    message(FATAL_ERROR
        "CheckPlistVersion: plutil failed to read CFBundleShortVersionString "
        "from ${PLIST_PATH}: ${_plutil_err}")
endif()

file(READ "${VERSION_HEADER}" _header_contents)
# Matches the exact form emitted by cavewhereVersion.h.in:
#   static const QString CavewhereVersion = QStringLiteral("...");
# A refactor to a different form triggers this FATAL_ERROR instead of
# silently capturing the wrong text.
if(NOT _header_contents MATCHES "CavewhereVersion[ \t]*=[ \t]*QStringLiteral\\(\"([^\"]+)\"\\)")
    message(FATAL_ERROR
        "CheckPlistVersion: could not find CavewhereVersion QStringLiteral in ${VERSION_HEADER}")
endif()
set(_header_version "${CMAKE_MATCH_1}")

string(LENGTH "${_plist_version}" _plist_len)
string(SUBSTRING "${_header_version}" 0 ${_plist_len} _header_prefix)

if(NOT _header_prefix STREQUAL _plist_version)
    message(FATAL_ERROR
        "CheckPlistVersion: version mismatch between Info.plist and cavewhereVersion.h\n"
        "  Info.plist CFBundleShortVersionString: ${_plist_version}\n"
        "  cavewhereVersion.h CavewhereVersion:   ${_header_version}\n"
        "Expected the header version to start with the plist version. "
        "Reconfigure CMake so both are regenerated from the current git state.")
endif()

message(STATUS
    "CheckPlistVersion: OK — plist ${_plist_version}, header ${_header_version}")
