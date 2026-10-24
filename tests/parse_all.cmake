# Runs `jmevm --dump-class` on every .class file under DIRS (separated by |).
#   cmake -DEXE=jmevm -DDIRS=a|b -P parse_all.cmake
string(REPLACE "|" ";" dirs "${DIRS}")
set(classes "")
foreach(d IN LISTS dirs)
    file(GLOB_RECURSE found ${d}/*.class)
    list(APPEND classes ${found})
endforeach()
list(LENGTH classes count)
if(count EQUAL 0)
    message(FATAL_ERROR "no class files found in ${DIRS}")
endif()

execute_process(
    COMMAND ${EXE} --dump-class ${classes}
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "some class files did not parse:\n${err}")
endif()
message(STATUS "${count} class files parsed")
