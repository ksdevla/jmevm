# MIDlets compiled against the MIDP classlib.jar and run on the jmevm-midp
# runner: console output, frames, determinism, input replay and save states.
set(T ${CMAKE_CURRENT_BINARY_DIR})
set(src ${CMAKE_CURRENT_SOURCE_DIR})
set(boot -bootclasspath ${JMEVM_MIDP_CLASSLIB_CLASSES})
set(run_test ${src}/run_test.cmake)

# --- MIDlet jars: midlets/<name> and the kit's stripes
set(midlet_dirs "")
file(GLOB children LIST_DIRECTORIES true ${src}/midlets/*)
foreach(d IN LISTS children)
    if(IS_DIRECTORY ${d})
        list(APPEND midlet_dirs ${d})
    endif()
endforeach()
list(APPEND midlet_dirs ${PROJECT_SOURCE_DIR}/tests/kit/stripes)

set(midlet_jars "")
foreach(d IN LISTS midlet_dirs)
    get_filename_component(name ${d} NAME)
    file(GLOB_RECURSE m_sources CONFIGURE_DEPENDS ${d}/src/*.java)
    file(GLOB_RECURSE m_res CONFIGURE_DEPENDS ${d}/res/*)
    set(out ${T}/midlets/${name})
    set(copy_res "")
    if(IS_DIRECTORY ${d}/res)
        set(copy_res COMMAND ${CMAKE_COMMAND} -E copy_directory ${d}/res ${out})
    endif()
    add_custom_command(
        OUTPUT ${T}/midlets/${name}.jar
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${out}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${out}
        COMMAND ${Java_JAVAC_EXECUTABLE} ${JMEVM_JAVAC_FLAGS} ${boot} -d ${out} ${m_sources}
        ${copy_res}
        COMMAND ${Java_JAR_EXECUTABLE} cfm ${T}/midlets/${name}.jar ${d}/MANIFEST.MF -C ${out} .
        DEPENDS ${m_sources} ${m_res} ${d}/MANIFEST.MF midp_classlib
        VERBATIM)
    list(APPEND midlet_jars ${T}/midlets/${name}.jar)
endforeach()
add_custom_target(midp_test_programs ALL DEPENDS ${midlet_jars})

# --- Test registration
# midp_test(<name> ARGS <runner args...> [EXPECTED <file>] [DATA <dir>]
#           [FRAMES] [EXPECTED_FRAMES <file>] [REPEAT [REPLAY]])
# FRAMES records a frame log; REPEAT runs twice and requires identical
# output and frames (see run_test.cmake).
function(midp_test name)
    cmake_parse_arguments(P "FRAMES;REPEAT;REPLAY" "EXPECTED;DATA;EXPECTED_FRAMES" "ARGS" ${ARGN})
    string(REPLACE ";" "|" args "${P_ARGS}")
    set(frames "")
    if(P_FRAMES)
        set(frames ${T}/out/${name}.frames)
    endif()
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DEXE=$<TARGET_FILE:jmevm_midp_runner>
            "-DARGS=${args}"
            -DEXPECTED=${P_EXPECTED}
            -DDATA=${P_DATA}
            -DFRAMES=${frames}
            -DEXPECTED_FRAMES=${P_EXPECTED_FRAMES}
            -DREPEAT=${P_REPEAT}
            -DREPLAY=${P_REPLAY}
            -DOUTPUT=${T}/out/${name}.txt
            -DWORKDIR=${T}
            -P ${run_test})
endfunction()

set(lib --classlib ${JMEVM_MIDP_CLASSLIB_JAR})
# Collect before every allocation in a small heap: an object that C code
# forgot to root is freed and poisoned, and the test fails.
set(stress --gc-stress --heap 2)

# The test MIDlets check themselves against the specifications and print
# one "ok" line per check, so expected.txt is the list of checks that must
# pass. A keys.txt next to a MIDlet is its scripted input (keys and
# touches); the others run without input here. Times are emulated
# milliseconds, so these runs take a fraction of that.
# Every MIDlet also gets the generic script below in the determinism, replay
# and save-state tests, unless it has a keys.txt.
set(keys "300:-1,400:-1:up,900:-2,1000:-2:up,1500:-5,1550:-5:up,2000:53,2100:53:up")
foreach(d IN LISTS midlet_dirs)
    get_filename_component(name ${d} NAME)
    set(own_keys "")
    set(script_${name} ${keys})
    if(EXISTS ${d}/keys.txt)
        file(READ ${d}/keys.txt script)
        string(STRIP "${script}" script)
        set(own_keys --keys ${script})
        set(script_${name} ${script})
    endif()
    # Each test gets its own record store folder: tests run in parallel.
    set(run --epoch 0 --run-ms 6000 ${own_keys} ${T}/midlets/${name}.jar)
    if(EXISTS ${d}/expected.txt)
        midp_test(midp.${name} ARGS ${lib} --data ${T}/rms-${name} ${run}
            EXPECTED ${d}/expected.txt DATA ${T}/rms-${name})
        midp_test(midp.stress.${name}
            ARGS ${stress} ${lib} --data ${T}/rms-stress-${name} ${run}
            EXPECTED ${d}/expected.txt DATA ${T}/rms-stress-${name})
    else()
        midp_test(midp.${name}
            ARGS ${lib} --data ${T}/rms-${name} --run-ms 1500
                 --screenshot ${T}/out/${name}.ppm ${T}/midlets/${name}.jar
            DATA ${T}/rms-${name})
    endif()
endforeach()

# Determinism: the same program, start time and input must give the same
# output and the same frames, at the same emulated times. The stripes frame
# log is also compared with a recorded one, which catches changes in timing
# (when such a change is intended, re-record stripes.frames).
midp_test(midp.determinism.stripes
    ARGS ${lib} --epoch 0 --run-ms 3000 ${T}/midlets/stripes.jar
    FRAMES EXPECTED_FRAMES ${src}/stripes.frames REPEAT)
foreach(name draw game forms touch sound)
    midp_test(midp.determinism.${name}
        ARGS ${lib} --epoch 0 --data ${T}/rms-det-${name} --run-ms 6000 --keys ${script_${name}}
             ${T}/midlets/${name}.jar
        DATA ${T}/rms-det-${name} FRAMES REPEAT)
endforeach()

# Record and replay: input saved with --record during a scripted run must
# reproduce that run exactly when fed back with --replay alone.
foreach(name draw game touch)
    midp_test(midp.replay.${name}
        ARGS ${lib} --epoch 0 --data ${T}/rms-replay-${name} --run-ms 6000 --keys ${script_${name}}
             ${T}/midlets/${name}.jar
        DATA ${T}/rms-replay-${name} FRAMES REPEAT REPLAY)
endforeach()

# Save states: a state saved part-way through a scripted run, loaded in a
# new process, must reproduce every later frame and line of output. Saved at
# 1250 ms, between the scripted key presses: draw and game mid-animation
# with record stores (game), forms on screen, touch between touches, sound
# while a MIDI file plays.
foreach(name draw game forms touch sound stripes)
    if(NOT DEFINED script_${name})
        set(script_${name} ${keys})
    endif()
    add_test(NAME midp.state.${name}
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:jmevm_midp_runner>
            "-DARGS=--classlib|${JMEVM_MIDP_CLASSLIB_JAR}|--epoch|0|--run-ms|6000|--keys|${script_${name}}|${T}/midlets/${name}.jar"
            -DSAVE_MS=1250 -DOUT=${T}/out/state.${name}
            -P ${src}/state_test.cmake)
endforeach()

# The library interface, as an app uses it.
add_executable(midp_embed_test embed_test.c)
target_link_libraries(midp_embed_test PRIVATE jmevm_midp_embed)
jmevm_asan_runtime(midp_embed_test)
foreach(name game stripes)
    add_test(NAME midp.embed.${name}
        COMMAND midp_embed_test ${JMEVM_MIDP_CLASSLIB_JAR} ${T}/midlets/${name}.jar
            ${T}/rms-embed-${name} ${T}/out/embed.${name}.jms)
endforeach()

# The DEFLATE, ZIP and PNG decoders, on the files in util/ (written by
# util/make_fixtures.py), the class library and the test MIDlets.
add_executable(midp_util_test util_test.c)
target_link_libraries(midp_util_test PRIVATE jmevm_midp)
jmevm_asan_runtime(midp_util_test)
add_test(NAME midp.util.inflate COMMAND midp_util_test inflate ${src}/util)
add_test(NAME midp.util.png COMMAND midp_util_test png ${src}/util)
add_test(NAME midp.util.zip COMMAND midp_util_test zip ${src}/util
    ${JMEVM_MIDP_CLASSLIB_JAR} ${midlet_jars})
