# The MIDP layer: what a MIDlet needs besides the VM (screen, images, fonts,
# sound, record stores, input), with its state in jmevm snapshots.
#   jmevm_midp        the natives and the portable runtime (src/)
#   jmevm_midp_embed  a library for apps that show the game themselves
#                     (platform/embed/jmevm_midp.h; NextJ2ME on Android)
#   jmevm-midp        the desktop runner (platform/host), with JMEVM_TOOLS
#   classlib.jar      CLDC and MIDP classes, with JMEVM_CLASSLIB
file(GLOB midp_sources CONFIGURE_DEPENDS
    src/util/*.c src/audio/*.c src/midp_jme/*.c
    src/midp/gfx.c src/midp/font.c src/midp/font_data.c)
add_library(jmevm_midp STATIC ${midp_sources})
target_include_directories(jmevm_midp PUBLIC src)
target_link_libraries(jmevm_midp PUBLIC jmevm)
# Floating point must not be contracted into FMA, so results match on every
# platform.
if(MSVC)
    target_compile_options(jmevm_midp PUBLIC /W3 /utf-8 /we4047 /we4024 /we4133 /we4013)
else()
    target_compile_options(jmevm_midp PUBLIC
        -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
        -Werror=int-conversion -Werror=incompatible-pointer-types
        -Werror=implicit-function-declaration -ffp-contract=off)
    target_compile_definitions(jmevm_midp PUBLIC _POSIX_C_SOURCE=200809L)
endif()

add_library(jmevm_midp_embed STATIC platform/embed/embed.c platform/host/engine_jmevm.c)
target_include_directories(jmevm_midp_embed PUBLIC platform/embed)
target_link_libraries(jmevm_midp_embed PUBLIC jmevm_midp)

if(JMEVM_CLASSLIB)
    # CLDC from ../classlib and MIDP from here, compiled against themselves.
    file(GLOB_RECURSE midp_classlib_sources CONFIGURE_DEPENDS
        ${PROJECT_SOURCE_DIR}/classlib/src/*.java ${CMAKE_CURRENT_SOURCE_DIR}/classlib/src/*.java)
    set(classes ${CMAKE_CURRENT_BINARY_DIR}/classlib)
    set(argfile ${CMAKE_CURRENT_BINARY_DIR}/classlib-sources.txt)
    set(JMEVM_MIDP_CLASSLIB_CLASSES ${classes})
    set(JMEVM_MIDP_CLASSLIB_JAR ${CMAKE_CURRENT_BINARY_DIR}/classlib.jar)
    set(lines "")
    foreach(f IN LISTS midp_classlib_sources)
        string(APPEND lines "\"${f}\"\n")
    endforeach()
    file(WRITE ${argfile} "${lines}")
    add_custom_command(
        OUTPUT ${JMEVM_MIDP_CLASSLIB_JAR}
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${classes}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${classes}
        COMMAND ${Java_JAVAC_EXECUTABLE} ${JMEVM_JAVAC_FLAGS}
            -bootclasspath ${classes} -d ${classes} @${argfile}
        COMMAND ${Java_JAR_EXECUTABLE} cfM ${JMEVM_MIDP_CLASSLIB_JAR} -C ${classes} .
        DEPENDS ${midp_classlib_sources}
        COMMENT "Building the MIDP classlib.jar"
        VERBATIM)
    add_custom_target(midp_classlib ALL DEPENDS ${JMEVM_MIDP_CLASSLIB_JAR})
endif()

if(JMEVM_TOOLS)
    set(runner_sources platform/host/main.c platform/host/engine_jmevm.c platform/host/desktop.c
        platform/host/input.c platform/host/sound.c)
    if(WIN32)
        list(APPEND runner_sources platform/host/window_win32.c platform/host/sound_win32.c)
    else()
        list(APPEND runner_sources platform/host/window_none.c platform/host/sound_none.c)
    endif()
    add_executable(jmevm_midp_runner ${runner_sources})
    set_target_properties(jmevm_midp_runner PROPERTIES OUTPUT_NAME jmevm-midp)
    target_link_libraries(jmevm_midp_runner PRIVATE jmevm_midp)
    if(WIN32)
        target_link_libraries(jmevm_midp_runner PRIVATE user32 gdi32 winmm)
    endif()
    jmevm_asan_runtime(jmevm_midp_runner)
    if(JMEVM_CLASSLIB)
        # The runner looks for classlib.jar next to itself.
        add_custom_target(midp_classlib_copy ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${JMEVM_MIDP_CLASSLIB_JAR} $<TARGET_FILE_DIR:jmevm_midp_runner>/classlib.jar
            DEPENDS midp_classlib
            VERBATIM)
        add_dependencies(midp_classlib_copy jmevm_midp_runner)
    endif()
    if(BUILD_TESTING AND JMEVM_CLASSLIB)
        add_subdirectory(tests)
    endif()
endif()
