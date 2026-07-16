# Abseil static linking helper for CaveWhere.
#
# Exposes:
#
#   cw_link_absl_static(<target>)
#       Explicitly globs Conan's Abseil static libs and links them into the
#       target. Works around the Conan Abseil CMakeDeps recipe declaring
#       incomplete library sets. No-op on macOS (uses dylibs/frameworks).
#
# Linux wraps the libs with --start-group/--end-group to satisfy the
# inter-library circular dependencies common in Abseil's split-component
# build.

# Work around Conan Abseil CMakeDeps missing libs by explicitly linking absl libs.
function(cw_link_absl_static target)
    if(NOT (WIN32 OR (UNIX AND NOT APPLE)))
        return()
    endif()

    if(WIN32)
        set(_absl_glob_patterns "abseil_dll.lib" "absl_*.lib" "libabsl_*.lib")
    else()
        set(_absl_glob_patterns "libabsl_*.a")
    endif()

    set(_absl_configs "")
    if(CMAKE_CONFIGURATION_TYPES)
        set(_absl_configs ${CMAKE_CONFIGURATION_TYPES})
    elseif(CMAKE_BUILD_TYPE)
        set(_absl_configs ${CMAKE_BUILD_TYPE})
    endif()

    if(NOT _absl_configs)
        message(WARNING "No build configuration detected; skipping absl static linking for ${target}")
        return()
    endif()

    foreach(_absl_config IN LISTS _absl_configs)
        string(TOUPPER "${_absl_config}" _absl_config_upper)
        set(_absl_lib_dirs "${abseil_LIB_DIRS_${_absl_config_upper}}")

        if(_absl_lib_dirs)
            list(GET _absl_lib_dirs 0 _absl_lib_dir)
            set(_absl_static_libs "")
            foreach(_absl_pattern IN LISTS _absl_glob_patterns)
                file(GLOB _absl_pattern_libs "${_absl_lib_dir}/${_absl_pattern}")
                list(APPEND _absl_static_libs ${_absl_pattern_libs})
            endforeach()
            list(REMOVE_DUPLICATES _absl_static_libs)

            if(_absl_static_libs)
                if(UNIX AND NOT APPLE)
                    target_link_libraries(
                        ${target}
                        PRIVATE
                        "$<$<CONFIG:${_absl_config}>:-Wl,--start-group>"
                        "$<$<CONFIG:${_absl_config}>:${_absl_static_libs}>"
                        "$<$<CONFIG:${_absl_config}>:-Wl,--end-group>"
                    )
                else()
                    target_link_libraries(${target} PRIVATE "$<$<CONFIG:${_absl_config}>:${_absl_static_libs}>")
                endif()
            else()
                message(WARNING "Abseil static libs not found in ${_absl_lib_dir} for ${_absl_config}")
            endif()
        else()
            message(WARNING "Abseil lib directories not found for ${_absl_config}; skipping absl static linking for ${target}")
        endif()
    endforeach()
endfunction()
