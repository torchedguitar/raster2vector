cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)

#------------------------------------------------------------------------------------------

project(raster2vector
    LANGUAGES CXX)

# Enforce standard C++ with sensible warnings and minimal compiler output on all platforms
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
if (${MSVC})
    # Must use - instead of / for option prefix when using NVCC, because it forwards args
    # it doesn't know to the host compiler, but being a UNIX-heritage program, it thinks
    # an argument like "/nologo" is an input file path.  Luckily, MSVC accepts - prefixes.
    add_compile_options(-permissive-)
    add_compile_options(-nologo)
    add_compile_options(-wd26451) # Disable warning: possible overflow multipying two 4-byte ints and storing to 8-byte int
    add_compile_options(-wd26812) # Disable warning: prefer enum class over unscoped enum
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)
else()
    add_compile_options(-Wfatal-errors)
endif()

# Build with minimal or no dependencies on installed C/C++ runtime libraries
if (${MSVC})
    # For Non-debug, change /MD (MultiThreadedDLL) to /MT (MultiThreaded)
    # For Debug, change /MDd (MultiThreadedDebugDLL) to /MTd ((MultiThreadedDebug)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    # Statically link libstdc++ and libgcc.  Do not statically link libc, though.
    # Use an old sysroot if compatibility with old GLIBC versions is required.
    # Use -s to strip unneeded symbols.
    add_link_options(-static-libstdc++ -static-libgcc -s)
endif()

include_directories(
    imports/CommandLine
    imports/simple-svg
    imports/stb
)

add_executable(raster2vector
    raster2vector.cpp
)

if(UNIX AND NOT APPLE)
    target_link_libraries(raster2vector
        ${CMAKE_DL_LIBS}
        rt
        pthread)
endif()