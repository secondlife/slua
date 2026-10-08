cmake_minimum_required(VERSION 3.10)
project(Tailslide LANGUAGES CXX)

include(GNUInstallDirs)

if (MSVC)
  # MSVC requires C++20 be enabled to allow designated initializers
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(_IGNORED "${CMAKE_C_COMPILER}")

option(TAILSLIDE_CI_RUN "Are we running in CI" OFF)
option(TAILSLIDE_BUILD_CLI "Build CLI" ON)
option(TAILSLIDE_BUILD_TESTS "Build Tests" ON)
option(TAILSLIDE_BUILD_FUZZER "Build Fuzzer" OFF)
option(TAILSLIDE_SANITIZE "Use ASAN" OFF)
option(TAILSLIDE_FUZZER_INSTRUMENTATION "Add instrumentation for libFuzzer" OFF)
option(TAILSLIDE_COVERAGE "Track coverage data in tests" OFF)
option(TAILSLIDE_SKIP_GEN "Skip flex/bison generation" OFF)

if (MSVC)
  # So?
  add_definitions("-D_CRT_SECURE_NO_WARNINGS=1")
else()
  # Don't whine about using sprintf() in doctest
  add_definitions("-Wall -Wno-deprecated-declarations")
  if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_definitions("-Wno-unneeded-internal-declaration")
  endif()
  if (TAILSLIDE_CI_RUN)
    add_definitions("-Werror")
  endif()
endif()


if (TAILSLIDE_BUILD_FUZZER)
  set(TAILSLIDE_SANITIZE ON)
  set(TAILSLIDE_FUZZER_INSTRUMENTATION ON)
endif ()

