
file(
  GLOB
  TEST_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
)

file(
  GLOB
  TEST_HEADER_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/test-common.h
  ${PROJECT_BINARY_DIR}/tests/test-common.h
  COPYONLY
)

set(test_name_list "")

foreach(test_file ${TEST_SOURCE_FILES})
  message(STATUS "Adding test: ${test_file}")

  get_filename_component(test_name ${test_file} NAME_WE)

  add_custom_command(
    OUTPUT "${test_name}.generated.cc"
    COMMAND ${PROJECT_BINARY_DIR}/sanitizer
    ARGS "-p" "${PROJECT_BINARY_DIR}/compile_commands.json"
         "-include-input"
         ">" "${test_name}.generated.cc"
         ${test_file}
    DEPENDS sanitizer
  )

  add_executable(${test_name} ${TEST_HEADER_FILES} "${test_name}.generated.cc")

  # add executable not intended to run but to generate the compile command
  # needed for the utility to build
  add_executable("${test_name}-skip" "${test_name}.cc")

  add_test(${test_name} ${test_name})
  list(APPEND test_name_list ${test_name})

  install(
    FILES ${test_file}
    DESTINATION tests
  )
endforeach()


add_custom_target(
  check
  COMMAND ${CMAKE_CTEST_COMMAND}
  DEPENDS ${test_name_list}
)
