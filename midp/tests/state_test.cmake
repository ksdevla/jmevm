# Save-state test (jmevm build): run A plays a MIDlet with scripted keys and
# saves the state at SAVE_MS; run B starts from that state with its own empty
# data folder and the same keys. Every frame after the save point, the
# console output after it and the final screen must be the same in both.
#   cmake -DEXE=jmevm-midp -DARGS=a|b -DSAVE_MS=n -DOUT=dir -P state_test.cmake
string(REPLACE "|" ";" args "${ARGS}")
file(REMOVE_RECURSE ${OUT})
file(MAKE_DIRECTORY ${OUT})

execute_process(
    COMMAND ${EXE} --data ${OUT}/a-data --frame-log ${OUT}/a.frames
        --save-state-at ${SAVE_MS}:${OUT}/state.jms --screenshot ${OUT}/a.ppm ${args}
    OUTPUT_VARIABLE out_a ERROR_VARIABLE out_a RESULT_VARIABLE rc_a TIMEOUT 120)
if(NOT rc_a EQUAL 0 OR NOT EXISTS ${OUT}/state.jms)
    message(FATAL_ERROR "run A failed (exit ${rc_a}) or saved no state\n${out_a}")
endif()

execute_process(
    COMMAND ${EXE} --data ${OUT}/b-data --frame-log ${OUT}/b.frames
        --load-state ${OUT}/state.jms --screenshot ${OUT}/b.ppm ${args}
    OUTPUT_VARIABLE out_b ERROR_VARIABLE out_b RESULT_VARIABLE rc_b TIMEOUT 120)
if(NOT rc_b EQUAL 0)
    message(FATAL_ERROR "run B failed (exit ${rc_b})\n${out_b}")
endif()

# Frames presented after the save point: "ms hash" lines with ms > SAVE_MS.
function(frames_after var file)
    file(STRINGS ${file} lines)
    set(kept "")
    foreach(line IN LISTS lines)
        string(REGEX MATCH "^[0-9]+" ms "${line}")
        if(ms GREATER ${SAVE_MS})
            string(APPEND kept "${line}\n")
        endif()
    endforeach()
    set(${var} "${kept}" PARENT_SCOPE)
endfunction()
frames_after(a ${OUT}/a.frames)
frames_after(b ${OUT}/b.frames)
if(NOT a STREQUAL b)
    message(FATAL_ERROR "frames after the save point differ: compare ${OUT}/a.frames and ${OUT}/b.frames")
endif()
# The screen at the end (with no new frame after the save point, run B shows
# the frame the save state carried).
file(SHA256 ${OUT}/a.ppm sum_a)
file(SHA256 ${OUT}/b.ppm sum_b)
if(NOT sum_a STREQUAL sum_b)
    message(FATAL_ERROR "the final screens differ: compare ${OUT}/a.ppm and ${OUT}/b.ppm")
endif()
# Run B prints only what came after the save; run A must end the same way.
string(REPLACE "\r\n" "\n" out_a "${out_a}")
string(REPLACE "\r\n" "\n" out_b "${out_b}")
string(LENGTH "${out_a}" la)
string(LENGTH "${out_b}" lb)
if(lb GREATER la)
    message(FATAL_ERROR "run B printed more than run A")
endif()
math(EXPR from "${la} - ${lb}")
string(SUBSTRING "${out_a}" ${from} -1 tail_a)
if(NOT tail_a STREQUAL out_b)
    message(FATAL_ERROR "the output after the save point differs\n--- A:\n${out_a}\n--- B:\n${out_b}")
endif()
