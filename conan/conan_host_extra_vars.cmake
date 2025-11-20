#Docs https://www.bookstack.cn/read/conan-2.10-en/6acce4282e622ff3.md

if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR ANDROID)
    #This turns off protoc binary when building for iOS and ANDROID
    set(protobuf_BUILD_PROTOC_BINARIES OFF)
endif()

