# Set variables that will be replaced in the template
#set(CAVEWHERE_NAME "Cavewhere")
#set(CAVEWHER_VERSION "1.0.0") # Replace with the actual version
#set(ARCH_ALLOWED "x64") # Replace as needed
#set(INSTALL_64BIT "x64") # Replace as needed
#set(ARCH "x64") # Replace as needed
#set(REDISTRIBUTABLE_EXE "vc_redist.x64.exe") # Replace as needed
#set(REDISTRIBUTABLE_VERSION "VC_2017_REDIST_X64_ADD") # Replace as needed
#set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy") # Replace with your actual deploy directory

include(${GET_HASH_CMAKE})
get_hash(${CAVEWHERE_VERSION_FILE} CAVEWHERE_VERSION)

#message(FATAL "innoConfigure: ${GET_HASH_CMAKE} ${CAVEWHERE_VERSION_FILE} Cavewhere version: ${CAVEWHERE_VERSION}")

# Use configure_file to generate cavewhere.iss from cavewhere.iss.in
configure_file(${ISS_IN} ${ISS})
