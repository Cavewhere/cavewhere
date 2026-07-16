# Android-specific build configuration for CaveWhere.
#
# Exposes:
#
#   cavewhere_setup_android_openssl(<target>)
#       Wires KDAB's prebuilt Android OpenSSL libraries into the APK so Qt's
#       TLS plugin can dlopen libssl/libcrypto at runtime.
#
# Safe to include() unconditionally — only has side effects when called.
#
# APK signing is handled natively by Qt Creator (Projects → Build → Build
# Android APK → Sign package), which runs androiddeployqt --sign. See
# docs/android-build.md for setup.

function(cavewhere_setup_android_openssl target)
    # Qt's TLS plugin dlopens libssl/libcrypto at runtime. Use KDAB's
    # prebuilt Android OpenSSL (libssl_3.so / libcrypto_3.so with
    # correct SONAMEs — unlike Conan's libssl.so.3 which Android can't
    # load from the APK lib dir). Point ANDROID_OPENSSL_DIR at the
    # `android_openssl` repo checkout; defaults to a common SDK path.
    set(ANDROID_OPENSSL_DIR "$ENV{HOME}/Library/Android/sdk/android_openssl"
        CACHE PATH "Path to KDAB android_openssl prebuilts (https://github.com/KDAB/android_openssl)")
    if(NOT EXISTS "${ANDROID_OPENSSL_DIR}/android_openssl.cmake")
        message(FATAL_ERROR
            "ANDROID_OPENSSL_DIR (${ANDROID_OPENSSL_DIR}) does not contain "
            "android_openssl.cmake. Clone https://github.com/KDAB/android_openssl "
            "and set -DANDROID_OPENSSL_DIR=<path> (see docs/android-build.md).")
    endif()
    include("${ANDROID_OPENSSL_DIR}/android_openssl.cmake")
    add_android_openssl_libraries(${target})
endfunction()
