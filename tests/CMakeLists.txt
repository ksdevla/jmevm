# --- Unit tests (C, no JDK needed)
file(GLOB unit_sources CONFIGURE_DEPENDS unit/*.c)
add_executable(jmevm_unit ${unit_sources})
target_include_directories(jmevm_unit PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(jmevm_unit PRIVATE jmevm)
jmevm_asan_runtime(jmevm_unit)
add_test(NAME unit COMMAND jmevm_unit)

add_test(NAME cli.version COMMAND jmevm_cli --version)
set_tests_properties(cli.version PROPERTIES PASS_REGULAR_EXPRESSION "^jmevm ${PROJECT_VERSION}")

if(NOT JMEVM_CLASSLIB)
    message(STATUS "Class library not built: Java tests are skipped")
    return()
endif()

# --- Java programs, compiled against the class library. Their output must
# match what a desktop JVM printed (the .expected files).
set(T ${CMAKE_CURRENT_BINARY_DIR})
set(src ${CMAKE_CURRENT_SOURCE_DIR})
set(kit ${src}/kit)
set(boot -bootclasspath ${JMEVM_CLASSLIB_CLASSES})

# tests/gen holds desktop Java programs that write class files javac cannot
# produce: malformed code (Bad1-6, next to the test classes) and jsr/ret.
file(GLOB java_tests CONFIGURE_DEPENDS ${src}/java/*.java)
set(java_sources ${java_tests} ${kit}/VmTest.java ${kit}/BytecodeTest.java)
add_custom_command(
    OUTPUT ${T}/java.stamp
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${T}/java
    COMMAND ${CMAKE_COMMAND} -E make_directory ${T}/java
    COMMAND ${Java_JAVAC_EXECUTABLE} ${JMEVM_JAVAC_FLAGS} ${boot} -d ${T}/java ${java_sources}
    COMMAND ${Java_JAVA_EXECUTABLE} ${src}/gen/MakeBadClasses.java ${T}/java
    COMMAND ${CMAKE_COMMAND} -E touch ${T}/java.stamp
    DEPENDS ${java_sources} ${src}/gen/MakeBadClasses.java classlib
    VERBATIM)

# jsr/ret in a class file version 45.3.
add_custom_command(
    OUTPUT ${T}/jsr/JsrTest.class
    COMMAND ${CMAKE_COMMAND} -E make_directory ${T}/jsr
    COMMAND ${Java_JAVA_EXECUTABLE} ${src}/gen/MakeJsrClass.java ${T}/jsr/JsrTest.class
    DEPENDS ${src}/gen/MakeJsrClass.java
    VERBATIM)

# The stripes MIDlet against pure-Java MIDP stubs.
file(GLOB_RECURSE stub_sources CONFIGURE_DEPENDS ${kit}/stubs/*.java)
add_custom_command(
    OUTPUT ${T}/headless.stamp
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${T}/headless
    COMMAND ${CMAKE_COMMAND} -E make_directory ${T}/headless
    COMMAND ${Java_JAVAC_EXECUTABLE} ${JMEVM_JAVAC_FLAGS} ${boot} -d ${T}/headless
        ${stub_sources} ${kit}/stripes/src/TestMidlet.java
    COMMAND ${CMAKE_COMMAND} -E touch ${T}/headless.stamp
    DEPENDS ${stub_sources} ${kit}/stripes/src/TestMidlet.java classlib
    VERBATIM)

add_custom_target(test_programs ALL DEPENDS ${T}/java.stamp ${T}/jsr/JsrTest.class ${T}/headless.stamp)

# Every class of the class library and the tests must parse.
add_test(NAME parse.classes
    COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:jmevm_cli>
        "-DDIRS=${JMEVM_CLASSLIB_CLASSES}|${T}/java|${T}/jsr|${T}/headless"
        -P ${src}/parse_all.cmake)

# java_test(<name> ARGS <runner args...> [EXPECTED <file>] [LINES <n>]
#           [EXIT_CODE <n>] [REPEAT [REPEAT_ARGS <args...>]] [DISABLED])
function(java_test name)
    cmake_parse_arguments(P "DISABLED;REPEAT" "EXPECTED;LINES;EXIT_CODE" "ARGS;REPEAT_ARGS" ${ARGN})
    string(REPLACE ";" "|" args "${P_ARGS}")
    string(REPLACE ";" "|" repeat_args "${P_REPEAT_ARGS}")
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:jmevm_cli> "-DARGS=${args}"
            -DEXPECTED=${P_EXPECTED} -DLINES=${P_LINES} -DEXIT_CODE=${P_EXIT_CODE}
            -DREPEAT=${P_REPEAT} "-DREPEAT_ARGS=${repeat_args}"
            -DOUTPUT=${T}/out/${name}.txt -P ${src}/run_test.cmake)
    if(P_DISABLED)
        set_tests_properties(${name} PROPERTIES DISABLED TRUE)
    endif()
endfunction()

# Each test also runs with a collection before every allocation in a small
# heap, so a reference the VM failed to update makes it fail.
function(java_test_both name)
    cmake_parse_arguments(P "DISABLED" "EXPECTED;LINES;EXIT_CODE" "ARGS" ${ARGN})
    set(extra "")
    if(P_DISABLED)
        set(extra DISABLED)
    endif()
    if(P_LINES)
        list(APPEND extra LINES ${P_LINES})
    endif()
    if(P_EXIT_CODE)
        list(APPEND extra EXIT_CODE ${P_EXIT_CODE})
    endif()
    java_test(${name} ARGS ${P_ARGS} EXPECTED ${P_EXPECTED} ${extra})
    java_test(stress.${name} ARGS --gc-stress --heap 2 ${P_ARGS} EXPECTED ${P_EXPECTED} ${extra})
endfunction()

set(lib --boot ${JMEVM_CLASSLIB_CLASSES})

foreach(name TestLang TestText TestUtil TestGc TestVm TestConcurrency TestThreadInit)
    java_test_both(java.${name} ARGS ${lib} -cp ${T}/java ${name} EXPECTED ${src}/java/${name}.expected)
endforeach()
# No thread can continue: reported, exit code 3 (expected output by hand).
java_test(java.TestDeadlock ARGS ${lib} -cp ${T}/java TestDeadlock
    EXPECTED ${src}/java/TestDeadlock.expected EXIT_CODE 3)
# An uncaught exception is printed like a desktop JVM does, and exits with 1.
java_test(java.TestUncaught ARGS ${lib} -cp ${T}/java TestUncaught
    EXPECTED ${src}/java/TestUncaught.expected EXIT_CODE 1)
# The host API through the runner's test host (expected output by hand).
java_test_both(java.TestHost ARGS --test-host ${lib} -cp ${T}/java TestHost
    EXPECTED ${src}/java/TestHost.expected EXIT_CODE 3)
# Malformed bytecode is rejected (expected output by hand).
java_test_both(java.TestVerify ARGS ${lib} -cp ${T}/java TestVerify EXPECTED ${src}/java/TestVerify.expected)
# Out of memory and weak references, in a 1 MB heap (expected output by hand).
java_test_both(java.TestHeapLimits ARGS ${lib} --heap 1 -cp ${T}/java TestHeapLimits
    EXPECTED ${src}/java/TestHeapLimits.expected)
java_test_both(kit.JsrTest ARGS ${lib} -cp ${T}/jsr JsrTest EXPECTED ${kit}/JsrTest.expected)

java_test_both(kit.VmTest ARGS ${lib} -cp ${T}/java VmTest EXPECTED ${kit}/VmTest.expected)
java_test_both(kit.BytecodeTest ARGS ${lib} -cp ${T}/java BytecodeTest EXPECTED ${kit}/BytecodeTest.expected)
set(stripes -cp ${T}/headless javax.microedition.midlet.HeadlessRunner TestMidlet 20)
java_test_both(kit.StripesHeadless ARGS ${lib} ${stripes} EXPECTED ${kit}/StripesHeadless.expected)

# Determinism: a second run, here with a collection before every allocation
# in a different heap size, must print exactly the same, down to the
# emulated time the program ended at. TestInterleave races threads on
# purpose; on a desktop JVM its output varies.
set(again REPEAT REPEAT_ARGS --gc-stress --heap 3)
java_test(determinism.TestInterleave ARGS --print-ticks ${lib} -cp ${T}/java TestInterleave ${again})
java_test(determinism.TestConcurrency ARGS --print-ticks ${lib} -cp ${T}/java TestConcurrency ${again})
java_test(determinism.BytecodeTest ARGS --print-ticks ${lib} -cp ${T}/java BytecodeTest ${again})
java_test(determinism.stripes ARGS --print-ticks ${lib} ${stripes} ${again})

# Snapshots: the program runs with a snapshot every STEP ticks. Each one must
# load into a fresh VM, save back to the same bytes, and run to the end with
# the same output, exit code and final tick; damaged snapshots must be
# refused. The rewind variant checks every entry of a rewind ring. Steps are
# chosen for 20-50 snapshots per program.
# The history variant records the every-frame history at each snapshot and
# steps back through it: every step must give the snapshot exactly, and the
# program must go on from the middle one like the full run.
# snapshot_test(<name> <step> ARGS <runner args...> [EXPECTED <file>] [EXIT_CODE <n>])
function(snapshot_test name step)
    cmake_parse_arguments(P "" "EXPECTED;EXIT_CODE" "ARGS" ${ARGN})
    set(extra "")
    if(P_EXPECTED)
        list(APPEND extra EXPECTED ${P_EXPECTED})
    endif()
    if(P_EXIT_CODE)
        list(APPEND extra EXIT_CODE ${P_EXIT_CODE})
    endif()
    java_test(snapshot.${name} ARGS --snapshot-check ${step} ${P_ARGS} ${extra})
    java_test(rewind.${name} ARGS --rewind-check ${step} ${P_ARGS} ${extra})
    java_test(history.${name} ARGS --history-check ${step} ${P_ARGS} ${extra})
endfunction()

set(j ${lib} -cp ${T}/java)
snapshot_test(TestLang 500 ARGS ${j} TestLang EXPECTED ${src}/java/TestLang.expected)
snapshot_test(TestText 5000 ARGS ${j} TestText EXPECTED ${src}/java/TestText.expected)
snapshot_test(TestUtil 1500 ARGS ${j} TestUtil EXPECTED ${src}/java/TestUtil.expected)
snapshot_test(TestGc 5000 ARGS ${j} TestGc EXPECTED ${src}/java/TestGc.expected)
snapshot_test(TestVm 100 ARGS ${j} TestVm EXPECTED ${src}/java/TestVm.expected)
snapshot_test(TestVerify 30 ARGS ${j} TestVerify EXPECTED ${src}/java/TestVerify.expected)
snapshot_test(TestHeapLimits 15 ARGS ${lib} --heap 1 -cp ${T}/java TestHeapLimits
    EXPECTED ${src}/java/TestHeapLimits.expected)
snapshot_test(TestUncaught 2 ARGS ${j} TestUncaught EXPECTED ${src}/java/TestUncaught.expected EXIT_CODE 1)
snapshot_test(TestConcurrency 100000 ARGS ${j} TestConcurrency EXPECTED ${src}/java/TestConcurrency.expected)
snapshot_test(TestThreadInit 30000 ARGS ${j} TestThreadInit EXPECTED ${src}/java/TestThreadInit.expected)
snapshot_test(TestInterleave 6000 ARGS ${j} TestInterleave)
snapshot_test(VmTest 9000 ARGS ${j} VmTest EXPECTED ${kit}/VmTest.expected)
snapshot_test(BytecodeTest 20000 ARGS ${j} BytecodeTest EXPECTED ${kit}/BytecodeTest.expected)
snapshot_test(stripes 400000 ARGS ${lib} ${stripes} EXPECTED ${kit}/StripesHeadless.expected)
# With host natives, host state and input fed between steps.
snapshot_test(TestHost 10000 ARGS --test-host ${j} TestHost EXPECTED ${src}/java/TestHost.expected EXIT_CODE 3)
