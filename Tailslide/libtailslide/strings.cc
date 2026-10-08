#include <cstring>
#include <string>

#include "strings.hh"
#include "allocator.hh"

namespace Tailslide {

char *parse_string(ScriptAllocator *allocator, char *input, YYLTYPE *lloc) {
  char *str = allocator->alloc((strlen(input) * 2) + 1);
  char *yp = input + 1;
  char *sp = str;
  int end = 0;
  const char *last_nl = nullptr;
  int num_nls = 0;
#define APPEND_CHAR(_x) do {*sp++ = (_x); ++end; } while(0)
  // The first `"` after an `L"` string opener is part of the string value itself
  // due to lscript's broken parser.
  if (input[0] == 'L' && input[1] == '"') {
    APPEND_CHAR('"');
    ++yp;
  }
  while (*yp) {
    if (*yp == '\\') {
      switch (*++yp) {
        case 'n':
          APPEND_CHAR('\n');
          break;
        case 't':
          // Tab escape really means "four spaces" in LSL.
          for (int i = 0; i < 4; ++i) {
            APPEND_CHAR(' ');
          }
          break;
        case '\\':
          APPEND_CHAR('\\');
          break;
        case '"':
          APPEND_CHAR('"');
          break;
        // should never be possible, but stop parsing if we hit a NUL immediately after a '\'
        case '\0':
          goto escaped_null;
        default:
          // "\f" just outputs "f"
          APPEND_CHAR(*yp);
          break;
      }
      ++yp;
      escaped_null:;
    } else if (*yp == '"') {
      break;
    } else {
      if (*yp == '\n') {
        ++num_nls;
        // yp always > input
        last_nl = yp;
      }
      APPEND_CHAR(*yp++);
    }
  }
#undef APPEND_CHAR
  if (lloc) {
    if (num_nls) {
      lloc->last_line += num_nls;
      // the line starts at the last newline, add one to account for terminating quote.
      lloc->last_column = (int)(yp - last_nl + 1);
    }
  }
  str[end] = '\0';
  return str;
}

std::string escape_string(const char *data) {
  std::string new_str;
  size_t datasize = strlen(data);
  for(size_t i=0; i<datasize; ++i) {
    char c = data[i];
    if (c == '\\')
      new_str += "\\\\";
    else if (c == '"')
      new_str += "\\\"";
    else if (c == '\n')
      new_str += "\\n";
    else
      new_str += c;
  }
  return new_str;
}

}