# On macOS, search Homebrew for keg-only versions of Bison and Flex. Xcode does
# not provide new enough versions for us to use.
if (CMAKE_HOST_SYSTEM_NAME MATCHES "Darwin")
  execute_process(
          COMMAND brew --prefix bison
          RESULT_VARIABLE BREW_BISON
          OUTPUT_VARIABLE BREW_BISON_PREFIX
          OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if (BREW_BISON EQUAL 0 AND EXISTS "${BREW_BISON_PREFIX}")
    message(STATUS "Found Bison keg installed by Homebrew at ${BREW_BISON_PREFIX}")
    set(BISON_EXECUTABLE "${BREW_BISON_PREFIX}/bin/bison")
  endif()

  execute_process(
          COMMAND brew --prefix flex
          RESULT_VARIABLE BREW_FLEX
          OUTPUT_VARIABLE BREW_FLEX_PREFIX
          OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if (BREW_FLEX EQUAL 0 AND EXISTS "${BREW_FLEX_PREFIX}")
    message(STATUS "Found Flex keg installed by Homebrew at ${BREW_FLEX_PREFIX}")
    set(FLEX_EXECUTABLE "${BREW_FLEX_PREFIX}/bin/flex")
  endif()
endif()

# Optional, the generated scanner and parser are checked in, see below.
find_package(BISON)
find_package(FLEX)

set(EXTRA_LIBS "")
if(UNIX AND NOT APPLE)
  set(EXTRA_LIBS "stdc++" "rt" "m")
endif()

if (TAILSLIDE_SANITIZE)
  add_compile_options("-fsanitize=address" "-O1" "-g")
  add_link_options("-fsanitize=address" "-g")
endif()

if (TAILSLIDE_FUZZER_INSTRUMENTATION)
  add_compile_options("-fsanitize=fuzzer")
  if (NOT TAILSLIDE_SANITIZE)
    add_link_options("-fsanitize=fuzzer-no-link")
  endif()
endif()

if (TAILSLIDE_COVERAGE)
  if ( NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang") )
    message(FATAL_ERROR "Clang must be used to enable coverage tracking, not ${CMAKE_CXX_COMPILER_ID}")
  endif()
  add_compile_options("-fprofile-instr-generate" "-fcoverage-mapping")
  add_link_options("-fprofile-instr-generate")
endif()

# Embed builtins.txt into a header file at configure time
# file(READ) doesn't register a configure dependency on its own, so add one explicitly
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/builtins.txt")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/builtins.txt" BUILTINS_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " BUILTINS_HEX_ARRAY "${BUILTINS_HEX}")
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/libtailslide/builtins_embedded.hh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/builtins_embedded.hh"
        @ONLY
)

add_library(libtailslide STATIC
        libtailslide/allocator.cc
        libtailslide/ast.cc
        libtailslide/builtins.cc
        libtailslide/logger.cc
        libtailslide/lslmini.cc
        libtailslide/operations.cc
        libtailslide/strings.cc
        libtailslide/symtab.cc
        libtailslide/types.cc
        libtailslide/visitor.cc
        libtailslide/passes/globalexpr_validator.cc
        libtailslide/passes/final_pass.cc
        libtailslide/passes/desugaring.cc
        libtailslide/passes/pretty_print.cc
        libtailslide/passes/symbol_resolution.cc
        libtailslide/passes/type_checking.cc
        libtailslide/passes/tree_print.cc
        libtailslide/passes/tree_simplifier.cc
        libtailslide/passes/constant_expression_simplifier.cc
        libtailslide/passes/values.cc
        libtailslide/passes/lso/bytecode_compiler.cc
        libtailslide/passes/lso/library_funcs.cc
        libtailslide/passes/lso/script_compiler.cc
        libtailslide/passes/lso/resource_collector.cc
        libtailslide/passes/mono/resource_collector.cc
        libtailslide/passes/mono/script_compiler.cc
        libtailslide/tailslide.cc
        )
target_sources(libtailslide PRIVATE
        libtailslide/allocator.hh
        libtailslide/ast.hh
        libtailslide/bitstream.hh
        libtailslide/builtins_embedded.hh.in
        libtailslide/loctype.hh
        libtailslide/logger.hh
        libtailslide/lslmini.hh
        libtailslide/operations.hh
        libtailslide/portable_endian.hh
        libtailslide/strings.hh
        libtailslide/symtab.hh
        libtailslide/types.hh
        libtailslide/unordered_cstr_map.hh
        libtailslide/visitor.hh
        libtailslide/passes/globalexpr_validator.hh
        libtailslide/passes/final_pass.hh
        libtailslide/passes/desugaring.hh
        libtailslide/passes/pretty_print.hh
        libtailslide/passes/symbol_resolution.hh
        libtailslide/passes/tree_simplifier.hh
        libtailslide/passes/tree_print.hh
        libtailslide/passes/type_checking.hh
        libtailslide/passes/values.hh
        libtailslide/passes/lso/bytecode_compiler.hh
        libtailslide/passes/lso/bytecode_format.hh
        libtailslide/passes/lso/library_funcs.hh
        libtailslide/passes/lso/script_compiler.hh
        libtailslide/passes/lso/resource_collector.hh
        libtailslide/passes/mono/resource_collector.hh
        libtailslide/passes/mono/script_compiler.hh
        libtailslide/tailslide.hh
)

string(TIMESTAMP BUILD_DATE "%Y-%m-%d")
add_definitions(-DBUILD_DATE="${BUILD_DATE}")
add_definitions(-DVERSION="0.1dev")

# The generated scanner and parser are checked in so that flex and bison aren't
# hard build dependencies. When they are available, regenerate in-tree so the
# checked-in copies stay current.
set(LSLMINI_SCANNER libtailslide/lslmini.flex.cc)
set(LSLMINI_PARSER libtailslide/lslmini.tab.cc)
set(LSLMINI_PARSER_HEADER libtailslide/lslmini.tab.hh)
if (BISON_FOUND AND FLEX_FOUND AND NOT TAILSLIDE_SKIP_GEN)
  # Run from the source dir with relative paths so the `#line` directives in the
  # generated files don't embed this machine's absolute paths.
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${LSLMINI_PARSER} ${CMAKE_CURRENT_SOURCE_DIR}/${LSLMINI_PARSER_HEADER}
    COMMAND ${BISON_EXECUTABLE} --defines=${LSLMINI_PARSER_HEADER} -o ${LSLMINI_PARSER} libtailslide/lslmini.y
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libtailslide/lslmini.y
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "[BISON] Building parser with bison ${BISON_VERSION}"
    VERBATIM
  )
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${LSLMINI_SCANNER}
    COMMAND ${FLEX_EXECUTABLE} -o ${LSLMINI_SCANNER} libtailslide/lslmini.l
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libtailslide/lslmini.l ${CMAKE_CURRENT_SOURCE_DIR}/${LSLMINI_PARSER_HEADER}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "[FLEX] Building scanner with flex ${FLEX_VERSION}"
    VERBATIM
  )
