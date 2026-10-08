#include <iostream>

#include "tailslide.hh"
#include "passes/tree_simplifier.hh"
#include "passes/lso/script_compiler.hh"
#include "passes/mono/script_compiler.hh"


static bool initialized = false;
static bool compile_lso = false;
static bool compile_cil = false;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!initialized) {
    Tailslide::tailslide_init_builtins(nullptr);
    initialized = true;
    compile_lso = getenv("COMPILE_LSO") != nullptr;
    compile_cil = getenv("COMPILE_CIL") != nullptr;
  }

  Tailslide::ScopedScriptParser parser(nullptr);
  try {
    auto *script = parser.parseLSLBytes((const char *)data, size);
    if (script) {
      script->collectSymbols();
      script->determineTypes();
      script->recalculateReferenceData();
      script->propagateValues();
      script->finalPass();
      if (compile_lso) {
        script->checkSymbols();
        script->validateGlobals(true);
        if (!parser.logger.getErrors()) {
          Tailslide::LSOScriptCompiler visitor(&parser.allocator);
          script->visit(&visitor);
        }
      } else if (compile_cil) {
        script->checkSymbols();
        script->validateGlobals(true);
        if (!parser.logger.getErrors()) {
          Tailslide::MonoScriptCompiler visitor(&parser.allocator);
          script->visit(&visitor);
        }
      } else {
        if (!parser.logger.getErrors()) {
          Tailslide::OptimizationOptions ctx{
              .fold_constants = true,
              .prune_unused_locals = true,
              .prune_unused_globals = true,
              .prune_unused_functions = true,
          };
          script->optimize(ctx);
          script->validateGlobals(true);
          script->checkSymbols();
        }
      }
    }
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
  }

  return EXIT_SUCCESS;
}
