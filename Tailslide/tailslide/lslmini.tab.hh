/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_TAILSLIDE_LIBTAILSLIDE_LSLMINI_TAB_HH_INCLUDED
# define YY_TAILSLIDE_LIBTAILSLIDE_LSLMINI_TAB_HH_INCLUDED
/* Debug traces.  */
#ifndef TAILSLIDE_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define TAILSLIDE_DEBUG 1
#  else
#   define TAILSLIDE_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define TAILSLIDE_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined TAILSLIDE_DEBUG */
#if TAILSLIDE_DEBUG
extern int tailslide_debug;
#endif
/* "%code requires" blocks.  */
#line 60 "libtailslide/lslmini.y"

#include "loctype.hh"

#line 61 "libtailslide/lslmini.tab.hh"

/* Token kinds.  */
#ifndef TAILSLIDE_TOKENTYPE
# define TAILSLIDE_TOKENTYPE
  enum tailslide_tokentype
  {
    TAILSLIDE_EMPTY = -2,
    TAILSLIDE_EOF = 0,             /* "end of file"  */
    TAILSLIDE_error = 256,         /* error  */
    TAILSLIDE_UNDEF = 257,         /* "invalid token"  */
    INTEGER = 258,                 /* INTEGER  */
    FLOAT_TYPE = 259,              /* FLOAT_TYPE  */
    STRING = 260,                  /* STRING  */
    LLKEY = 261,                   /* LLKEY  */
    VECTOR = 262,                  /* VECTOR  */
    QUATERNION = 263,              /* QUATERNION  */
    LIST = 264,                    /* LIST  */
    STATE = 265,                   /* STATE  */
    EVENT = 266,                   /* EVENT  */
    JUMP = 267,                    /* JUMP  */
    RETURN = 268,                  /* RETURN  */
    IDENTIFIER = 269,              /* IDENTIFIER  */
    STATE_DEFAULT = 270,           /* STATE_DEFAULT  */
    INTEGER_CONSTANT = 271,        /* INTEGER_CONSTANT  */
    FP_CONSTANT = 272,             /* FP_CONSTANT  */
    STRING_CONSTANT = 273,         /* STRING_CONSTANT  */
    INC_OP = 274,                  /* INC_OP  */
    DEC_OP = 275,                  /* DEC_OP  */
    INC_PRE_OP = 276,              /* INC_PRE_OP  */
    DEC_PRE_OP = 277,              /* DEC_PRE_OP  */
    INC_POST_OP = 278,             /* INC_POST_OP  */
    DEC_POST_OP = 279,             /* DEC_POST_OP  */
    ADD_ASSIGN = 280,              /* ADD_ASSIGN  */
    SUB_ASSIGN = 281,              /* SUB_ASSIGN  */
    MUL_ASSIGN = 282,              /* MUL_ASSIGN  */
    DIV_ASSIGN = 283,              /* DIV_ASSIGN  */
    MOD_ASSIGN = 284,              /* MOD_ASSIGN  */
    EQ = 285,                      /* EQ  */
    NEQ = 286,                     /* NEQ  */
    GEQ = 287,                     /* GEQ  */
    LEQ = 288,                     /* LEQ  */
    BOOLEAN_AND = 289,             /* BOOLEAN_AND  */
    BOOLEAN_OR = 290,              /* BOOLEAN_OR  */
    SHIFT_LEFT = 291,              /* SHIFT_LEFT  */
    SHIFT_RIGHT = 292,             /* SHIFT_RIGHT  */
    IF = 293,                      /* IF  */
    ELSE = 294,                    /* ELSE  */
    FOR = 295,                     /* FOR  */
    DO = 296,                      /* DO  */
    WHILE = 297,                   /* WHILE  */
    PRINT = 298,                   /* PRINT  */
    PERIOD = 299,                  /* PERIOD  */
    LOWER_THAN_ELSE = 300,         /* LOWER_THAN_ELSE  */
    INITIALIZER = 301              /* INITIALIZER  */
  };
  typedef enum tailslide_tokentype tailslide_token_kind_t;
#endif

/* Value type.  */
#if ! defined TAILSLIDE_STYPE && ! defined TAILSLIDE_STYPE_IS_DECLARED
union TAILSLIDE_STYPE
{
#line 65 "libtailslide/lslmini.y"

    Tailslide::S32                             ival;
    Tailslide::F32                             fval;
    char                                       *sval;
    class Tailslide::LSLType              *type;
    class Tailslide::LSLConstant          *constant;
    class Tailslide::LSLIdentifier        *identifier;
    class Tailslide::LSLGlobalVariable    *global;
    class Tailslide::LSLEventHandler      *handler;
    class Tailslide::LSLExpression        *expression;
    class Tailslide::LSLStatement         *statement;
    class Tailslide::LSLGlobalFunction    *global_funcs;
    class Tailslide::LSLASTNode           *global_store;
    class Tailslide::LSLASTNodeList<Tailslide::LSLASTNode> *node_list;
    class Tailslide::LSLState             *state;
    class Tailslide::LSLScript            *script;

#line 142 "libtailslide/lslmini.tab.hh"

};
typedef union TAILSLIDE_STYPE TAILSLIDE_STYPE;
# define TAILSLIDE_STYPE_IS_TRIVIAL 1
# define TAILSLIDE_STYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined TAILSLIDE_LTYPE && ! defined TAILSLIDE_LTYPE_IS_DECLARED
typedef struct TAILSLIDE_LTYPE TAILSLIDE_LTYPE;
struct TAILSLIDE_LTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define TAILSLIDE_LTYPE_IS_DECLARED 1
# define TAILSLIDE_LTYPE_IS_TRIVIAL 1
#endif




int tailslide_parse (void *scanner);


#endif /* !YY_TAILSLIDE_LIBTAILSLIDE_LSLMINI_TAB_HH_INCLUDED  */
