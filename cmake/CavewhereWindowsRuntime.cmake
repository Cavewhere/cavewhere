# Windows runtime DLL collection for CaveWhere.
#
# Exposes:
#
#   cavewhere_setup_windows_runtime(<target>)
#       Scrapes the Conan-generated *_BIN_DIRS_* variables for protobuf,
#       abseil, and OpenSSL DLLs, then adds a POST_BUILD copy_if_different
#       step that drops them next to the target's .exe. Also applies the
#       Windows /SUBSYSTEM:WINDOWS link flag for Release configurations.
#
# The Conan BIN_DIRS variables are set by find_package() calls in the root
# CMakeLists.txt (protobuf, OpenSSL, absl) and therefore live in the root's
# directory scope, which a function called from the root can read freely.

function(cavewhere_setup_windows_runtime target)
    set(_conan_runtime_dlls "")
    message(STATUS "CaveWhere Windows runtime DLL collection enabled")
    set(_protobuf_bin_dirs "")
    if(DEFINED protobuf_BIN_DIRS_RELEASE)
        list(APPEND _protobuf_bin_dirs ${protobuf_BIN_DIRS_RELEASE})
    endif()
    if(DEFINED protobuf_BIN_DIRS_DEBUG)
        list(APPEND _protobuf_bin_dirs ${protobuf_BIN_DIRS_DEBUG})
    endif()
    if(DEFINED protobuf_BIN_DIRS)
        list(APPEND _protobuf_bin_dirs ${protobuf_BIN_DIRS})
    endif()
    message(STATUS "protobuf bin dirs: ${_protobuf_bin_dirs}")
    foreach(_dir IN LISTS _protobuf_bin_dirs)
        file(GLOB _protobuf_dlls
            "${_dir}/libprotobuf*.dll"
            "${_dir}/protobuf*.dll"
            "${_dir}/libutf8_validity.dll"
        )
        message(STATUS "protobuf dlls from ${_dir}: ${_protobuf_dlls}")
        list(APPEND _conan_runtime_dlls ${_protobuf_dlls})
    endforeach()

    set(_abseil_bin_dirs "")
    if(DEFINED abseil_BIN_DIRS_RELEASE)
        list(APPEND _abseil_bin_dirs ${abseil_BIN_DIRS_RELEASE})
    endif()
    if(DEFINED abseil_BIN_DIRS_DEBUG)
        list(APPEND _abseil_bin_dirs ${abseil_BIN_DIRS_DEBUG})
    endif()
    if(DEFINED abseil_BIN_DIRS)
        list(APPEND _abseil_bin_dirs ${abseil_BIN_DIRS})
    endif()
    message(STATUS "abseil bin dirs: ${_abseil_bin_dirs}")
    foreach(_dir IN LISTS _abseil_bin_dirs)
        file(GLOB _abseil_dlls
            "${_dir}/abseil_dll.dll"
            "${_dir}/absl_*.dll"
            "${_dir}/libutf8_validity.dll"
        )
        message(STATUS "abseil dlls from ${_dir}: ${_abseil_dlls}")
        list(APPEND _conan_runtime_dlls ${_abseil_dlls})
    endforeach()

    # Conan exports the package as OpenSSL, but its generated metadata variables
    # use the lowercase recipe name prefix: openssl_*.
    set(_openssl_bin_dirs "")
    if(DEFINED openssl_OpenSSL_SSL_BIN_DIRS_DEBUG)
        list(APPEND _openssl_bin_dirs ${openssl_OpenSSL_SSL_BIN_DIRS_DEBUG})
    endif()
    if(DEFINED openssl_OpenSSL_SSL_BIN_DIRS_RELEASE)
        list(APPEND _openssl_bin_dirs ${openssl_OpenSSL_SSL_BIN_DIRS_RELEASE})
    endif()
    if(DEFINED openssl_OpenSSL_Crypto_BIN_DIRS_DEBUG)
        list(APPEND _openssl_bin_dirs ${openssl_OpenSSL_Crypto_BIN_DIRS_DEBUG})
    endif()
    if(DEFINED openssl_OpenSSL_Crypto_BIN_DIRS_RELEASE)
        list(APPEND _openssl_bin_dirs ${openssl_OpenSSL_Crypto_BIN_DIRS_RELEASE})
    endif()
    if(DEFINED openssl_BIN_DIRS_DEBUG)
        list(APPEND _openssl_bin_dirs ${openssl_BIN_DIRS_DEBUG})
    endif()
    if(DEFINED openssl_BIN_DIRS_RELEASE)
        list(APPEND _openssl_bin_dirs ${openssl_BIN_DIRS_RELEASE})
    endif()
    list(REMOVE_DUPLICATES _openssl_bin_dirs)
    message(STATUS "openssl component bin dirs: ${_openssl_bin_dirs}")
    foreach(_dir IN LISTS _openssl_bin_dirs)
        file(GLOB _openssl_dlls
            "${_dir}/libssl*.dll"
            "${_dir}/libcrypto*.dll"
        )
        message(STATUS "openssl dlls from ${_dir}: ${_openssl_dlls}")
        list(APPEND _conan_runtime_dlls ${_openssl_dlls})
    endforeach()

    list(REMOVE_DUPLICATES _conan_runtime_dlls)
    message(STATUS "final Conan runtime DLLs: ${_conan_runtime_dlls}")

    if(_conan_runtime_dlls)
        add_custom_command(TARGET CaveWhere POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_conan_runtime_dlls}
                $<TARGET_FILE_DIR:CaveWhere>
            COMMAND_EXPAND_LISTS
            COMMENT "Copying Conan runtime DLLs next to CaveWhere.exe."
        )
    else()
        message(WARNING "No Conan runtime DLLs were discovered for CaveWhere on Windows")
    endif()

    # add_custom_command(TARGET CaveWhere POST_BUILD
    #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #         $<TARGET_RUNTIME_DLLS:CaveWhere>
    #         $<TARGET_FILE_DIR:CaveWhere>
    #     COMMAND_EXPAND_LISTS
    #     COMMENT "Copying CaveWhere runtime DLLs next to the executable."
    # )

    target_link_options(CaveWhere PRIVATE
        $<$<AND:$<CONFIG:Release>,$<STREQUAL:${CMAKE_SYSTEM_NAME},Windows>>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>
    )
endfunction()
