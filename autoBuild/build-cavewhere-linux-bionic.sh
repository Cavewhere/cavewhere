set -e

#
# Qbs is built with the address sanitizer enabled.
# Suppress findings in some parts of Qbs / dependencies.
#
#export LSAN_OPTIONS="suppressions=$( cd "$(dirname "$0")" ; pwd -P )/address-sanitizer-suppressions.txt:print_suppressions=0"

#
# Additional build options
#
BUILD_OPTIONS="\
#    ${QBS_BUILD_PROFILE:+profile:${QBS_BUILD_PROFILE}} \
#    modules.qbsbuildconfig.enableAddressSanitizer:true \
#    modules.qbsbuildconfig.enableProjectFileUpdates:true \
#    modules.qbsbuildconfig.enableUnitTests:true \
#    modules.cpp.treatWarningsAsErrors:true \
#    project.withExamples:true \
    ${BUILD_OPTIONS} \
    config:release \
"

#
# Build all default products of Qbs
#
qbs resolve ${BUILD_OPTIONS}
qbs build ${BUILD_OPTIONS}
qbs run --product cavewhere-test

