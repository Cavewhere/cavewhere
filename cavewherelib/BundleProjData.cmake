# Idempotent copy of PROJ's runtime data dir for cw_bundle_proj_data().
# Invoked via `cmake -P` from a POST_BUILD step; skips work when the
# destination's proj.db is at least as new as the source's, so 8.4 MB of
# files don't get re-copied on every relink.
#
# Inputs (passed via -D):
#   SRC — source data dir (must contain proj.db)
#   DST — destination dir to populate

if(NOT DEFINED SRC OR NOT DEFINED DST)
    message(FATAL_ERROR "BundleProjData.cmake: SRC and DST must be set")
endif()

set(_src_db "${SRC}/proj.db")
set(_dst_db "${DST}/proj.db")

if(NOT EXISTS "${_src_db}")
    message(FATAL_ERROR "BundleProjData.cmake: ${_src_db} does not exist")
endif()

if(EXISTS "${_dst_db}")
    file(TIMESTAMP "${_src_db}" _src_ts "%s")
    file(TIMESTAMP "${_dst_db}" _dst_ts "%s")
    if(_dst_ts GREATER_EQUAL _src_ts)
        return()
    endif()
endif()

file(MAKE_DIRECTORY "${DST}")
file(COPY "${SRC}/" DESTINATION "${DST}")
