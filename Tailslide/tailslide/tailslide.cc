#include <stdexcept>
#include <string>

#include "tailslide.hh"
#include "lslmini.tab.hh"

int tailslide_lex_init_extra(Tailslide::ScriptContext *, void **);
void tailslide_set_in(FILE *, void *);
struct yy_buffer_state *tailslide__scan_bytes ( const char *bytes, int len, void *);

int tailslide_lex_destroy(void *);

namespace Tailslide {

extern LSLSymbolTable gBuiltinsSymbolTable;

ScopedScriptParser::ScopedScriptParser(LSLSymbolTable *builtins) : logger(&allocator), table_manager(&allocator) {
  context.allocator = &allocator;
  context.logger = &logger;
  context.table_manager = &table_manager;
  if (builtins)
    context.builtins = builtins;
  else
    context.builtins = &gBuiltinsSymbolTable;
}

ScopedScriptParser::~ScopedScriptParser() {
  // parseInternal() normally tears the scanner down, but it won't run if
  // tailslide_parse() threw, or if initScanner() was called without a
  // following parse.
  destroyScanner();
}

void ScopedScriptParser::destroyScanner() {
  if (context.scanner) {
    tailslide_lex_destroy(context.scanner);
    context.scanner = nullptr;
  }
}

// make sure we don't leak an FH if we throw
class FileCloser {
  public:
    explicit FileCloser(FILE *file): _mFile(file) {};
    ~FileCloser() {fclose(_mFile);};

    FileCloser(const FileCloser &) = delete;
    FileCloser &operator=(const FileCloser &) = delete;

    FILE *_mFile;
};

LSLScript *ScopedScriptParser::parseLSLFile(const std::string &filename) {
  // can only be used to parse a single script.
  assert(!script);
  FILE *yyin = fopen(filename.c_str(), "rb");
  if (yyin == nullptr) {
    throw std::runtime_error("couldn't open file");
  }
  FileCloser closer(yyin);
  return parseLSLFile(yyin);
}

LSLScript *ScopedScriptParser::parseLSLFile(FILE *yyin) {
  initScanner();
  // set input file
  tailslide_set_in(yyin, context.scanner);
  parseInternal();
  return script;
}

LSLScript *ScopedScriptParser::parseLSLBytes(const char *buf, int buf_len) {
  initScanner();
  // set input file
  tailslide__scan_bytes(buf, buf_len, context.scanner);
  parseInternal();
  return script;
}

void ScopedScriptParser::initScanner() {
  assert(!script);
  // ScopedScriptParser owns the allocator and context instance because we can't
  // reasonably re-use Allocator instances with our current model of having
  // it magically pass along the current script context.
  allocator.setContext(&context);

  // a previous parse that threw may have left a scanner behind; don't leak it
  destroyScanner();

  // initialize flex
  tailslide_lex_init_extra(&context, &context.scanner);
}

void ScopedScriptParser::parseInternal() {
  // parse
  context.parsing = true;
  tailslide_parse(context.scanner);
  context.parsing = false;

  // clean up flex
  destroyScanner();
  ast_sane = context.ast_sane;
  script = context.script;
}

}
