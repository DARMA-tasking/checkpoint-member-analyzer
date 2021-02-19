
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

  # FIXME: it would be better to add clang flags (per target?), but we have to
  # make sure that the plugin is built first
  # note: add_dependencies(${test_name} sanitizer) doesn't seem to work
  add_custom_command(
    OUTPUT "${test_name}.generated.cc"
    COMMAND clang++
    ARGS "-std=c++14" "-c" ${test_file}
         "-Xclang" "-load" "-Xclang" "${PROJECT_BINARY_DIR}/libsanitizer.so"
         "-Xclang" "-plugin" "-Xclang" "sanitizer"
         "-Xclang" "-plugin-arg-sanitizer" "-Xclang" "-include-input"
         "-Xclang" "-plugin-arg-sanitizer" "-Xclang" "-o"
         "-Xclang" "-plugin-arg-sanitizer" "-Xclang" "${test_name}.generated.cc"
    DEPENDS sanitizer
  )

  add_executable(${test_name} ${TEST_HEADER_FILES} "${test_name}.generated.cc")

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
