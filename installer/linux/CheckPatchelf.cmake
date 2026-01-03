if(DEFINED PATCHELF AND PATCHELF)
    set(PATCHELF_EXECUTABLE "${PATCHELF}")
else()
    find_program(PATCHELF_EXECUTABLE patchelf)
endif()

if(NOT PATCHELF_EXECUTABLE)
    message(FATAL_ERROR "patchelf not found. Install it or configure with -DPATCHELF=/full/path/to/patchelf.")
endif()
