# Runs one program and compares its combined stdout/stderr with EXPECTED.
#   cmake -DEXE=jmevm -DARGS=a|b|c -DOUTPUT=file [-DEXPECTED=file] [-DLINES=n]
#         [-DEXIT_CODE=n] [-DREPEAT=ON [-DREPEAT_ARGS=x|y]] -P run_test.cmake
# The exit code must be EXIT_CODE (default 0). With LINES, only the first n
# lines are compared and the exit code is not checked. REPEAT runs the
# program a second time, with REPEAT_ARGS put before ARGS, and requires
# exactly the same output: the VM must be deterministic.
string(REPLACE "|" ";" args "${ARGS}")
get_filename_component(out_dir ${OUTPUT} DIRECTORY)
file(MAKE_DIRECTORY ${out_dir})
if(NOT EXIT_CODE)
    set(EXIT_CODE 0)
endif()

function(run_once out_var file)
    execute_process(
        COMMAND ${EXE} ${ARGN}
        OUTPUT_VARIABLE out
        ERROR_VARIABLE out
        RESULT_VARIABLE rc
        TIMEOUT 300)
    string(REPLACE "\r\n" "\n" out "${out}")
    file(WRITE ${file} "${out}")
    if(NOT LINES AND NOT rc EQUAL EXIT_CODE)
        message(FATAL_ERROR "exit code ${rc}, expected ${EXIT_CODE}\n${out}")
    endif()
    set(${out_var} "${out}" PARENT_SCOPE)
endfunction()

run_once(out ${OUTPUT} ${args})
if(REPEAT)
    string(REPLACE "|" ";" repeat_args "${REPEAT_ARGS}")
    run_once(out2 ${OUTPUT}.2 ${repeat_args} ${args})
    if(NOT out STREQUAL out2)
        message(FATAL_ERROR "a second run printed something else: compare ${OUTPUT} and ${OUTPUT}.2")
    endif()
endif()

# The first n lines of text, each ending in a newline.
function(first_lines var text n)
    set(result "")
    set(rest "${text}")
    foreach(i RANGE 1 ${n})
        string(FIND "${rest}" "\n" nl)
        if(nl EQUAL -1)
            string(APPEND result "${rest}")
            break()
        endif()
        math(EXPR len "${nl} + 1")
        string(SUBSTRING "${rest}" 0 ${len} line)
        string(APPEND result "${line}")
        string(SUBSTRING "${rest}" ${len} -1 rest)
    endforeach()
    set(${var} "${result}" PARENT_SCOPE)
endfunction()

if(EXPECTED)
    file(READ ${EXPECTED} expected)
    string(REPLACE "\r\n" "\n" expected "${expected}")
    if(LINES)
        first_lines(expected "${expected}" ${LINES})
        first_lines(got "${out}" ${LINES})
    else()
        set(got "${out}")
    endif()
    if(NOT got STREQUAL expected)
        message(FATAL_ERROR "output differs from ${EXPECTED}\n--- got (saved in ${OUTPUT}):\n${out}")
    endif()
endif()
