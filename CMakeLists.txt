
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(serialization-sanitizer)

find_program(ccache_binary ccache)
if (ccache_binary)
  message(
    STATUS
    "Sanitizer: Found ccache binary: ${ccache_binary}; adding launch rule"
  )
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${ccache_binary}")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  add_compile_options(-fcolor-diagnostics)
endif()

set(CMAKE_CXX_STANDARD 14)

set(SERIALIZATION_SANITIZER_LIBRARY serialization-sanitizer)
set(SERIALIZATION_SANITIZER_LIBRARY_NS vt::lib::serialization-sanitizer)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules/")

find_package(Clang REQUIRED)

# include fmt in the build
add_subdirectory(lib/fmt)

file(
  GLOB
  HEADER_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/sanitizer/*.h
)

file(
  GLOB
  SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/sanitizer/*.cc
)
add_executable(
  sanitizer
  ${HEADER_FILES} ${SOURCE_FILES}
)

install(FILES ${HEADER_FILES} DESTINATION include)

target_compile_definitions(
  sanitizer PUBLIC FMT_HEADER_ONLY=1 FMT_USE_USER_DEFINED_LITERALS=0
)
target_include_directories(
  sanitizer PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/fmt>
  $<INSTALL_INTERFACE:include/fmt>
)


# target_include_directories(
#   ${SERIALIZATION_SANITIZER_LIBRARY} PUBLIC
#   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
#   $<INSTALL_INTERFACE:include>
# )

# target_include_directories(
#   ${SERIALIZATION_SANITIZER_LIBRARY} PUBLIC ${CLANG_INCLUDE_DIRS}
# )

target_include_directories(
  sanitizer PUBLIC ${CLANG_INCLUDE_DIRS}
)

set_target_properties(
  sanitizer PROPERTIES COMPILE_FLAGS ${LLVM_CXXFLAGS}
)

set_target_properties(
  sanitizer PROPERTIES LINK_FLAGS "${LLVM_LFLAGS} ${LLVM_SYSLIBS} ${LLVM_LIBS}"
)

target_link_libraries(
  sanitizer PUBLIC ${CLANG_LIBS}
)

target_link_libraries(
  sanitizer PUBLIC clangASTMatchers clangToolingCore clangLex clangParse clangSema
)

install(
  TARGETS sanitizer
  RUNTIME DESTINATION bin
)

install(
  FILES scripts/parse-json.pl scripts/transform.sh
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
  DESTINATION scripts
)

###############################################################################
# Build for sanitizer runtime tests
###############################################################################

include(CTest)
enable_testing()

add_subdirectory(tests)

###############################################################################
# Build for sanitizer runtime that links with checkpoint
###############################################################################

file(
  GLOB
  RT_HEADER_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/*.h
)

file(
  GLOB
  RT_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/*.cc
)

add_library(
  sanitizer_rt
  SHARED
  ${RT_HEADER_FILES} ${RT_SOURCE_FILES}
)

add_library(vt::lib::sanitizer_rt ALIAS sanitizer_rt)

target_compile_definitions(
  sanitizer_rt PUBLIC FMT_HEADER_ONLY=1 FMT_USE_USER_DEFINED_LITERALS=0
)
target_include_directories(
  sanitizer_rt PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/fmt>
  $<INSTALL_INTERFACE:include/fmt>
)

install(
  TARGETS                   sanitizer_rt
  EXPORT                    sanitizer_rt
  LIBRARY DESTINATION       lib
  ARCHIVE DESTINATION       lib
  RUNTIME DESTINATION       bin
  INCLUDES DESTINATION      include
)

# install(
#   EXPORT                    sanitizer_rt
#   DESTINATION               cmake
#   FILE                      "checkpointTargets.cmake"
#   NAMESPACE                 vt::lib::
#   COMPONENT                 runtime
# )

# install(
#   TARGETS sanitizer_rt
#   RUNTIME DESTINATION bin
# )