else()
  message(STATUS "Not using flex and/or bison, using the checked-in scanner and parser")
endif()

target_include_directories(libtailslide PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(libtailslide PUBLIC libtailslide)
target_sources(libtailslide PRIVATE ${LSLMINI_SCANNER} ${LSLMINI_PARSER} ${LSLMINI_PARSER_HEADER})

# Suppress warning about unused yynerrs variable in Bison-generated code
if (NOT MSVC)
  set_source_files_properties(
    ${LSLMINI_PARSER}
    PROPERTIES
    COMPILE_FLAGS "-Wno-unused-but-set-variable"
  )
  # GCC doesn't support the FP_CONTRACT pragma, so we have to use this to tell it it not to
  # fuse multiply and add for these files.
  set_source_files_properties(
    libtailslide/operations.cc
    PROPERTIES
    COMPILE_FLAGS "-ffp-contract=off"
  )
endif()

set_target_properties(libtailslide PROPERTIES OUTPUT_NAME tailslide)
if (NOT WIN32 AND NOT TAILSLIDE_FUZZER_INSTRUMENTATION)
  add_definitions("-fPIC")
endif()


install(TARGETS libtailslide DESTINATION lib)
install(DIRECTORY libtailslide/ DESTINATION include/tailslide FILES_MATCHING PATTERN "*.hh")


if (TAILSLIDE_BUILD_CLI)
  add_executable(tailslide_cli tailslide_cli/main.cc)
  target_sources(tailslide_cli PRIVATE
          extern/cxxopt.hh
  )
  target_include_directories(tailslide_cli PUBLIC ${CMAKE_CURRENT_BINARY_DIR} libtailslide extern)
  target_link_libraries(tailslide_cli PUBLIC ${EXTRA_LIBS} libtailslide)
  set_target_properties(tailslide_cli PROPERTIES OUTPUT_NAME tailslide)
endif()


if (TAILSLIDE_BUILD_TESTS)
  add_executable(tailslide_test
          tests/conformance.cc
          tests/ast_rewriting.cc
          tests/lso_compilation.cc
          tests/cil_compilation.cc
          tests/testutils.cc
          tests/main.cc
          tests/unit_tests.cc
  )
  target_sources(tailslide_test PRIVATE
    extern/doctest.hh
    tests/testutils.hh
  )
  target_include_directories(tailslide_test PUBLIC ${CMAKE_CURRENT_BINARY_DIR} libtailslide extern)
  target_link_libraries(tailslide_test PUBLIC ${EXTRA_LIBS} libtailslide)
  set_target_properties(tailslide_test PROPERTIES OUTPUT_NAME tailslide-test)
endif()


if (TAILSLIDE_BUILD_FUZZER)
  if ( NOT UNIX )
    message(FATAL_ERROR "The libFuzzer harness is only supported under *NIX!")
  endif()
  if ( NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang") )
    message(FATAL_ERROR "The libFuzzer harness is only supported under Clang, not ${CMAKE_CXX_COMPILER_ID}")
  endif()

  add_executable(tailslide_fuzzer
    fuzz/fuzz.cc
  )
  target_sources(tailslide_fuzzer PRIVATE
  )
  target_include_directories(tailslide_fuzzer PUBLIC ${CMAKE_CURRENT_BINARY_DIR} libtailslide)
  target_link_libraries(tailslide_fuzzer PUBLIC ${EXTRA_LIBS} libtailslide "-fsanitize=fuzzer,address")
  set_target_properties(tailslide_fuzzer PROPERTIES OUTPUT_NAME tailslide-fuzzer)
endif()
