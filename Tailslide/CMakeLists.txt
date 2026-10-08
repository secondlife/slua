# Built only as a subdirectory of the SLua tree; the parent owns the toolchain,
# sanitizer and coverage settings.

option(TAILSLIDE_BUILD_CLI "Build the tailslide CLI" OFF)
option(TAILSLIDE_BUILD_FUZZER "Build the libFuzzer harness" OFF)
option(TAILSLIDE_SANITIZE "Use ASAN" OFF)
option(TAILSLIDE_FUZZER_INSTRUMENTATION "Add instrumentation for libFuzzer" OFF)
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

add_library(libtailslide STATIC
        tailslide/allocator.cc
        tailslide/ast.cc
        tailslide/builtins.cc
        tailslide/logger.cc
        tailslide/lslmini.cc
        tailslide/operations.cc
        tailslide/strings.cc
        tailslide/symtab.cc
        tailslide/types.cc
        tailslide/visitor.cc
        tailslide/passes/globalexpr_validator.cc
        tailslide/passes/final_pass.cc
        tailslide/passes/desugaring.cc
        tailslide/passes/pretty_print.cc
        tailslide/passes/symbol_resolution.cc
        tailslide/passes/type_checking.cc
        tailslide/passes/tree_print.cc
        tailslide/passes/tree_simplifier.cc
        tailslide/passes/constant_expression_simplifier.cc
        tailslide/passes/values.cc
        tailslide/passes/lso/bytecode_compiler.cc
        tailslide/passes/lso/library_funcs.cc
        tailslide/passes/lso/script_compiler.cc
        tailslide/passes/lso/resource_collector.cc
        tailslide/passes/mono/resource_collector.cc
        tailslide/passes/mono/script_compiler.cc
        tailslide/tailslide.cc
        )
target_sources(libtailslide PRIVATE
        tailslide/allocator.hh
        tailslide/ast.hh
        tailslide/bitstream.hh
        tailslide/loctype.hh
        tailslide/logger.hh
        tailslide/lslmini.hh
        tailslide/operations.hh
        tailslide/portable_endian.hh
        tailslide/strings.hh
        tailslide/symtab.hh
        tailslide/types.hh
        tailslide/unordered_cstr_map.hh
        tailslide/visitor.hh
        tailslide/passes/globalexpr_validator.hh
        tailslide/passes/final_pass.hh
        tailslide/passes/desugaring.hh
        tailslide/passes/pretty_print.hh
        tailslide/passes/symbol_resolution.hh
        tailslide/passes/tree_simplifier.hh
        tailslide/passes/tree_print.hh
        tailslide/passes/type_checking.hh
        tailslide/passes/constant_expression_simplifier.hh
        tailslide/passes/values.hh
        tailslide/passes/lso/bytecode_compiler.hh
        tailslide/passes/lso/bytecode_format.hh
        tailslide/passes/lso/library_funcs.hh
        tailslide/passes/lso/script_compiler.hh
        tailslide/passes/lso/resource_collector.hh
        tailslide/passes/mono/resource_collector.hh
        tailslide/passes/mono/script_compiler.hh
        tailslide/tailslide.hh
)

# The generated scanner and parser are checked in so that flex and bison aren't
# hard build dependencies. When they are available, regenerate in-tree so the
# checked-in copies stay current.
set(LSLMINI_SCANNER tailslide/lslmini.flex.cc)
set(LSLMINI_PARSER tailslide/lslmini.tab.cc)
set(LSLMINI_PARSER_HEADER tailslide/lslmini.tab.hh)
if (BISON_FOUND AND FLEX_FOUND AND NOT TAILSLIDE_SKIP_GEN)
  # Run from the source dir with relative paths so the `#line` directives in the
  # generated files don't embed this machine's absolute paths.
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${LSLMINI_PARSER} ${CMAKE_CURRENT_SOURCE_DIR}/${LSLMINI_PARSER_HEADER}
    COMMAND ${BISON_EXECUTABLE} --defines=${LSLMINI_PARSER_HEADER} -o ${LSLMINI_PARSER} tailslide/lslmini.y
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tailslide/lslmini.y
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "[BISON] Building parser with bison ${BISON_VERSION}"
    VERBATIM
  )
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${LSLMINI_SCANNER}
    COMMAND ${FLEX_EXECUTABLE} -o ${LSLMINI_SCANNER} tailslide/lslmini.l
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tailslide/lslmini.l ${CMAKE_CURRENT_SOURCE_DIR}/${LSLMINI_PARSER_HEADER}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "[FLEX] Building scanner with flex ${FLEX_VERSION}"
    VERBATIM
  )
else()
  message(STATUS "Not using flex and/or bison, using the checked-in scanner and parser")
endif()

# Consumers include <tailslide/...>, the library itself uses bare names.
target_include_directories(libtailslide PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(libtailslide PRIVATE tailslide)
target_sources(libtailslide PRIVATE ${LSLMINI_SCANNER} ${LSLMINI_PARSER} ${LSLMINI_PARSER_HEADER})
# The embedded builtins.txt comes from here
target_link_libraries(libtailslide PUBLIC Luau.LSLBuiltins)

target_compile_features(libtailslide PUBLIC cxx_std_17)
set_target_properties(libtailslide PROPERTIES CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

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
    tailslide/operations.cc
    PROPERTIES
    COMPILE_FLAGS "-ffp-contract=off"
  )
endif()

set_target_properties(libtailslide PROPERTIES OUTPUT_NAME tailslide)
if (NOT WIN32 AND NOT TAILSLIDE_FUZZER_INSTRUMENTATION)
  add_definitions("-fPIC")
endif()


if (TAILSLIDE_BUILD_CLI)
  add_executable(tailslide_cli tailslide_cli/main.cc)
  target_sources(tailslide_cli PRIVATE
          ${CMAKE_SOURCE_DIR}/extern/cxxopt.hh
  )
  target_include_directories(tailslide_cli PRIVATE tailslide ${CMAKE_SOURCE_DIR}/extern)
  target_link_libraries(tailslide_cli PUBLIC ${EXTRA_LIBS} libtailslide)
  set_target_properties(tailslide_cli PROPERTIES OUTPUT_NAME tailslide)
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
  target_include_directories(tailslide_fuzzer PRIVATE tailslide)
  target_link_libraries(tailslide_fuzzer PUBLIC ${EXTRA_LIBS} libtailslide "-fsanitize=fuzzer,address")
  set_target_properties(tailslide_fuzzer PROPERTIES OUTPUT_NAME tailslide-fuzzer)
endif()
