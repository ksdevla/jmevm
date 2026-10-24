# Runs one test:
#   cmake -DEXE=... -DARGS=a|b|c -DOUTPUT=file -DWORKDIR=dir
#         [-DEXPECTED=file] [-DDATA=dir] [-DFRAMES=file [-DEXPECTED_FRAMES=file]]
#         [-DREPEAT=ON [-DREPLAY=ON]] -P run_test.cmake
# Fails when the runner exits with an error, prints "no frame to save", or
# its combined stdout/stderr differs from EXPECTED. FRAMES passes
# --frame-log FRAMES to the runner; its log is compared with EXPECTED_FRAMES.
# REPEAT runs the program a second time and requires identical output and
# frame log: the emulator must be deterministic. With REPLAY, the first run
# records its input (--record) and the second run gets no --keys, only that
# recording (--replay).

function(run_once out_var frames mode)
    if(DATA)
        file(REMOVE_RECURSE ${DATA})
    endif()
    string(REPLACE "|" ";" args "${ARGS}")
    if(frames)
        list(APPEND args --frame-log ${frames})
    endif()
    if(mode STREQUAL "record")
        list(APPEND args --record ${OUTPUT}.rec)
    elseif(mode STREQUAL "replay")
        list(FIND args --keys k)
        if(k GREATER -1)
            math(EXPR k1 "${k} + 1")
            list(REMOVE_AT args ${k} ${k1})
        endif()
        list(APPEND args --replay ${OUTPUT}.rec)
    endif()
    execute_process(
        COMMAND ${EXE} ${args}
        WORKING_DIRECTORY ${WORKDIR}
        OUTPUT_VARIABLE out
        ERROR_VARIABLE out
        RESULT_VARIABLE rc
        TIMEOUT 120)
    string(REPLACE "\r\n" "\n" out "${out}")
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "exit code ${rc}\n${out}")
    endif()
    if(out MATCHES "no frame to save")
        message(FATAL_ERROR "the program never presented a frame\n${out}")
    endif()
    set(${out_var} "${out}" PARENT_SCOPE)
endfunction()

function(read_text var path)
    file(READ ${path} text)
    string(REPLACE "\r\n" "\n" text "${text}")
    set(${var} "${text}" PARENT_SCOPE)
endfunction()

get_filename_component(out_dir ${OUTPUT} DIRECTORY)
file(MAKE_DIRECTORY ${out_dir})

if(REPLAY)
    run_once(out "${FRAMES}" record)
else()
    run_once(out "${FRAMES}" "")
endif()
file(WRITE ${OUTPUT} "${out}")

if(EXPECTED)
    read_text(expected ${EXPECTED})
    if(NOT out STREQUAL expected)
        message(FATAL_ERROR "output differs from ${EXPECTED}\n"
            "--- got (saved in ${OUTPUT}):\n${out}")
    endif()
endif()
if(FRAMES AND EXPECTED_FRAMES)
    read_text(frames ${FRAMES})
    read_text(expected ${EXPECTED_FRAMES})
    if(NOT frames STREQUAL expected)
        message(FATAL_ERROR "frame log ${FRAMES} differs from ${EXPECTED_FRAMES}")
    endif()
endif()

if(REPEAT)
    set(frames2 "")
    if(FRAMES)
        set(frames2 ${FRAMES}.2)
    endif()
    if(REPLAY)
        run_once(out2 "${frames2}" replay)
    else()
        run_once(out2 "${frames2}" "")
    endif()
    file(WRITE ${OUTPUT}.2 "${out2}")
    if(NOT out STREQUAL out2)
        message(FATAL_ERROR "a second run printed something else: compare ${OUTPUT} and ${OUTPUT}.2")
    endif()
    if(FRAMES)
        read_text(a ${FRAMES})
        read_text(b ${frames2})
        if(NOT a STREQUAL b)
            message(FATAL_ERROR "a second run presented different frames: compare ${FRAMES} and ${frames2}")
        endif()
        if(a STREQUAL "")
            message(FATAL_ERROR "no frames were presented")
        endif()
    endif()
endif()
