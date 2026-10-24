cmake_minimum_required(VERSION 3.21)
# Keep the version in sync with JME_VERSION_* in include/jmevm/jmevm.h.
project(jmevm VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(ANDROID)
    set(host_default OFF)
else()
    set(host_default ON)
endif()
option(JMEVM_TOOLS "Build the jmevm command-line runner" ${host_default})
option(JMEVM_ASAN "Build with AddressSanitizer" OFF)
option(JMEVM_CLASSLIB "Compile the class library and Java tests (needs a JDK)" ${host_default})

include(CTest)

# Before any target, so it applies to all of them.
if(JMEVM_ASAN)
    if(MSVC)
        add_compile_options(/fsanitize=address /Zi)
        add_link_options(/INCREMENTAL:NO)
        # Executables need the ASan runtime DLL from the compiler's folder.
        get_filename_component(msvc_bin ${CMAKE_C_COMPILER} DIRECTORY)
        file(GLOB JMEVM_ASAN_DLL ${msvc_bin}/clang_rt.asan_dynamic-*.dll)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

file(GLOB vm_sources CONFIGURE_DEPENDS src/*.c)
add_library(jmevm STATIC ${vm_sources})
target_include_directories(jmevm PUBLIC include PRIVATE src)
# Floating point must not be contracted into FMA, so results match on every
# platform. Calling an undeclared function or mixing pointer types is always
# a bug.
if(MSVC)
    # The library and the runner are both called jmevm: their debug
    # databases must not share a name, or MSBuild skips relinking.
    set_target_properties(jmevm PROPERTIES COMPILE_PDB_NAME jmevm_lib)
    target_compile_options(jmevm PRIVATE /W4 /utf-8 /we4013 /we4047 /we4024 /we4133)
    target_compile_definitions(jmevm PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
    # The heap is a byte array read through typed pointers.
    target_compile_options(jmevm PRIVATE
        -Wall -Wextra -Wno-unused-parameter
        -Werror=int-conversion -Werror=incompatible-pointer-types
        -Werror=implicit-function-declaration -ffp-contract=off -fno-strict-aliasing)
    target_link_libraries(jmevm PUBLIC m)
endif()

if(JMEVM_TOOLS)
    add_executable(jmevm_cli tools/jmevm_main.c tools/check.c tools/testhost.c)
    # The runner is jmevm.exe; its import library must not be jmevm.lib, the
    # VM library itself, or MSBuild never relinks the runner.
    set_target_properties(jmevm_cli PROPERTIES OUTPUT_NAME jmevm ARCHIVE_OUTPUT_NAME jmevm_cli)
    # The runner is an internal tool: it also uses the class file parser.
    target_include_directories(jmevm_cli PRIVATE src)
    target_link_libraries(jmevm_cli PRIVATE jmevm)
endif()

# Copies the ASan runtime next to an executable (MSVC only).
function(jmevm_asan_runtime target)
    if(JMEVM_ASAN_DLL)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${JMEVM_ASAN_DLL} $<TARGET_FILE_DIR:${target}>
            VERBATIM)
    endif()
endfunction()
if(JMEVM_TOOLS)
    jmevm_asan_runtime(jmevm_cli)
endif()

if(JMEVM_CLASSLIB)
    find_package(Java 11 COMPONENTS Development REQUIRED)
    # javac 9+ can no longer target 1.3; class files are version 52, which the
    # VM accepts. Games keep whatever version they were built with (45-49).
    set(JMEVM_JAVAC_FLAGS -nowarn -Xlint:-options -encoding UTF-8 -source 8 -target 8)
    add_subdirectory(classlib)
endif()

if(BUILD_TESTING AND JMEVM_TOOLS)
    add_subdirectory(tests)
endif()

# The MIDP layer (midp/): natives, embedding library, desktop runner.
option(JMEVM_MIDP "Build the MIDP layer" ON)
if(JMEVM_MIDP)
    add_subdirectory(midp)
endif()
