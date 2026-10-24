# The class library: CLDC 1.1 (java.*) and the VM's own glue (jmevm.*),
# compiled against itself into a class directory that `jmevm --boot` reads.
file(GLOB_RECURSE classlib_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.java)

set(classes ${CMAKE_CURRENT_BINARY_DIR}/classes)
set(JMEVM_CLASSLIB_CLASSES ${classes} PARENT_SCOPE)
set(argfile ${CMAKE_CURRENT_BINARY_DIR}/sources.txt)
set(stamp ${CMAKE_CURRENT_BINARY_DIR}/classes.stamp)

set(lines "")
foreach(f IN LISTS classlib_sources)
    string(APPEND lines "\"${f}\"\n")
endforeach()
file(WRITE ${argfile} "${lines}")

add_custom_command(
    OUTPUT ${stamp}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${classes}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${classes}
    COMMAND ${Java_JAVAC_EXECUTABLE} ${JMEVM_JAVAC_FLAGS} -bootclasspath ${classes} -d ${classes} @${argfile}
    COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
    DEPENDS ${classlib_sources}
    COMMENT "Compiling the class library"
    VERBATIM)
add_custom_target(classlib ALL DEPENDS ${stamp})
