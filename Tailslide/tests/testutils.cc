#include "testutils.hh"
#include "passes/lso/script_compiler.hh"
#include "passes/mono/script_compiler.hh"

using namespace Tailslide;

std::vector<LogMessage*> getFilteredMessages(const ParserRef &parser) {
  Logger *logger = &parser->logger;
  // Take a copy of the assertions we saw during script parsing
  std::vector<std::pair<int, ErrorCode>> remaining_assertions = parser->context.assertions;
  std::vector<LogMessage*> filtered_messages;

  // Check each message against assertions and collect non-suppressed ones
  for (auto msg : logger->getMessages()) {
    bool suppressed = false;
    // Crucially, we don't keep this iterator around.
    for (auto ai = remaining_assertions.begin(); ai != remaining_assertions.end(); ++ai) {
      if (ai->first == msg->getLoc()->first_line && ai->second == msg->getError()) {
        suppressed = true;
        remaining_assertions.erase(ai);
        break;
      }
    }
    if (!suppressed) {
      filtered_messages.push_back(msg);
    }
  }

  // Add failed assertions as synthetic error messages
  for (auto &failed_assert : remaining_assertions) {
    YYLTYPE loc = {failed_assert.first, 1, failed_assert.first, 1};
    char msg_buf[100];
    snprintf(msg_buf, sizeof(msg_buf), "Assertion failed: expected error %d", failed_assert.second);
    auto *synthetic_msg = logger->createMessage(LOG_ERROR, &loc, msg_buf, E_ERROR);
    filtered_messages.push_back(synthetic_msg);
  }

  return filtered_messages;
}

static void printLogMessages(const std::vector<LogMessage*> &messages) {
  for (auto msg : messages) {
    fprintf(stderr, "%s\n", msg->toString().c_str());
  }
}

ParserRef runConformance(const char *name, bool allow_syntax_errors)
{
  std::string path = __FILE__;
  path.erase(path.find_last_of("\\/"));
  path += "/scripts/";
  path += name;

  ParserRef parser(new ScopedScriptParser(nullptr));
  parser->context.collect_assertions = true;
  parser->parseLSLFile(path);
  LSLScript *script = parser->script;

  if (script == nullptr)
  {
    if (!allow_syntax_errors) {
      std::string message = "script " + path + " completely failed to parse!";
      printLogMessages(getFilteredMessages(parser));
      FAIL(message);
    }
  } else {
    script->collectSymbols();
    script->determineTypes();
    script->recalculateReferenceData();
    script->propagateValues();
    script->finalPass();
    script->validateGlobals(true);
    script->checkSymbols();
  }

  return parser;
}

void assertNoLintErrors(const ParserRef &parser, const std::string& name) {
  auto filtered = getFilteredMessages(parser);

  if (!filtered.empty()) {
    printLogMessages(filtered);
    std::string message = "script " + name + " encountered " + std::to_string(filtered.size()) + " errors during parse!";
    FAIL(message);
  }
}


class ScriptFormatter {
public:
  virtual std::string format(LSLScript *script) const = 0;
  bool mBinary = false;
};

class ScriptPrettyPrinter: public ScriptFormatter {
public:
  explicit ScriptPrettyPrinter(const PrettyPrintOpts &pretty_opts): pretty_opts(pretty_opts) {};
  virtual std::string format(LSLScript *script) const {
    PrettyPrintVisitor pretty_visitor(pretty_opts);
    script->visit(&pretty_visitor);
    return pretty_visitor.mStream.str();
  };
  PrettyPrintOpts pretty_opts;
};

class ScriptTreeDumper: public ScriptFormatter {
public:
  virtual std::string format(LSLScript *script) const {
    TreePrintingVisitor tree_visitor;
    script->visit(&tree_visitor);
    return tree_visitor.mStream.str();
  };
};

class ScriptLSOCompiler: public ScriptFormatter {
  public:
    ScriptLSOCompiler() {mBinary = true;};
    virtual std::string format(LSLScript *script) const {
      LSOScriptCompiler lso_visitor(script->mContext->allocator);
      script->visit(&lso_visitor);
      return {(const char*)lso_visitor.mScriptBS.data(), lso_visitor.mScriptBS.size()};
    };
};

class ScriptCILCompiler: public ScriptFormatter {
  public:
  explicit ScriptCILCompiler(MonoCompilationOptions options) : _mOptions(options) {mBinary = false;};
  virtual std::string format(LSLScript *script) const {
    MonoScriptCompiler cil_visitor(script->mContext->allocator, _mOptions);
    script->visit(&cil_visitor);
    return cil_visitor.mCIL.str();
  };
  MonoCompilationOptions _mOptions;
};

static void checkStringOutput(
    const char *name,
    const char *expected_prefix,
    const OptimizationOptions &ctx,
    void (*massager)(LSLScript *script),
    const ScriptFormatter& formatter
) {
  ParserRef parser = runConformance(name);
  Logger *logger = &parser->logger;

  auto filtered = getFilteredMessages(parser);
  CHECK(filtered.empty());
  if (!filtered.empty()) {
    printLogMessages(filtered);
    // If there are errors at this stage then the AST isn't even guaranteed to be sane.
    return;
  }
  logger->reset();

  if (massager != nullptr)
    massager(parser->script);
  if (ctx)
    parser->script->optimize(ctx);
  parser->script->validateGlobals(true);
  parser->script->checkSymbols();
  parser->table_manager.setMangledNames();

  std::string prettified = formatter.format(parser->script);

  std::string path = __FILE__;
  path.erase(path.find_last_of("\\/"));
  path += "/scripts/expected/";
  if (expected_prefix != nullptr) {
    path += expected_prefix;
    path += "/";
  }
  path += name;

  std::ifstream in;
  in.open(path, std::ifstream::in | std::ifstream::binary);
  std::stringstream sstr;
  sstr << in.rdbuf();
  const std::string expected(sstr.str());
  in.close();
  if (expected != prettified) {
    size_t diff_pos;
    for(diff_pos=0; diff_pos < std::min(expected.length(), prettified.length()); ++diff_pos) {
      if (expected[diff_pos] != prettified[diff_pos])
        break;
    }
    std::string msg;
    if (formatter.mBinary) {
      msg = "difference at pos " + std::to_string(diff_pos);
    } else {
      msg = expected + "\n\nIS NOT EQUAL TO\n\n" + prettified + "\ndifference at pos " + std::to_string(diff_pos);
    }
    FAIL(msg);
  }
}


void checkPrettyPrintOutput(
        const char *name,
        const OptimizationOptions &ctx,
        const PrettyPrintOpts &pretty_opts,
        void (*massager)(LSLScript *script)
) {
  checkStringOutput(
    name,
    nullptr,
    ctx,
    massager,
    ScriptPrettyPrinter(pretty_opts)
  );
}

void checkTreeDumpOutput(
    const char *name,
    const OptimizationOptions &ctx,
    void (*massager)(LSLScript *script)
) {
  checkStringOutput(
      name,
      "tree_dump",
      ctx,
      massager,
      ScriptTreeDumper()
  );
}

void checkLSOOutput(
    const char *name,
    void (*massager)(LSLScript *script)
) {
  OptimizationOptions opt;
  checkStringOutput(
      name,
      "lso",
      opt,
      massager,
      ScriptLSOCompiler()
  );
}

void checkCILOutput(
    const char *name,
    MonoCompilationOptions options,
    void (*massager)(LSLScript *script)
) {
  OptimizationOptions opt;
  checkStringOutput(
      name,
      "cil",
      opt,
      massager,
      ScriptCILCompiler(options)
  );
}