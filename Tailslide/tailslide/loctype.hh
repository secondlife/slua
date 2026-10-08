#pragma once

namespace Tailslide {

typedef struct TailslideLType TAILSLIDE_LTYPE;
struct TailslideLType {
  int first_line;
  int first_column;
  int last_line;
  int last_column;

  bool operator>(const TailslideLType &other) const {
    if (first_line > other.first_line)
      return true;
    if (first_line < other.first_line)
      return false;

    if (first_column > other.first_column)
      return true;
    if (first_column < other.first_column)
      return false;
    return false;
  }

  bool operator<(const TailslideLType &other) const {
    if (first_line < other.first_line)
      return true;
    if (first_line > other.first_line)
      return false;

    if (first_column < other.first_column)
      return true;
    if (first_column > other.first_column)
      return false;
    return false;
  }
};

// keep existing consumers inside tailslide working
using YYLTYPE = TailslideLType;

}
# define TAILSLIDE_LTYPE_IS_DECLARED 1
# define TAILSLIDE_LTYPE_IS_TRIVIAL 1
