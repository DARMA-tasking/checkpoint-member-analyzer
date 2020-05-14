
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(checkpoint-member-analyzer)

find_program(ccache_binary ccache)
if (ccache_binary)
  message(
    STATUS
    "VT: Found ccache binary: ${ccache_binary}; adding launch rule"
  )
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${ccache_binary}")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  add_compile_options(-fcolor-diagnostics)
endif()

set(CMAKE_CXX_STANDARD 14)

set(CHECKPOINT_MEMBER_ANALYZER_LIBRARY checkpoint-member-analyzer)
set(CHECKPOINT_MEMBER_ANALYZER_LIBRARY_NS vt::lib::checkpoint-member-analyzer)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules/")

#find_package(LLVM REQUIRED 5.0.2)
find_package(Clang REQUIRED)

# set_package_properties(Clang PROPERTIES
#     DESCRIPTION "Clang libraries from the LLVM project. ${clangSearchHint}"
#     PURPOSE "Used for KDevelop's C++/C support plugin."
#     TYPE REQUIRED
# )

# add_library(Clang::clang UNKNOWN IMPORTED)
# set_target_properties(
#   Clang::clang
#   PROPERTIES
#   IMPORTED_LOCATION             ${CLANG_CLANG_LIB}
#   INTERFACE_INCLUDE_DIRECTORIES ${CLANG_INCLUDE_DIRS}
#   INTERFACE_LINK_LIBRARIES      ${CLANG_LIBS}
# )

# add_library(${CHECKPOINT_MEMBER_ANALYZER_LIBRARY_NS} ALIAS ${CHECKPOINT_MEMBER_ANALYZER_LIBRARY})
# add_library(${CHECKPOINT_MEMBER_ANALYZER_LIBRARY} INTERFACE)
# add_library(${CHECKPOINT_MEMBER_ANALYZER_LIBRARY_NS} ALIAS ${CHECKPOINT_MEMBER_ANALYZER_LIBRARY})

file(
  GLOB
  HEADER_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

file(
  GLOB
  SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
)

# add_library(
#   ${CHECKPOINT_MEMBER_ANALYZER_LIBRARY}
#   STATIC
#   ${HEADER_FILES} ${SOURCE_FILES}
# )

add_executable(
  checker
  ${HEADER_FILES} ${SOURCE_FILES}
)

install(FILES ${HEADER_FILES} DESTINATION include)

# target_include_directories(
#   ${CHECKPOINT_MEMBER_ANALYZER_LIBRARY} PUBLIC
#   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
#   $<INSTALL_INTERFACE:include>
# )

# target_include_directories(
#   ${CHECKPOINT_MEMBER_ANALYZER_LIBRARY} PUBLIC ${CLANG_INCLUDE_DIRS}
# )

target_include_directories(
  checker PUBLIC ${CLANG_INCLUDE_DIRS}
)

set_target_properties(
  checker PROPERTIES COMPILE_FLAGS ${LLVM_CXXFLAGS}
)

set_target_properties(
  checker PROPERTIES LINK_FLAGS "${LLVM_LFLAGS} ${LLVM_SYSLIBS} ${LLVM_LIBS}"
)

target_link_libraries(
  checker PUBLIC ${CLANG_LIBS}
)

target_link_libraries(
  checker PUBLIC clangASTMatchers clangToolingCore
)
