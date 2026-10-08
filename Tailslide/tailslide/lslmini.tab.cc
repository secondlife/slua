/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE         TAILSLIDE_STYPE
#define YYLTYPE         TAILSLIDE_LTYPE
/* Substitute the variable and function names.  */
#define yyparse         tailslide_parse
#define yylex           tailslide_lex
#define yyerror         tailslide_error
#define yydebug         tailslide_debug
#define yynerrs         tailslide_nerrs

/* First part of user prologue.  */
#line 1 "libtailslide/lslmini.y"

// Based on a grammar file LL provided to the community before the viewer was open-sourced,
// which was used in LSLint. Presumably public domain.
    #include "lslmini.hh"
    #include "lslmini.tab.hh"
    #include "logger.hh"
    #include <stdio.h>
    #include <string.h>

    using namespace Tailslide;
    //int yylex(YYSTYPE *yylval_param, YYLTYPE *yylloc_param);
    extern int tailslide_lex (TAILSLIDE_STYPE * yylval_param, TailslideLType * yylloc_param , void *yyscanner);
    extern ScriptContext *tailslide_get_extra(void *scanner);
    #define ALLOCATOR tailslide_get_extra(scanner)->allocator

    int yyerror( YYLTYPE*, void *, const char * );
    #define MAKEID(type,id,pos) ALLOCATOR->newTracked<LSLIdentifier>(TYPE(type), (id), &(pos))
    #ifndef LSLINT_STACK_OVERFLOW_AT
    #  define LSLINT_STACK_OVERFLOW_AT 10000
    #endif
    // slightly higher so we can still have assert comments that check for stack depth
    #define YYMAXDEPTH LSLINT_STACK_OVERFLOW_AT + 20
    inline int _yylex( TAILSLIDE_STYPE * yylval, YYLTYPE *yylloc, void *yyscanner, int stack ) {
        if ( stack == LSLINT_STACK_OVERFLOW_AT ) {
            tailslide_get_extra(yyscanner)->logger->error( yylloc, E_PARSER_STACK_DEPTH );
            return 0;
        }
        return tailslide_lex( yylval, yylloc, yyscanner );
    }
    #ifdef yylex
    #  undef yylex
    #endif
    #define yylex(a,b,c) _yylex(a, b, c,  (int)(yyssp - yyss))
        

    // Same as bison's default, but update global position so we don't have
    // to pass it in every time we make a branch
    # define YYLLOC_DEFAULT(Current, Rhs, N)                \
        ((Current).first_line   = (Rhs)[1].first_line,       \
         (Current).first_column = (Rhs)[1].first_column,     \
         (Current).last_line    = (Rhs)[N].last_line,        \
         (Current).last_column  = (Rhs)[N].last_column,      \
         tailslide_get_extra(scanner)->glloc = (Current))


#line 130 "libtailslide/lslmini.tab.cc"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "lslmini.tab.hh"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_INTEGER = 3,                    /* INTEGER  */
  YYSYMBOL_FLOAT_TYPE = 4,                 /* FLOAT_TYPE  */
  YYSYMBOL_STRING = 5,                     /* STRING  */
  YYSYMBOL_LLKEY = 6,                      /* LLKEY  */
  YYSYMBOL_VECTOR = 7,                     /* VECTOR  */
  YYSYMBOL_QUATERNION = 8,                 /* QUATERNION  */
  YYSYMBOL_LIST = 9,                       /* LIST  */
  YYSYMBOL_STATE = 10,                     /* STATE  */
  YYSYMBOL_EVENT = 11,                     /* EVENT  */
  YYSYMBOL_JUMP = 12,                      /* JUMP  */
  YYSYMBOL_RETURN = 13,                    /* RETURN  */
  YYSYMBOL_IDENTIFIER = 14,                /* IDENTIFIER  */
  YYSYMBOL_STATE_DEFAULT = 15,             /* STATE_DEFAULT  */
  YYSYMBOL_INTEGER_CONSTANT = 16,          /* INTEGER_CONSTANT  */
  YYSYMBOL_FP_CONSTANT = 17,               /* FP_CONSTANT  */
  YYSYMBOL_STRING_CONSTANT = 18,           /* STRING_CONSTANT  */
  YYSYMBOL_INC_OP = 19,                    /* INC_OP  */
  YYSYMBOL_DEC_OP = 20,                    /* DEC_OP  */
  YYSYMBOL_INC_PRE_OP = 21,                /* INC_PRE_OP  */
  YYSYMBOL_DEC_PRE_OP = 22,                /* DEC_PRE_OP  */
  YYSYMBOL_INC_POST_OP = 23,               /* INC_POST_OP  */
  YYSYMBOL_DEC_POST_OP = 24,               /* DEC_POST_OP  */
  YYSYMBOL_ADD_ASSIGN = 25,                /* ADD_ASSIGN  */
  YYSYMBOL_SUB_ASSIGN = 26,                /* SUB_ASSIGN  */
  YYSYMBOL_MUL_ASSIGN = 27,                /* MUL_ASSIGN  */
  YYSYMBOL_DIV_ASSIGN = 28,                /* DIV_ASSIGN  */
  YYSYMBOL_MOD_ASSIGN = 29,                /* MOD_ASSIGN  */
  YYSYMBOL_EQ = 30,                        /* EQ  */
  YYSYMBOL_NEQ = 31,                       /* NEQ  */
  YYSYMBOL_GEQ = 32,                       /* GEQ  */
  YYSYMBOL_LEQ = 33,                       /* LEQ  */
  YYSYMBOL_BOOLEAN_AND = 34,               /* BOOLEAN_AND  */
  YYSYMBOL_BOOLEAN_OR = 35,                /* BOOLEAN_OR  */
  YYSYMBOL_SHIFT_LEFT = 36,                /* SHIFT_LEFT  */
  YYSYMBOL_SHIFT_RIGHT = 37,               /* SHIFT_RIGHT  */
  YYSYMBOL_IF = 38,                        /* IF  */
  YYSYMBOL_ELSE = 39,                      /* ELSE  */
  YYSYMBOL_FOR = 40,                       /* FOR  */
  YYSYMBOL_DO = 41,                        /* DO  */
  YYSYMBOL_WHILE = 42,                     /* WHILE  */
  YYSYMBOL_PRINT = 43,                     /* PRINT  */
  YYSYMBOL_PERIOD = 44,                    /* PERIOD  */
  YYSYMBOL_LOWER_THAN_ELSE = 45,           /* LOWER_THAN_ELSE  */
  YYSYMBOL_46_ = 46,                       /* ';'  */
  YYSYMBOL_47_ = 47,                       /* '@'  */
  YYSYMBOL_48_ = 48,                       /* '-'  */
  YYSYMBOL_49_ = 49,                       /* '!'  */
  YYSYMBOL_50_ = 50,                       /* '~'  */
  YYSYMBOL_51_ = 51,                       /* '='  */
  YYSYMBOL_52_ = 52,                       /* '<'  */
  YYSYMBOL_53_ = 53,                       /* '>'  */
  YYSYMBOL_54_ = 54,                       /* '+'  */
  YYSYMBOL_55_ = 55,                       /* '*'  */
  YYSYMBOL_56_ = 56,                       /* '/'  */
  YYSYMBOL_57_ = 57,                       /* '%'  */
  YYSYMBOL_58_ = 58,                       /* '&'  */
  YYSYMBOL_59_ = 59,                       /* '|'  */
  YYSYMBOL_60_ = 60,                       /* '^'  */
  YYSYMBOL_61_ = 61,                       /* '('  */
  YYSYMBOL_62_ = 62,                       /* ')'  */
  YYSYMBOL_INITIALIZER = 63,               /* INITIALIZER  */
  YYSYMBOL_64_ = 64,                       /* ','  */
  YYSYMBOL_65_ = 65,                       /* '{'  */
  YYSYMBOL_66_ = 66,                       /* '}'  */
  YYSYMBOL_67_ = 67,                       /* '['  */
  YYSYMBOL_68_ = 68,                       /* ']'  */
  YYSYMBOL_YYACCEPT = 69,                  /* $accept  */
  YYSYMBOL_lscript_program = 70,           /* lscript_program  */
  YYSYMBOL_globals = 71,                   /* globals  */
  YYSYMBOL_global = 72,                    /* global  */
  YYSYMBOL_name_type = 73,                 /* name_type  */
  YYSYMBOL_global_variable = 74,           /* global_variable  */
  YYSYMBOL_constant = 75,                  /* constant  */
  YYSYMBOL_typename = 76,                  /* typename  */
  YYSYMBOL_global_function = 77,           /* global_function  */
  YYSYMBOL_function_parameters = 78,       /* function_parameters  */
  YYSYMBOL_function_parameter = 79,        /* function_parameter  */
  YYSYMBOL_event_parameters = 80,          /* event_parameters  */
  YYSYMBOL_event_parameter = 81,           /* event_parameter  */
  YYSYMBOL_states = 82,                    /* states  */
  YYSYMBOL_other_states = 83,              /* other_states  */
  YYSYMBOL_default = 84,                   /* default  */
  YYSYMBOL_state = 85,                     /* state  */
  YYSYMBOL_state_body = 86,                /* state_body  */
  YYSYMBOL_event = 87,                     /* event  */
  YYSYMBOL_compound_statement = 88,        /* compound_statement  */
  YYSYMBOL_statements = 89,                /* statements  */
  YYSYMBOL_statement = 90,                 /* statement  */
  YYSYMBOL_declaration = 91,               /* declaration  */
  YYSYMBOL_forexpressionlist = 92,         /* forexpressionlist  */
  YYSYMBOL_nextforexpressionlist = 93,     /* nextforexpressionlist  */
  YYSYMBOL_funcexpressionlist = 94,        /* funcexpressionlist  */
  YYSYMBOL_nextfuncexpressionlist = 95,    /* nextfuncexpressionlist  */
  YYSYMBOL_listexpressionlist = 96,        /* listexpressionlist  */
  YYSYMBOL_nextlistexpressionlist = 97,    /* nextlistexpressionlist  */
  YYSYMBOL_expression = 98,                /* expression  */
  YYSYMBOL_unaryexpression = 99,           /* unaryexpression  */
  YYSYMBOL_typecast = 100,                 /* typecast  */
  YYSYMBOL_unarypostfixexpression = 101,   /* unarypostfixexpression  */
  YYSYMBOL_vector_initializer = 102,       /* vector_initializer  */
  YYSYMBOL_quaternion_initializer = 103,   /* quaternion_initializer  */
  YYSYMBOL_list_initializer = 104,         /* list_initializer  */
  YYSYMBOL_lvalue = 105                    /* lvalue  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined TAILSLIDE_LTYPE_IS_TRIVIAL && TAILSLIDE_LTYPE_IS_TRIVIAL \
             && defined TAILSLIDE_STYPE_IS_TRIVIAL && TAILSLIDE_STYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  21
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   1257

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  69
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  37
/* YYNRULES -- Number of rules.  */
#define YYNRULES  131
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  258

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   301


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    49,     2,     2,     2,    57,    58,     2,
      61,    62,    55,    54,    64,    48,     2,    56,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    46,
      52,    51,    53,     2,    47,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    67,     2,    68,    60,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    65,    59,    66,    50,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    63
};

#if TAILSLIDE_DEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   229,   229,   236,   246,   251,   264,   268,   275,   282,
     286,   290,   297,   302,   306,   311,   315,   322,   326,   330,
     334,   338,   342,   346,   353,   361,   369,   377,   388,   392,
     404,   411,   415,   427,   434,   438,   451,   456,   469,   476,
     487,   494,   505,   509,   521,   529,   540,   544,   556,   561,
     575,   579,   583,   587,   591,   595,   599,   603,   607,   611,
     615,   620,   626,   636,   641,   646,   653,   657,   667,   670,
     677,   681,   694,   698,   705,   709,   722,   727,   734,   738,
     750,   754,   758,   762,   766,   770,   774,   778,   782,   786,
     790,   794,   798,   802,   806,   810,   814,   818,   822,   826,
     830,   834,   838,   842,   846,   853,   857,   861,   865,   869,
     873,   877,   881,   888,   892,   906,   919,   923,   930,   935,
     939,   943,   947,   951,   955,   962,   968,   975,   982,   989,
     996,  1003
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "INTEGER",
  "FLOAT_TYPE", "STRING", "LLKEY", "VECTOR", "QUATERNION", "LIST", "STATE",
  "EVENT", "JUMP", "RETURN", "IDENTIFIER", "STATE_DEFAULT",
  "INTEGER_CONSTANT", "FP_CONSTANT", "STRING_CONSTANT", "INC_OP", "DEC_OP",
  "INC_PRE_OP", "DEC_PRE_OP", "INC_POST_OP", "DEC_POST_OP", "ADD_ASSIGN",
  "SUB_ASSIGN", "MUL_ASSIGN", "DIV_ASSIGN", "MOD_ASSIGN", "EQ", "NEQ",
  "GEQ", "LEQ", "BOOLEAN_AND", "BOOLEAN_OR", "SHIFT_LEFT", "SHIFT_RIGHT",
  "IF", "ELSE", "FOR", "DO", "WHILE", "PRINT", "PERIOD", "LOWER_THAN_ELSE",
  "';'", "'@'", "'-'", "'!'", "'~'", "'='", "'<'", "'>'", "'+'", "'*'",
  "'/'", "'%'", "'&'", "'|'", "'^'", "'('", "')'", "INITIALIZER", "','",
  "'{'", "'}'", "'['", "']'", "$accept", "lscript_program", "globals",
  "global", "name_type", "global_variable", "constant", "typename",
  "global_function", "function_parameters", "function_parameter",
  "event_parameters", "event_parameter", "states", "other_states",
  "default", "state", "state_body", "event", "compound_statement",
  "statements", "statement", "declaration", "forexpressionlist",
  "nextforexpressionlist", "funcexpressionlist", "nextfuncexpressionlist",
  "listexpressionlist", "nextlistexpressionlist", "expression",
  "unaryexpression", "typecast", "unarypostfixexpression",
  "vector_initializer", "quaternion_initializer", "list_initializer",
  "lvalue", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-127)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     252,  -127,  -127,  -127,  -127,  -127,  -127,  -127,   -54,   -19,
      20,    26,   284,    52,  -127,    36,  -127,  -127,    47,    25,
     -13,  -127,  -127,  -127,  -127,   100,   128,  -127,    45,  -127,
      47,    10,    85,    40,    48,    43,  -127,    55,   109,    78,
     -23,  -127,  -127,  -127,   112,   112,    66,   449,   495,   495,
     495,   433,   495,  -127,   949,  -127,  -127,  -127,  -127,  -127,
    -127,   166,    10,    77,    63,  -127,   234,  -127,  -127,    10,
     319,   150,  -127,  -127,  -127,   126,   495,    97,  -127,  -127,
     495,  -127,  -127,   -45,  -127,  -127,   541,    80,   751,    83,
    -127,   576,   495,   495,   495,   495,   495,   495,   495,   495,
    -127,   495,   495,   495,   495,   495,   495,   495,   495,   495,
     495,  -127,  -127,   495,   495,   495,   495,   495,   495,  -127,
      10,    -8,   114,    81,   148,   441,   104,   107,   368,   108,
    -127,   156,  -127,   157,  -127,   301,  -127,   127,   980,  -127,
    -127,    10,   158,   113,   115,  -127,   118,  -127,   611,   784,
     495,   503,  -127,  -127,   495,  1200,  1200,     8,     8,  1117,
    1117,    90,    90,   -45,     8,     8,   -45,  -127,  -127,  -127,
    1190,  1148,  1161,  1073,  1073,  1073,  1073,  1073,  1073,  -127,
    -127,   110,  -127,   131,   135,   137,  -127,  1011,   495,   495,
     142,   495,   141,   138,  -127,  -127,  -127,  -127,  -127,  -127,
      10,   319,  -127,   495,  -127,   646,    23,   495,  -127,  -127,
      89,  -127,  -127,  -127,  -127,  -127,  -127,   817,   151,  -127,
     681,   139,   850,  -127,   495,  -127,  -127,  -127,   495,  -127,
    -127,  -127,   883,   368,   495,   495,   495,   368,  1073,   716,
    -127,   149,  1042,  -127,   916,  -127,    -1,   495,   368,   495,
     152,  1104,  -127,   134,  -127,    -1,   368,  -127
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,    17,    18,    19,    20,    21,    22,    23,     0,     0,
       0,     0,     4,     0,     6,     0,     7,     3,    34,     0,
       0,     1,     2,     5,     9,     0,     0,     8,     0,    35,
      36,     0,     0,     0,    28,     0,    39,     0,    42,     0,
     130,    13,    15,    16,     0,     0,     0,     0,     0,     0,
       0,     0,    76,   126,     0,    80,   110,   111,   118,   119,
     120,   121,     0,     0,     0,    37,     0,    24,    30,     0,
       0,     0,    38,    43,    11,     0,    72,   130,   108,   109,
       0,    12,    14,   105,   106,   107,     0,     0,     0,     0,
      77,    78,     0,     0,     0,     0,     0,     0,     0,     0,
      10,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   122,   123,     0,     0,     0,     0,     0,     0,    25,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      50,     0,    46,     0,    59,     0,    48,     0,     0,    26,
      29,     0,     0,     0,    31,   131,     0,    73,    74,     0,
       0,     0,   112,   129,     0,    87,    88,    90,    89,   101,
     102,   103,   104,    94,    91,    92,    93,    95,    96,    97,
      98,    99,   100,    82,    83,    84,    85,    86,    81,    27,
      41,     0,    65,     0,     0,     0,    56,     0,     0,    68,
       0,     0,     0,    66,    47,    49,    58,    57,    44,    33,
       0,     0,   124,     0,   125,     0,     0,     0,   115,   116,
     113,    79,    40,    51,    52,    53,    55,     0,     0,    69,
      70,     0,     0,    54,     0,    45,    32,    75,     0,   114,
      12,    14,     0,     0,     0,     0,     0,     0,    67,     0,
     117,    60,     0,    71,     0,    64,   127,     0,     0,    68,
       0,     0,    61,     0,    63,   128,     0,    62
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -127,  -127,   189,  -127,  -127,  -127,    51,    35,  -127,   -18,
    -127,     3,  -127,   194,   176,  -127,  -127,   -24,  -127,   -26,
    -127,  -126,  -127,   -42,   -27,  -127,    11,  -127,    59,   -25,
    -127,  -127,    64,  -127,  -127,  -127,   -41
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,    10,    11,    12,    13,    14,    53,   133,    16,    33,
      34,   143,   144,    17,    29,    18,    30,    37,    38,   134,
     135,   136,   137,   218,   219,   146,   147,    89,    90,   138,
      55,    56,    57,    58,    59,    60,    61
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      54,    35,   190,    78,    79,    67,    35,    19,    63,   195,
     105,   106,   107,    40,    73,    41,    42,    43,    44,    45,
      21,    75,    83,    84,    85,    86,    88,    91,     1,     2,
       3,     4,     5,     6,     7,    15,   119,   229,    76,   230,
     231,     9,    46,   139,    98,    99,    20,    15,    48,    49,
      27,   148,   140,    36,    32,   149,   101,    28,   180,    64,
      51,    32,   104,   105,   106,   107,    52,   155,   156,   157,
     158,   159,   160,   161,   162,    66,   163,   164,   165,   166,
     167,   168,   169,   170,   171,   172,    87,    31,   173,   174,
     175,   176,   177,   178,   179,   183,   184,   181,    24,    68,
     187,    39,    69,    25,    71,    32,   142,   241,   111,   112,
     210,   245,    70,    26,    40,   198,    41,    42,    43,    44,
      45,    72,   252,    35,    74,   205,    77,    80,   121,    91,
     257,     1,     2,     3,     4,     5,     6,     7,   101,   120,
     145,    75,   151,    46,   104,   105,   106,   107,    47,    48,
      49,   153,    50,     1,     2,     3,     4,     5,     6,     7,
     182,    51,   185,   217,   220,   188,   222,    52,   189,   191,
     192,   193,   199,   196,   225,   200,   212,   213,   148,   201,
     202,   214,   232,   215,   221,   111,   112,   223,   248,   224,
      62,   113,   114,   115,   116,   117,   256,   234,   254,   238,
     236,    23,   208,   239,   226,    22,    65,   253,   243,   242,
     220,   244,   141,   211,   227,   209,     0,   118,     0,     0,
       0,   165,   251,     0,   220,     0,     0,     0,     0,     0,
     165,     0,     0,     0,     0,   122,   142,     1,     2,     3,
       4,     5,     6,     7,   123,     0,   124,   125,    40,     0,
      41,    42,    43,    44,    45,     1,     2,     3,     4,     5,
       6,     7,     0,     0,     0,     0,     8,     9,     0,     0,
       0,     0,   126,     0,   127,   128,   129,    46,     0,     0,
     130,   131,    47,    48,    49,     0,    50,     1,     2,     3,
       4,     5,     6,     7,     0,    51,     0,     0,     8,    66,
     132,    52,   122,     0,     1,     2,     3,     4,     5,     6,
       7,   123,     0,   124,   125,    40,     0,    41,    42,    43,
      44,    45,     1,     2,     3,     4,     5,     6,     7,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   126,
       0,   127,   128,   129,    46,     0,     0,   130,   131,    47,
      48,    49,     0,    50,     0,     0,     0,     0,     0,     0,
       0,     0,    51,     0,     0,     0,    66,   194,    52,   122,
       0,     1,     2,     3,     4,     5,     6,     7,   123,     0,
     124,   125,    40,     0,    41,    42,    43,    44,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   126,     0,   127,   128,
     129,    46,     0,     0,   130,   131,    47,    48,    49,     0,
      50,     0,     0,     0,     0,     0,     0,     0,     0,    51,
       0,     0,     0,    66,     0,    52,     1,     2,     3,     4,
       5,     6,     7,     0,     0,     0,     0,    40,     0,    41,
      42,    43,    44,    45,     0,    40,     0,    41,    42,    43,
      44,    45,     0,    40,     0,    81,    82,    43,    44,    45,
       0,     0,     0,     0,     0,     0,    46,     0,     0,     0,
       0,    47,    48,    49,    46,    50,     0,   186,     0,    47,
      48,    49,    46,    50,    51,     0,     0,    47,    48,    49,
      52,    50,    51,     0,     0,     0,     0,     0,    52,    40,
      51,    41,    42,    43,    44,    45,    52,    40,     0,    41,
      42,    43,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    46,     0,
       0,     0,     0,    47,    48,    49,    46,    50,     0,     0,
       0,   206,     0,     0,     0,    50,    51,     0,     0,     0,
       0,     0,    52,     0,   207,     0,     0,     0,     0,     0,
      52,    92,    93,    94,    95,    96,    97,    98,    99,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   101,
       0,     0,     0,   102,   103,   104,   105,   106,   107,   108,
     109,   110,     0,     0,     0,   150,    92,    93,    94,    95,
      96,    97,    98,    99,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   101,     0,     0,     0,   102,   103,
     104,   105,   106,   107,   108,   109,   110,     0,     0,     0,
     154,    92,    93,    94,    95,    96,    97,    98,    99,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   101,
       0,     0,     0,   102,   103,   104,   105,   106,   107,   108,
     109,   110,     0,     0,     0,   203,    92,    93,    94,    95,
      96,    97,    98,    99,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   101,     0,     0,     0,   102,   103,
     104,   105,   106,   107,   108,   109,   110,     0,     0,     0,
     228,    92,    93,    94,    95,    96,    97,    98,    99,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   101,
       0,     0,     0,   102,   103,   104,   105,   106,   107,   108,
     109,   110,     0,     0,     0,   235,    92,    93,    94,    95,
      96,    97,    98,    99,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   101,     0,     0,     0,   102,   246,
     104,   105,   106,   107,   108,   109,   110,     0,     0,     0,
     247,    92,    93,    94,    95,    96,    97,    98,    99,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   101,
       0,     0,     0,   102,   103,   104,   105,   106,   107,   108,
     109,   110,     0,   152,    92,    93,    94,    95,    96,    97,
      98,    99,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   101,     0,     0,     0,   102,   103,   104,   105,
     106,   107,   108,   109,   110,     0,   204,    92,    93,    94,
      95,    96,    97,    98,    99,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   101,     0,     0,     0,   102,
     103,   104,   105,   106,   107,   108,   109,   110,     0,   233,
      92,    93,    94,    95,    96,    97,    98,    99,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   101,     0,
       0,     0,   102,   103,   104,   105,   106,   107,   108,   109,
     110,     0,   237,    92,    93,    94,    95,    96,    97,    98,
      99,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   101,     0,     0,     0,   102,   103,   104,   105,   106,
     107,   108,   109,   110,     0,   240,    92,    93,    94,    95,
      96,    97,    98,    99,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   101,     0,     0,     0,   102,   103,
     104,   105,   106,   107,   108,   109,   110,     0,   250,    92,
      93,    94,    95,    96,    97,    98,    99,     0,     0,     0,
       0,     0,     0,     0,     0,   100,     0,   101,     0,     0,
       0,   102,   103,   104,   105,   106,   107,   108,   109,   110,
      92,    93,    94,    95,    96,    97,    98,    99,     0,     0,
       0,     0,     0,     0,     0,     0,   197,     0,   101,     0,
       0,     0,   102,   103,   104,   105,   106,   107,   108,   109,
     110,    92,    93,    94,    95,    96,    97,    98,    99,     0,
       0,     0,     0,     0,     0,     0,     0,   216,     0,   101,
       0,     0,     0,   102,   103,   104,   105,   106,   107,   108,
     109,   110,    92,    93,    94,    95,    96,    97,    98,    99,
       0,     0,     0,     0,     0,     0,     0,     0,   249,     0,
     101,     0,     0,     0,   102,   103,   104,   105,   106,   107,
     108,   109,   110,    92,    93,    94,    95,    96,    97,    98,
      99,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   101,     0,     0,     0,   102,   103,   104,   105,   106,
     107,   108,   109,   110,    92,    93,    94,    95,    96,    97,
      98,    99,     0,     0,     0,     0,     0,    92,    93,    94,
      95,     0,   101,    98,    99,     0,   102,   255,   104,   105,
     106,   107,   108,   109,   110,   101,     0,     0,     0,   102,
     103,   104,   105,   106,   107,   108,   109,   110,    92,    93,
      94,    95,     0,     0,    98,    99,     0,     0,     0,     0,
       0,    92,    93,    94,    95,     0,   101,    98,    99,     0,
     102,   103,   104,   105,   106,   107,   108,     0,   110,   101,
       0,     0,     0,   102,   103,   104,   105,   106,   107,   108,
      92,    93,    94,    95,     0,     0,    98,    99,     0,     0,
       0,     0,    94,    95,     0,     0,    98,    99,   101,     0,
       0,     0,   102,   103,   104,   105,   106,   107,   101,     0,
       0,     0,   102,   103,   104,   105,   106,   107
};

static const yytype_int16 yycheck[] =
{
      25,    14,   128,    44,    45,    31,    14,    61,    26,   135,
      55,    56,    57,    14,    38,    16,    17,    18,    19,    20,
       0,    44,    47,    48,    49,    50,    51,    52,     3,     4,
       5,     6,     7,     8,     9,     0,    62,    14,    61,    16,
      17,    15,    43,    69,    36,    37,    65,    12,    49,    50,
      14,    76,    70,    66,    19,    80,    48,    10,    66,    14,
      61,    26,    54,    55,    56,    57,    67,    92,    93,    94,
      95,    96,    97,    98,    99,    65,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,    51,    62,   113,   114,
     115,   116,   117,   118,   120,    14,    15,   121,    46,    14,
     125,     1,    62,    51,    61,    70,    71,   233,    19,    20,
     151,   237,    64,    61,    14,   141,    16,    17,    18,    19,
      20,    66,   248,    14,    46,   150,    14,    61,    65,   154,
     256,     3,     4,     5,     6,     7,     8,     9,    48,    62,
      14,    44,    62,    43,    54,    55,    56,    57,    48,    49,
      50,    68,    52,     3,     4,     5,     6,     7,     8,     9,
      46,    61,    14,   188,   189,    61,   191,    67,    61,    61,
      14,    14,    14,    46,   200,    62,    66,    46,   203,    64,
      62,    46,   207,    46,    42,    19,    20,    46,    39,    51,
      62,    25,    26,    27,    28,    29,    62,    46,    46,   224,
      61,    12,   151,   228,   201,    11,    30,   249,   235,   234,
     235,   236,    62,   154,   203,   151,    -1,    51,    -1,    -1,
      -1,   246,   247,    -1,   249,    -1,    -1,    -1,    -1,    -1,
     255,    -1,    -1,    -1,    -1,     1,   201,     3,     4,     5,
       6,     7,     8,     9,    10,    -1,    12,    13,    14,    -1,
      16,    17,    18,    19,    20,     3,     4,     5,     6,     7,
       8,     9,    -1,    -1,    -1,    -1,    14,    15,    -1,    -1,
      -1,    -1,    38,    -1,    40,    41,    42,    43,    -1,    -1,
      46,    47,    48,    49,    50,    -1,    52,     3,     4,     5,
       6,     7,     8,     9,    -1,    61,    -1,    -1,    14,    65,
      66,    67,     1,    -1,     3,     4,     5,     6,     7,     8,
       9,    10,    -1,    12,    13,    14,    -1,    16,    17,    18,
      19,    20,     3,     4,     5,     6,     7,     8,     9,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    38,
      -1,    40,    41,    42,    43,    -1,    -1,    46,    47,    48,
      49,    50,    -1,    52,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    61,    -1,    -1,    -1,    65,    66,    67,     1,
      -1,     3,     4,     5,     6,     7,     8,     9,    10,    -1,
      12,    13,    14,    -1,    16,    17,    18,    19,    20,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    38,    -1,    40,    41,
      42,    43,    -1,    -1,    46,    47,    48,    49,    50,    -1,
      52,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    61,
      -1,    -1,    -1,    65,    -1,    67,     3,     4,     5,     6,
       7,     8,     9,    -1,    -1,    -1,    -1,    14,    -1,    16,
      17,    18,    19,    20,    -1,    14,    -1,    16,    17,    18,
      19,    20,    -1,    14,    -1,    16,    17,    18,    19,    20,
      -1,    -1,    -1,    -1,    -1,    -1,    43,    -1,    -1,    -1,
      -1,    48,    49,    50,    43,    52,    -1,    46,    -1,    48,
      49,    50,    43,    52,    61,    -1,    -1,    48,    49,    50,
      67,    52,    61,    -1,    -1,    -1,    -1,    -1,    67,    14,
      61,    16,    17,    18,    19,    20,    67,    14,    -1,    16,
      17,    18,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    43,    -1,
      -1,    -1,    -1,    48,    49,    50,    43,    52,    -1,    -1,
      -1,    48,    -1,    -1,    -1,    52,    61,    -1,    -1,    -1,
      -1,    -1,    67,    -1,    61,    -1,    -1,    -1,    -1,    -1,
      67,    30,    31,    32,    33,    34,    35,    36,    37,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      -1,    -1,    -1,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    -1,    -1,    -1,    64,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    -1,    -1,    -1,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    -1,    -1,    -1,
      64,    30,    31,    32,    33,    34,    35,    36,    37,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      -1,    -1,    -1,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    -1,    -1,    -1,    64,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    -1,    -1,    -1,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    -1,    -1,    -1,
      64,    30,    31,    32,    33,    34,    35,    36,    37,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      -1,    -1,    -1,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    -1,    -1,    -1,    64,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    -1,    -1,    -1,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    -1,    -1,    -1,
      64,    30,    31,    32,    33,    34,    35,    36,    37,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,
      -1,    -1,    -1,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    -1,    62,    30,    31,    32,    33,    34,    35,
      36,    37,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    48,    -1,    -1,    -1,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    -1,    62,    30,    31,    32,
      33,    34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    48,    -1,    -1,    -1,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    -1,    62,
      30,    31,    32,    33,    34,    35,    36,    37,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    48,    -1,
      -1,    -1,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    -1,    62,    30,    31,    32,    33,    34,    35,    36,
      37,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    -1,    -1,    -1,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    -1,    62,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    48,    -1,    -1,    -1,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    -1,    62,    30,
      31,    32,    33,    34,    35,    36,    37,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    46,    -1,    48,    -1,    -1,
      -1,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      30,    31,    32,    33,    34,    35,    36,    37,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    48,    -1,
      -1,    -1,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    30,    31,    32,    33,    34,    35,    36,    37,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    48,
      -1,    -1,    -1,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    30,    31,    32,    33,    34,    35,    36,    37,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,
      48,    -1,    -1,    -1,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    30,    31,    32,    33,    34,    35,    36,
      37,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    48,    -1,    -1,    -1,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    30,    31,    32,    33,    34,    35,
      36,    37,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    -1,    48,    36,    37,    -1,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    48,    -1,    -1,    -1,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    30,    31,
      32,    33,    -1,    -1,    36,    37,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    -1,    48,    36,    37,    -1,
      52,    53,    54,    55,    56,    57,    58,    -1,    60,    48,
      -1,    -1,    -1,    52,    53,    54,    55,    56,    57,    58,
      30,    31,    32,    33,    -1,    -1,    36,    37,    -1,    -1,
      -1,    -1,    32,    33,    -1,    -1,    36,    37,    48,    -1,
      -1,    -1,    52,    53,    54,    55,    56,    57,    48,    -1,
      -1,    -1,    52,    53,    54,    55,    56,    57
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,     4,     5,     6,     7,     8,     9,    14,    15,
      70,    71,    72,    73,    74,    76,    77,    82,    84,    61,
      65,     0,    82,    71,    46,    51,    61,    14,    10,    83,
      85,    62,    76,    78,    79,    14,    66,    86,    87,     1,
      14,    16,    17,    18,    19,    20,    43,    48,    49,    50,
      52,    61,    67,    75,    98,    99,   100,   101,   102,   103,
     104,   105,    62,    78,    14,    83,    65,    88,    14,    62,
      64,    61,    66,    86,    46,    44,    61,    14,   105,   105,
      61,    16,    17,    98,    98,    98,    98,    76,    98,    96,
      97,    98,    30,    31,    32,    33,    34,    35,    36,    37,
      46,    48,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    19,    20,    25,    26,    27,    28,    29,    51,    88,
      62,    65,     1,    10,    12,    13,    38,    40,    41,    42,
      46,    47,    66,    76,    88,    89,    90,    91,    98,    88,
      78,    62,    76,    80,    81,    14,    94,    95,    98,    98,
      64,    62,    62,    68,    64,    98,    98,    98,    98,    98,
      98,    98,    98,    98,    98,    98,    98,    98,    98,    98,
      98,    98,    98,    98,    98,    98,    98,    98,    98,    88,
      66,    86,    46,    14,    15,    14,    46,    98,    61,    61,
      90,    61,    14,    14,    66,    90,    46,    46,    88,    14,
      62,    64,    62,    64,    62,    98,    48,    61,    75,   101,
     105,    97,    66,    46,    46,    46,    46,    98,    92,    93,
      98,    42,    98,    46,    51,    88,    80,    95,    64,    14,
      16,    17,    98,    62,    46,    64,    61,    62,    98,    98,
      62,    90,    98,    93,    98,    90,    53,    64,    39,    46,
      62,    98,    90,    92,    46,    53,    62,    90
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    69,    70,    70,    71,    71,    72,    72,    73,    74,
      74,    74,    75,    75,    75,    75,    75,    76,    76,    76,
      76,    76,    76,    76,    77,    77,    77,    77,    78,    78,
      79,    80,    80,    81,    82,    82,    83,    83,    84,    84,
      85,    85,    86,    86,    87,    87,    88,    88,    89,    89,
      90,    90,    90,    90,    90,    90,    90,    90,    90,    90,
      90,    90,    90,    90,    90,    90,    91,    91,    92,    92,
      93,    93,    94,    94,    95,    95,    96,    96,    97,    97,
      98,    98,    98,    98,    98,    98,    98,    98,    98,    98,
      98,    98,    98,    98,    98,    98,    98,    98,    98,    98,
      98,    98,    98,    98,    98,    99,    99,    99,    99,    99,
      99,    99,    99,   100,   100,   100,   100,   100,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   102,   103,   104,
     105,   105
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     1,     1,     2,     1,     1,     2,     2,
       4,     4,     2,     1,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     4,     4,     5,     5,     1,     3,
       2,     1,     3,     2,     1,     2,     1,     2,     4,     3,
       5,     4,     1,     2,     4,     5,     2,     3,     1,     2,
       1,     3,     3,     3,     3,     3,     2,     2,     2,     1,
       5,     7,     9,     7,     5,     2,     2,     4,     0,     1,
       1,     3,     0,     1,     1,     3,     0,     1,     1,     3,
       1,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     2,     2,     2,     2,     2,
       1,     1,     3,     4,     5,     4,     4,     6,     1,     1,
       1,     1,     2,     2,     4,     4,     1,     7,     9,     3,
       1,     3
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = TAILSLIDE_EMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == TAILSLIDE_EMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (&yylloc, scanner, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use TAILSLIDE_error or TAILSLIDE_UNDEF. */
#define YYERRCODE TAILSLIDE_UNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if TAILSLIDE_DEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined TAILSLIDE_LTYPE_IS_TRIVIAL && TAILSLIDE_LTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location, scanner); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, void *scanner)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  YY_USE (scanner);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, void *scanner)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, scanner);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule, void *scanner)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]), scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, scanner); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !TAILSLIDE_DEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !TAILSLIDE_DEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
  YYLTYPE *yylloc;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;
      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, void *scanner)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  YY_USE (scanner);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (void *scanner)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined TAILSLIDE_LTYPE_IS_TRIVIAL && TAILSLIDE_LTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = TAILSLIDE_EMPTY; /* Cause a token to be read.  */

  yylsp[0] = yylloc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == TAILSLIDE_EMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, &yylloc, scanner);
    }

  if (yychar <= TAILSLIDE_EOF)
    {
      yychar = TAILSLIDE_EOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == TAILSLIDE_error)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = TAILSLIDE_UNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = TAILSLIDE_EMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* lscript_program: globals states  */
#line 230 "libtailslide/lslmini.y"
    {
        tailslide_get_extra(scanner)->script = ALLOCATOR->newTracked<LSLScript>(
            ALLOCATOR->newTracked<LSLASTNodeList<LSLASTNode>>((yyvsp[-1].global_store)),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLState>>((yyvsp[0].state))
        );
    }
#line 2004 "libtailslide/lslmini.tab.cc"
    break;

  case 3: /* lscript_program: states  */
#line 237 "libtailslide/lslmini.y"
    {
        tailslide_get_extra(scanner)->script = ALLOCATOR->newTracked<LSLScript>(
            ALLOCATOR->newTracked<LSLASTNodeList<LSLASTNode>>(),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLState>>((yyvsp[0].state))
        );
    }
#line 2015 "libtailslide/lslmini.tab.cc"
    break;

  case 4: /* globals: global  */
#line 247 "libtailslide/lslmini.y"
    {
        DEBUG( LOG_DEBUG_SPAM, nullptr, "** global\n");
        (yyval.global_store) = (yyvsp[0].global_store);
    }
#line 2024 "libtailslide/lslmini.tab.cc"
    break;

  case 5: /* globals: global globals  */
#line 252 "libtailslide/lslmini.y"
    {
        if ( (yyvsp[-1].global_store) ) {
            DEBUG( LOG_DEBUG_SPAM, nullptr, "** global [%p,%p] globals [%p,%p]\n", (yyvsp[-1].global_store)->getPrev(), (yyvsp[-1].global_store)->getNext(), (yyvsp[0].global_store)->getPrev(), (yyvsp[0].global_store)->getNext());
            (yyvsp[-1].global_store)->setNext((yyvsp[0].global_store));
            (yyval.global_store) = (yyvsp[-1].global_store);
        } else {
            (yyval.global_store) = (yyvsp[0].global_store);
        }
    }
#line 2038 "libtailslide/lslmini.tab.cc"
    break;

  case 6: /* global: global_variable  */
#line 265 "libtailslide/lslmini.y"
    {
        (yyval.global_store) = (yyvsp[0].global);
    }
#line 2046 "libtailslide/lslmini.tab.cc"
    break;

  case 7: /* global: global_function  */
#line 269 "libtailslide/lslmini.y"
    {
        (yyval.global_store) = (yyvsp[0].global_funcs);
    }
#line 2054 "libtailslide/lslmini.tab.cc"
    break;

  case 8: /* name_type: typename IDENTIFIER  */
#line 276 "libtailslide/lslmini.y"
    {
        (yyval.identifier) = ALLOCATOR->newTracked<LSLIdentifier>((yyvsp[-1].type), (yyvsp[0].sval), &(yylsp[0]));
    }
#line 2062 "libtailslide/lslmini.tab.cc"
    break;

  case 9: /* global_variable: name_type ';'  */
#line 283 "libtailslide/lslmini.y"
    {
        (yyval.global) = ALLOCATOR->newTracked<LSLGlobalVariable>((yyvsp[-1].identifier), nullptr);
    }
#line 2070 "libtailslide/lslmini.tab.cc"
    break;

  case 10: /* global_variable: name_type '=' expression ';'  */
#line 287 "libtailslide/lslmini.y"
    {
        (yyval.global) = ALLOCATOR->newTracked<LSLGlobalVariable>((yyvsp[-3].identifier), (yyvsp[-1].expression));
    }
#line 2078 "libtailslide/lslmini.tab.cc"
    break;

  case 11: /* global_variable: name_type '=' error ';'  */
#line 291 "libtailslide/lslmini.y"
    {
        (yyval.global) = ALLOCATOR->newTracked<LSLGlobalVariable>((yyvsp[-3].identifier), nullptr);
    }
#line 2086 "libtailslide/lslmini.tab.cc"
    break;

  case 12: /* constant: '-' INTEGER_CONSTANT  */
#line 298 "libtailslide/lslmini.y"
    {
        (yyval.constant) = ALLOCATOR->newTracked<LSLIntegerConstant>(negate_wrapping((yyvsp[0].ival)));
        (yyval.constant)->setWasNegated(true);
    }
#line 2095 "libtailslide/lslmini.tab.cc"
    break;

  case 13: /* constant: INTEGER_CONSTANT  */
#line 303 "libtailslide/lslmini.y"
    {
        (yyval.constant) = ALLOCATOR->newTracked<LSLIntegerConstant>((yyvsp[0].ival));
    }
#line 2103 "libtailslide/lslmini.tab.cc"
    break;

  case 14: /* constant: '-' FP_CONSTANT  */
#line 307 "libtailslide/lslmini.y"
    {
        (yyval.constant) = ALLOCATOR->newTracked<LSLFloatConstant>(-(yyvsp[0].fval));
        (yyval.constant)->setWasNegated(true);
    }
#line 2112 "libtailslide/lslmini.tab.cc"
    break;

  case 15: /* constant: FP_CONSTANT  */
#line 312 "libtailslide/lslmini.y"
    {
        (yyval.constant) = ALLOCATOR->newTracked<LSLFloatConstant>((yyvsp[0].fval));
    }
#line 2120 "libtailslide/lslmini.tab.cc"
    break;

  case 16: /* constant: STRING_CONSTANT  */
#line 316 "libtailslide/lslmini.y"
    {
        (yyval.constant) = ALLOCATOR->newTracked<LSLStringConstant>((yyvsp[0].sval));
    }
#line 2128 "libtailslide/lslmini.tab.cc"
    break;

  case 17: /* typename: INTEGER  */
#line 323 "libtailslide/lslmini.y"
    {
        (yyval.type) = TYPE(LST_INTEGER);
    }
#line 2136 "libtailslide/lslmini.tab.cc"
    break;

  case 18: /* typename: FLOAT_TYPE  */
#line 327 "libtailslide/lslmini.y"
    {
        (yyval.type) = TYPE(LST_FLOATINGPOINT);
    }
#line 2144 "libtailslide/lslmini.tab.cc"
    break;

  case 19: /* typename: STRING  */
#line 331 "libtailslide/lslmini.y"
    {
        (yyval.type) = TYPE(LST_STRING);
    }
#line 2152 "libtailslide/lslmini.tab.cc"
    break;

  case 20: /* typename: LLKEY  */
#line 335 "libtailslide/lslmini.y"
    {
        (yyval.type) = TYPE(LST_KEY);
    }
#line 2160 "libtailslide/lslmini.tab.cc"
    break;

  case 21: /* typename: VECTOR  */
#line 339 "libtailslide/lslmini.y"
    {
        (yyval.type) = TYPE(LST_VECTOR);
    }
#line 2168 "libtailslide/lslmini.tab.cc"
    break;

  case 22: /* typename: QUATERNION  */
#line 343 "libtailslide/lslmini.y"
    {
        (yyval.type) = TYPE(LST_QUATERNION);
    }
#line 2176 "libtailslide/lslmini.tab.cc"
    break;

  case 23: /* typename: LIST  */
#line 347 "libtailslide/lslmini.y"
    {
        (yyval.type) = TYPE(LST_LIST);
    }
#line 2184 "libtailslide/lslmini.tab.cc"
    break;

  case 24: /* global_function: IDENTIFIER '(' ')' compound_statement  */
#line 354 "libtailslide/lslmini.y"
    {
        (yyval.global_funcs) = ALLOCATOR->newTracked<LSLGlobalFunction>(
            MAKEID(LST_NULL, (yyvsp[-3].sval), (yylsp[-3])),
            ALLOCATOR->newTracked<LSLFunctionDec>(nullptr),
            (yyvsp[0].statement)
        );
    }
#line 2196 "libtailslide/lslmini.tab.cc"
    break;

  case 25: /* global_function: name_type '(' ')' compound_statement  */
#line 362 "libtailslide/lslmini.y"
    {
        (yyval.global_funcs) = ALLOCATOR->newTracked<LSLGlobalFunction>(
            (yyvsp[-3].identifier),
            ALLOCATOR->newTracked<LSLFunctionDec>(nullptr),
            (yyvsp[0].statement)
        );
    }
#line 2208 "libtailslide/lslmini.tab.cc"
    break;

  case 26: /* global_function: IDENTIFIER '(' function_parameters ')' compound_statement  */
#line 370 "libtailslide/lslmini.y"
    {
        (yyval.global_funcs) = ALLOCATOR->newTracked<LSLGlobalFunction>(
            MAKEID(LST_NULL, (yyvsp[-4].sval), (yylsp[-4])),
            ALLOCATOR->newTracked<LSLFunctionDec>((yyvsp[-2].identifier)),
            (yyvsp[0].statement)
        );
    }
#line 2220 "libtailslide/lslmini.tab.cc"
    break;

  case 27: /* global_function: name_type '(' function_parameters ')' compound_statement  */
#line 378 "libtailslide/lslmini.y"
    {
        (yyval.global_funcs) = ALLOCATOR->newTracked<LSLGlobalFunction>(
            (yyvsp[-4].identifier),
            ALLOCATOR->newTracked<LSLFunctionDec>((yyvsp[-2].identifier)),
            (yyvsp[0].statement)
        );
    }
#line 2232 "libtailslide/lslmini.tab.cc"
    break;

  case 28: /* function_parameters: function_parameter  */
#line 389 "libtailslide/lslmini.y"
    {
        (yyval.identifier) = (yyvsp[0].identifier);
    }
#line 2240 "libtailslide/lslmini.tab.cc"
    break;

  case 29: /* function_parameters: function_parameter ',' function_parameters  */
#line 393 "libtailslide/lslmini.y"
    {
        if ( (yyvsp[-2].identifier) ) {
            (yyvsp[-2].identifier)->setNext((yyvsp[0].identifier));
            (yyval.identifier) = (yyvsp[-2].identifier);
        } else {
            (yyval.identifier) = (yyvsp[0].identifier);
        }
    }
#line 2253 "libtailslide/lslmini.tab.cc"
    break;

  case 30: /* function_parameter: typename IDENTIFIER  */
#line 405 "libtailslide/lslmini.y"
    {
        (yyval.identifier) = ALLOCATOR->newTracked<LSLIdentifier>((yyvsp[-1].type), (yyvsp[0].sval), &(yylsp[0]));
    }
#line 2261 "libtailslide/lslmini.tab.cc"
    break;

  case 31: /* event_parameters: event_parameter  */
#line 412 "libtailslide/lslmini.y"
    {
        (yyval.identifier) = (yyvsp[0].identifier);
    }
#line 2269 "libtailslide/lslmini.tab.cc"
    break;

  case 32: /* event_parameters: event_parameter ',' event_parameters  */
#line 416 "libtailslide/lslmini.y"
    {
        if ( (yyvsp[-2].identifier) ) {
            (yyvsp[-2].identifier)->setNext((yyvsp[0].identifier));
            (yyval.identifier) = (yyvsp[-2].identifier);
        } else {
            (yyval.identifier) = (yyvsp[0].identifier);
        }
    }
#line 2282 "libtailslide/lslmini.tab.cc"
    break;

  case 33: /* event_parameter: typename IDENTIFIER  */
#line 428 "libtailslide/lslmini.y"
    {
        (yyval.identifier) = ALLOCATOR->newTracked<LSLIdentifier>((yyvsp[-1].type), (yyvsp[0].sval), &(yylsp[0]));
    }
#line 2290 "libtailslide/lslmini.tab.cc"
    break;

  case 34: /* states: default  */
#line 435 "libtailslide/lslmini.y"
    {
        (yyval.state) = (yyvsp[0].state);
    }
#line 2298 "libtailslide/lslmini.tab.cc"
    break;

  case 35: /* states: default other_states  */
#line 439 "libtailslide/lslmini.y"
    {
        if ( (yyvsp[-1].state) ) {
            DEBUG( LOG_DEBUG_SPAM, nullptr, "---- default [%p,%p] other_states [%p,%p]\n", (yyvsp[-1].state)->getPrev(), (yyvsp[-1].state)->getNext(), (yyvsp[0].state)->getPrev(), (yyvsp[0].state)->getNext());
            (yyvsp[-1].state)->setNext((yyvsp[0].state));
            (yyval.state) = (yyvsp[-1].state);
        } else {
            (yyval.state) = (yyvsp[0].state);
        }
    }
#line 2312 "libtailslide/lslmini.tab.cc"
    break;

  case 36: /* other_states: state  */
#line 452 "libtailslide/lslmini.y"
    {
        //DEBUG(200,"--(%d)-- state\n", yylloc.first_line);
        (yyval.state) = (yyvsp[0].state);
    }
#line 2321 "libtailslide/lslmini.tab.cc"
    break;

  case 37: /* other_states: state other_states  */
#line 457 "libtailslide/lslmini.y"
    {
        //DEBUG(200,"--(%d)-- state other_states\n", yylloc.first_line);
        if ( (yyvsp[-1].state) ) {
            (yyvsp[-1].state)->setNext((yyvsp[0].state));
            (yyval.state) = (yyvsp[-1].state);
        } else {
            (yyval.state) = (yyvsp[0].state);
        }
    }
#line 2335 "libtailslide/lslmini.tab.cc"
    break;

  case 38: /* default: STATE_DEFAULT '{' state_body '}'  */
#line 470 "libtailslide/lslmini.y"
    {
        (yyval.state) = ALLOCATOR->newTracked<LSLState>(
            MAKEID(LST_NULL, (yyvsp[-3].sval), (yylsp[-3])),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLEventHandler>>((yyvsp[-1].handler))
        );
    }
#line 2346 "libtailslide/lslmini.tab.cc"
    break;

  case 39: /* default: STATE_DEFAULT '{' '}'  */
#line 477 "libtailslide/lslmini.y"
    {
        tailslide_get_extra(scanner)->logger->error( &(yylsp[-2]), E_NO_EVENT_HANDLERS );
        (yyval.state) = ALLOCATOR->newTracked<LSLState>(
            MAKEID(LST_NULL, (yyvsp[-2].sval), (yylsp[-2])),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLEventHandler>>(nullptr)
        );
    }
#line 2358 "libtailslide/lslmini.tab.cc"
    break;

  case 40: /* state: STATE IDENTIFIER '{' state_body '}'  */
#line 488 "libtailslide/lslmini.y"
    {
        (yyval.state) = ALLOCATOR->newTracked<LSLState>(
            MAKEID(LST_NULL, (yyvsp[-3].sval), (yylsp[-3])),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLEventHandler>>((yyvsp[-1].handler))
        );
    }
#line 2369 "libtailslide/lslmini.tab.cc"
    break;

  case 41: /* state: STATE IDENTIFIER '{' '}'  */
#line 495 "libtailslide/lslmini.y"
    {
        tailslide_get_extra(scanner)->logger->error( &(yylsp[-3]), E_NO_EVENT_HANDLERS );
        (yyval.state) = ALLOCATOR->newTracked<LSLState>(
            MAKEID(LST_NULL, (yyvsp[-2].sval), (yylsp[-2])),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLEventHandler>>(nullptr)
        );
    }
#line 2381 "libtailslide/lslmini.tab.cc"
    break;

  case 42: /* state_body: event  */
#line 506 "libtailslide/lslmini.y"
    {
        (yyval.handler) = (yyvsp[0].handler);
    }
#line 2389 "libtailslide/lslmini.tab.cc"
    break;

  case 43: /* state_body: event state_body  */
#line 510 "libtailslide/lslmini.y"
    {
        if ( (yyvsp[-1].handler) ) {
            (yyvsp[-1].handler)->setNext((yyvsp[0].handler));
            (yyval.handler) = (yyvsp[-1].handler);
        } else {
            (yyval.handler) = (yyvsp[0].handler);
        }
    }
#line 2402 "libtailslide/lslmini.tab.cc"
    break;

  case 44: /* event: IDENTIFIER '(' ')' compound_statement  */
#line 522 "libtailslide/lslmini.y"
    {
        (yyval.handler) = ALLOCATOR->newTracked<LSLEventHandler>(
            MAKEID(LST_NULL, (yyvsp[-3].sval), (yylsp[-3])),
            ALLOCATOR->newTracked<LSLEventDec>(nullptr),
            (yyvsp[0].statement)
        );
    }
#line 2414 "libtailslide/lslmini.tab.cc"
    break;

  case 45: /* event: IDENTIFIER '(' event_parameters ')' compound_statement  */
#line 530 "libtailslide/lslmini.y"
    {
        (yyval.handler) = ALLOCATOR->newTracked<LSLEventHandler>(
            MAKEID(LST_NULL, (yyvsp[-4].sval), (yylsp[-4])),
            ALLOCATOR->newTracked<LSLEventDec>((yyvsp[-2].identifier)),
            (yyvsp[0].statement)
        );
    }
#line 2426 "libtailslide/lslmini.tab.cc"
    break;

  case 46: /* compound_statement: '{' '}'  */
#line 541 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLCompoundStatement>(nullptr);
    }
#line 2434 "libtailslide/lslmini.tab.cc"
    break;

  case 47: /* compound_statement: '{' statements '}'  */
#line 545 "libtailslide/lslmini.y"
    {
        // $2 is the tail of the statement list, walk back to find the head.
        auto *head = (yyvsp[-1].statement);
        while (auto *prev_head=(LSLStatement*)head->getPrev()) {
            head = prev_head;
        }
        (yyval.statement) = ALLOCATOR->newTracked<LSLCompoundStatement>(head);
    }
#line 2447 "libtailslide/lslmini.tab.cc"
    break;

  case 48: /* statements: statement  */
#line 557 "libtailslide/lslmini.y"
    {
        //DEBUG( LOG_DEBUG_SPAM, nullptr, "statement %d\n", yylloc.first_line );
        (yyval.statement) = (yyvsp[0].statement);
    }
#line 2456 "libtailslide/lslmini.tab.cc"
    break;

  case 49: /* statements: statements statement  */
#line 562 "libtailslide/lslmini.y"
    {
        if ( (yyvsp[-1].statement) ) {
            // Not setNext() due to left recursion, we keep track of the tail of the list instead
            // and walk back to find the head once we've added all statements.
            (yyvsp[0].statement)->setPrev((yyvsp[-1].statement));
            (yyval.statement) = (yyvsp[0].statement);
        } else {
            (yyval.statement) = (yyvsp[0].statement);
        }
    }
#line 2471 "libtailslide/lslmini.tab.cc"
    break;

  case 50: /* statement: ';'  */
#line 576 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLNopStatement>();
    }
#line 2479 "libtailslide/lslmini.tab.cc"
    break;

  case 51: /* statement: STATE IDENTIFIER ';'  */
#line 580 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLStateStatement>(MAKEID(LST_NULL, (yyvsp[-1].sval), (yylsp[-1])));
    }
#line 2487 "libtailslide/lslmini.tab.cc"
    break;

  case 52: /* statement: STATE STATE_DEFAULT ';'  */
#line 584 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLStateStatement>(MAKEID(LST_NULL, (yyvsp[-1].sval), (yylsp[-1])));
    }
#line 2495 "libtailslide/lslmini.tab.cc"
    break;

  case 53: /* statement: JUMP IDENTIFIER ';'  */
#line 588 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLJumpStatement>(MAKEID(LST_NULL, (yyvsp[-1].sval), (yylsp[-1])));
    }
#line 2503 "libtailslide/lslmini.tab.cc"
    break;

  case 54: /* statement: '@' IDENTIFIER ';'  */
#line 592 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLLabel>(MAKEID(LST_NULL, (yyvsp[-1].sval), (yylsp[-1])));
    }
#line 2511 "libtailslide/lslmini.tab.cc"
    break;

  case 55: /* statement: RETURN expression ';'  */
#line 596 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLReturnStatement>((yyvsp[-1].expression));
    }
#line 2519 "libtailslide/lslmini.tab.cc"
    break;

  case 56: /* statement: RETURN ';'  */
#line 600 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLReturnStatement>(nullptr);
    }
#line 2527 "libtailslide/lslmini.tab.cc"
    break;

  case 57: /* statement: expression ';'  */
#line 604 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLExpressionStatement>((yyvsp[-1].expression));
    }
#line 2535 "libtailslide/lslmini.tab.cc"
    break;

  case 58: /* statement: declaration ';'  */
#line 608 "libtailslide/lslmini.y"
    {
        (yyval.statement) = (yyvsp[-1].statement);
    }
#line 2543 "libtailslide/lslmini.tab.cc"
    break;

  case 59: /* statement: compound_statement  */
#line 612 "libtailslide/lslmini.y"
    {
        (yyval.statement) = (yyvsp[0].statement);
    }
#line 2551 "libtailslide/lslmini.tab.cc"
    break;

  case 60: /* statement: IF '(' expression ')' statement  */
#line 616 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLIfStatement>((yyvsp[-2].expression), (yyvsp[0].statement), nullptr);
        (yyvsp[0].statement)->setDeclarationAllowed(false);
    }
#line 2560 "libtailslide/lslmini.tab.cc"
    break;

  case 61: /* statement: IF '(' expression ')' statement ELSE statement  */
#line 621 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLIfStatement>((yyvsp[-4].expression), (yyvsp[-2].statement), (yyvsp[0].statement));
        (yyvsp[-2].statement)->setDeclarationAllowed(false);
        (yyvsp[0].statement)->setDeclarationAllowed(false);
    }
#line 2570 "libtailslide/lslmini.tab.cc"
    break;

  case 62: /* statement: FOR '(' forexpressionlist ';' expression ';' forexpressionlist ')' statement  */
#line 627 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLForStatement>(
            ALLOCATOR->newTracked<LSLASTNodeList<LSLExpression>>((yyvsp[-6].expression)),
            (yyvsp[-4].expression),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLExpression>>((yyvsp[-2].expression)),
            (yyvsp[0].statement)
        );
        (yyvsp[0].statement)->setDeclarationAllowed(false);
    }
#line 2584 "libtailslide/lslmini.tab.cc"
    break;

  case 63: /* statement: DO statement WHILE '(' expression ')' ';'  */
#line 637 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLDoStatement>((yyvsp[-5].statement), (yyvsp[-2].expression));
        (yyvsp[-5].statement)->setDeclarationAllowed(false);
    }
#line 2593 "libtailslide/lslmini.tab.cc"
    break;

  case 64: /* statement: WHILE '(' expression ')' statement  */
#line 642 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLWhileStatement>((yyvsp[-2].expression), (yyvsp[0].statement));
        (yyvsp[0].statement)->setDeclarationAllowed(false);
    }
#line 2602 "libtailslide/lslmini.tab.cc"
    break;

  case 65: /* statement: error ';'  */
#line 647 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLNopStatement>();
    }
#line 2610 "libtailslide/lslmini.tab.cc"
    break;

  case 66: /* declaration: typename IDENTIFIER  */
#line 654 "libtailslide/lslmini.y"
    {
        (yyval.statement) = ALLOCATOR->newTracked<LSLDeclaration>(ALLOCATOR->newTracked<LSLIdentifier>((yyvsp[-1].type), (yyvsp[0].sval), &(yylsp[0])), nullptr);
    }
#line 2618 "libtailslide/lslmini.tab.cc"
    break;

  case 67: /* declaration: typename IDENTIFIER '=' expression  */
#line 658 "libtailslide/lslmini.y"
    {
        DEBUG( LOG_DEBUG_SPAM, nullptr, "= %s\n", (yyvsp[0].expression)->getNodeName().c_str());
        (yyval.statement) = ALLOCATOR->newTracked<LSLDeclaration>(ALLOCATOR->newTracked<LSLIdentifier>((yyvsp[-3].type), (yyvsp[-2].sval), &(yylsp[-2])), (yyvsp[0].expression));
    }
#line 2627 "libtailslide/lslmini.tab.cc"
    break;

  case 68: /* forexpressionlist: %empty  */
#line 667 "libtailslide/lslmini.y"
    {
        (yyval.expression) = nullptr;
    }
#line 2635 "libtailslide/lslmini.tab.cc"
    break;

  case 69: /* forexpressionlist: nextforexpressionlist  */
#line 671 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 2643 "libtailslide/lslmini.tab.cc"
    break;

  case 70: /* nextforexpressionlist: expression  */
#line 678 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 2651 "libtailslide/lslmini.tab.cc"
    break;

  case 71: /* nextforexpressionlist: expression ',' nextforexpressionlist  */
#line 682 "libtailslide/lslmini.y"
    {
        if ( (yyvsp[-2].expression) ) {
            (yyvsp[-2].expression)->setNext((yyvsp[0].expression));
            (yyval.expression) = (yyvsp[-2].expression);
        } else {
            (yyval.expression) = (yyvsp[0].expression);
        }
    }
#line 2664 "libtailslide/lslmini.tab.cc"
    break;

  case 72: /* funcexpressionlist: %empty  */
#line 694 "libtailslide/lslmini.y"
    {
        //$$ = ALLOCATOR->newTracked<LSLExpression>(0);
        (yyval.expression) = nullptr;
    }
#line 2673 "libtailslide/lslmini.tab.cc"
    break;

  case 73: /* funcexpressionlist: nextfuncexpressionlist  */
#line 699 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 2681 "libtailslide/lslmini.tab.cc"
    break;

  case 74: /* nextfuncexpressionlist: expression  */
#line 706 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 2689 "libtailslide/lslmini.tab.cc"
    break;

  case 75: /* nextfuncexpressionlist: expression ',' nextfuncexpressionlist  */
#line 710 "libtailslide/lslmini.y"
    {
        if ( (yyvsp[-2].expression) ) {
            (yyvsp[-2].expression)->setNext((yyvsp[0].expression));
            (yyval.expression) = (yyvsp[-2].expression);
        } else {
            (yyval.expression) = (yyvsp[0].expression);
        }
    }
#line 2702 "libtailslide/lslmini.tab.cc"
    break;

  case 76: /* listexpressionlist: %empty  */
#line 722 "libtailslide/lslmini.y"
    {
        //$$ = ALLOCATOR->newTracked<LSLExpression>(0);
        //$$ = nullptr;
        (yyval.expression) = nullptr;
    }
#line 2712 "libtailslide/lslmini.tab.cc"
    break;

  case 77: /* listexpressionlist: nextlistexpressionlist  */
#line 728 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 2720 "libtailslide/lslmini.tab.cc"
    break;

  case 78: /* nextlistexpressionlist: expression  */
#line 735 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 2728 "libtailslide/lslmini.tab.cc"
    break;

  case 79: /* nextlistexpressionlist: expression ',' nextlistexpressionlist  */
#line 739 "libtailslide/lslmini.y"
    {
        if ((yyvsp[-2].expression)) {
            (yyvsp[-2].expression)->setNext((yyvsp[0].expression));
            (yyval.expression) = (yyvsp[-2].expression);
        } else {
            (yyval.expression) = (yyvsp[0].expression);
        }
    }
#line 2741 "libtailslide/lslmini.tab.cc"
    break;

  case 80: /* expression: unaryexpression  */
#line 751 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 2749 "libtailslide/lslmini.tab.cc"
    break;

  case 81: /* expression: lvalue '=' expression  */
#line 755 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_ASSIGN, (yyvsp[0].expression) );
    }
#line 2757 "libtailslide/lslmini.tab.cc"
    break;

  case 82: /* expression: lvalue ADD_ASSIGN expression  */
#line 759 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_ADD_ASSIGN, (yyvsp[0].expression) );
    }
#line 2765 "libtailslide/lslmini.tab.cc"
    break;

  case 83: /* expression: lvalue SUB_ASSIGN expression  */
#line 763 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_SUB_ASSIGN, (yyvsp[0].expression) );
    }
#line 2773 "libtailslide/lslmini.tab.cc"
    break;

  case 84: /* expression: lvalue MUL_ASSIGN expression  */
#line 767 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_MUL_ASSIGN, (yyvsp[0].expression) );
    }
#line 2781 "libtailslide/lslmini.tab.cc"
    break;

  case 85: /* expression: lvalue DIV_ASSIGN expression  */
#line 771 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_DIV_ASSIGN, (yyvsp[0].expression) );
    }
#line 2789 "libtailslide/lslmini.tab.cc"
    break;

  case 86: /* expression: lvalue MOD_ASSIGN expression  */
#line 775 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_MOD_ASSIGN, (yyvsp[0].expression) );
    }
#line 2797 "libtailslide/lslmini.tab.cc"
    break;

  case 87: /* expression: expression EQ expression  */
#line 779 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_EQ, (yyvsp[0].expression) );
    }
#line 2805 "libtailslide/lslmini.tab.cc"
    break;

  case 88: /* expression: expression NEQ expression  */
#line 783 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_NEQ, (yyvsp[0].expression) );
    }
#line 2813 "libtailslide/lslmini.tab.cc"
    break;

  case 89: /* expression: expression LEQ expression  */
#line 787 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_LEQ, (yyvsp[0].expression) );
    }
#line 2821 "libtailslide/lslmini.tab.cc"
    break;

  case 90: /* expression: expression GEQ expression  */
#line 791 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_GEQ, (yyvsp[0].expression) );
    }
#line 2829 "libtailslide/lslmini.tab.cc"
    break;

  case 91: /* expression: expression '<' expression  */
#line 795 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_LESS, (yyvsp[0].expression) );
    }
#line 2837 "libtailslide/lslmini.tab.cc"
    break;

  case 92: /* expression: expression '>' expression  */
#line 799 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_GREATER, (yyvsp[0].expression) );
    }
#line 2845 "libtailslide/lslmini.tab.cc"
    break;

  case 93: /* expression: expression '+' expression  */
#line 803 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_PLUS, (yyvsp[0].expression) );
    }
#line 2853 "libtailslide/lslmini.tab.cc"
    break;

  case 94: /* expression: expression '-' expression  */
#line 807 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_MINUS, (yyvsp[0].expression) );
    }
#line 2861 "libtailslide/lslmini.tab.cc"
    break;

  case 95: /* expression: expression '*' expression  */
#line 811 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>( (yyvsp[-2].expression), OP_MUL, (yyvsp[0].expression) );
    }
#line 2869 "libtailslide/lslmini.tab.cc"
    break;

  case 96: /* expression: expression '/' expression  */
#line 815 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>(  (yyvsp[-2].expression), OP_DIV,  (yyvsp[0].expression)  );
    }
#line 2877 "libtailslide/lslmini.tab.cc"
    break;

  case 97: /* expression: expression '%' expression  */
#line 819 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>(  (yyvsp[-2].expression), OP_MOD,  (yyvsp[0].expression)  );
    }
#line 2885 "libtailslide/lslmini.tab.cc"
    break;

  case 98: /* expression: expression '&' expression  */
#line 823 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>(  (yyvsp[-2].expression), OP_BIT_AND,  (yyvsp[0].expression)  );
    }
#line 2893 "libtailslide/lslmini.tab.cc"
    break;

  case 99: /* expression: expression '|' expression  */
#line 827 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>(  (yyvsp[-2].expression), OP_BIT_OR,  (yyvsp[0].expression)  );
    }
#line 2901 "libtailslide/lslmini.tab.cc"
    break;

  case 100: /* expression: expression '^' expression  */
#line 831 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>(  (yyvsp[-2].expression), OP_BIT_XOR,  (yyvsp[0].expression)  );
    }
#line 2909 "libtailslide/lslmini.tab.cc"
    break;

  case 101: /* expression: expression BOOLEAN_AND expression  */
#line 835 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>(  (yyvsp[-2].expression), OP_BOOLEAN_AND,  (yyvsp[0].expression)  );
    }
#line 2917 "libtailslide/lslmini.tab.cc"
    break;

  case 102: /* expression: expression BOOLEAN_OR expression  */
#line 839 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>(  (yyvsp[-2].expression), OP_BOOLEAN_OR,  (yyvsp[0].expression)  );
    }
#line 2925 "libtailslide/lslmini.tab.cc"
    break;

  case 103: /* expression: expression SHIFT_LEFT expression  */
#line 843 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>(  (yyvsp[-2].expression), OP_SHIFT_LEFT,  (yyvsp[0].expression)  );
    }
#line 2933 "libtailslide/lslmini.tab.cc"
    break;

  case 104: /* expression: expression SHIFT_RIGHT expression  */
#line 847 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLBinaryExpression>(  (yyvsp[-2].expression), OP_SHIFT_RIGHT,  (yyvsp[0].expression)  );
    }
#line 2941 "libtailslide/lslmini.tab.cc"
    break;

  case 105: /* unaryexpression: '-' expression  */
#line 854 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLUnaryExpression>( (yyvsp[0].expression), OP_MINUS );
    }
#line 2949 "libtailslide/lslmini.tab.cc"
    break;

  case 106: /* unaryexpression: '!' expression  */
#line 858 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLUnaryExpression>(  (yyvsp[0].expression) , OP_BOOLEAN_NOT );
    }
#line 2957 "libtailslide/lslmini.tab.cc"
    break;

  case 107: /* unaryexpression: '~' expression  */
#line 862 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLUnaryExpression>(  (yyvsp[0].expression) , OP_BIT_NOT );
    }
#line 2965 "libtailslide/lslmini.tab.cc"
    break;

  case 108: /* unaryexpression: INC_OP lvalue  */
#line 866 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLUnaryExpression>(  (yyvsp[0].expression) , OP_PRE_INCR );
    }
#line 2973 "libtailslide/lslmini.tab.cc"
    break;

  case 109: /* unaryexpression: DEC_OP lvalue  */
#line 870 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLUnaryExpression>(  (yyvsp[0].expression) , OP_PRE_DECR );
    }
#line 2981 "libtailslide/lslmini.tab.cc"
    break;

  case 110: /* unaryexpression: typecast  */
#line 874 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 2989 "libtailslide/lslmini.tab.cc"
    break;

  case 111: /* unaryexpression: unarypostfixexpression  */
#line 878 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 2997 "libtailslide/lslmini.tab.cc"
    break;

  case 112: /* unaryexpression: '(' expression ')'  */
#line 882 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLParenthesisExpression>((yyvsp[-1].expression));
    }
#line 3005 "libtailslide/lslmini.tab.cc"
    break;

  case 113: /* typecast: '(' typename ')' lvalue  */
#line 889 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLTypecastExpression>((yyvsp[-2].type), (yyvsp[0].expression));
    }
#line 3013 "libtailslide/lslmini.tab.cc"
    break;

  case 114: /* typecast: '(' typename ')' '-' IDENTIFIER  */
#line 893 "libtailslide/lslmini.y"
    {
        // this branch is necessary because integer and float builtin constants
        // are no longer treated as integer_constant and float_constant lexer tokens
        bool valid_id_typecast = false;
        if (auto *sym = tailslide_get_extra(scanner)->builtins->lookup((yyvsp[0].sval), SYM_VARIABLE))
            valid_id_typecast = sym->getSubType() == SYM_BUILTIN && (sym->getIType() == LST_INTEGER || sym->getIType() == LST_FLOATINGPOINT);

        auto *lvalue = ALLOCATOR->newTracked<LSLLValueExpression>(MAKEID(LST_NULL, (yyvsp[0].sval), (yylsp[0])), nullptr);
        (yyval.expression) = ALLOCATOR->newTracked<LSLTypecastExpression>((yyvsp[-3].type), ALLOCATOR->newTracked<LSLUnaryExpression>(lvalue, OP_MINUS));
        if (!valid_id_typecast) {
            tailslide_get_extra(scanner)->logger->error(&(yylsp[-3]), E_SYNTAX_ERROR, "Typecast requires parentheses");
        }
    }
#line 3031 "libtailslide/lslmini.tab.cc"
    break;

  case 115: /* typecast: '(' typename ')' constant  */
#line 907 "libtailslide/lslmini.y"
    {
        // No, this rule isn't a mistake even though constants are included in the
        // unarypostfixexpression case. _Specifically_ negated constants within do not
        // get parsed as `unary_minus(num_literal)` by LL's compiler because the internal
        // negation case in `constant` gets handled first. just mark this as not negated and
        // wrap it in a ConstantExpression.
        (yyvsp[0].constant)->setWasNegated(false);
        (yyval.expression) = ALLOCATOR->newTracked<LSLTypecastExpression>(
            (yyvsp[-2].type),
            ALLOCATOR->newTracked<LSLConstantExpression>((yyvsp[0].constant))
        );
    }
#line 3048 "libtailslide/lslmini.tab.cc"
    break;

  case 116: /* typecast: '(' typename ')' unarypostfixexpression  */
#line 920 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLTypecastExpression>((yyvsp[-2].type), (yyvsp[0].expression));
    }
#line 3056 "libtailslide/lslmini.tab.cc"
    break;

  case 117: /* typecast: '(' typename ')' '(' expression ')'  */
#line 924 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLTypecastExpression>((yyvsp[-4].type), (yyvsp[-1].expression));
    }
#line 3064 "libtailslide/lslmini.tab.cc"
    break;

  case 118: /* unarypostfixexpression: vector_initializer  */
#line 931 "libtailslide/lslmini.y"
    {
        DEBUG( LOG_DEBUG_SPAM, nullptr, "vector intializer..");
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 3073 "libtailslide/lslmini.tab.cc"
    break;

  case 119: /* unarypostfixexpression: quaternion_initializer  */
#line 936 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 3081 "libtailslide/lslmini.tab.cc"
    break;

  case 120: /* unarypostfixexpression: list_initializer  */
#line 940 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 3089 "libtailslide/lslmini.tab.cc"
    break;

  case 121: /* unarypostfixexpression: lvalue  */
#line 944 "libtailslide/lslmini.y"
    {
        (yyval.expression) = (yyvsp[0].expression);
    }
#line 3097 "libtailslide/lslmini.tab.cc"
    break;

  case 122: /* unarypostfixexpression: lvalue INC_OP  */
#line 948 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLUnaryExpression>(  (yyvsp[-1].expression) , OP_POST_INCR );
    }
#line 3105 "libtailslide/lslmini.tab.cc"
    break;

  case 123: /* unarypostfixexpression: lvalue DEC_OP  */
#line 952 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLUnaryExpression>(  (yyvsp[-1].expression) , OP_POST_DECR );
    }
#line 3113 "libtailslide/lslmini.tab.cc"
    break;

  case 124: /* unarypostfixexpression: IDENTIFIER '(' funcexpressionlist ')'  */
#line 956 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLFunctionExpression>(
            ALLOCATOR->newTracked<LSLIdentifier>((yyvsp[-3].sval)),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLExpression>>((yyvsp[-1].expression))
        );
    }
#line 3124 "libtailslide/lslmini.tab.cc"
    break;

  case 125: /* unarypostfixexpression: PRINT '(' expression ')'  */
#line 963 "libtailslide/lslmini.y"
    {
    /* This is effectively a no-op to most people in modern LSL, but we might need
       `expression`'s side-effects. Oh well. */
        (yyval.expression) = ALLOCATOR->newTracked<LSLPrintExpression>((yyvsp[-1].expression));
    }
#line 3134 "libtailslide/lslmini.tab.cc"
    break;

  case 126: /* unarypostfixexpression: constant  */
#line 969 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLConstantExpression>((yyvsp[0].constant));
    }
#line 3142 "libtailslide/lslmini.tab.cc"
    break;

  case 127: /* vector_initializer: '<' expression ',' expression ',' expression '>'  */
#line 976 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLVectorExpression>((yyvsp[-5].expression), (yyvsp[-3].expression), (yyvsp[-1].expression));
    }
#line 3150 "libtailslide/lslmini.tab.cc"
    break;

  case 128: /* quaternion_initializer: '<' expression ',' expression ',' expression ',' expression '>'  */
#line 983 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLQuaternionExpression>((yyvsp[-7].expression), (yyvsp[-5].expression), (yyvsp[-3].expression), (yyvsp[-1].expression));
    }
#line 3158 "libtailslide/lslmini.tab.cc"
    break;

  case 129: /* list_initializer: '[' listexpressionlist ']'  */
#line 990 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLListExpression>((yyvsp[-1].expression));
    }
#line 3166 "libtailslide/lslmini.tab.cc"
    break;

  case 130: /* lvalue: IDENTIFIER  */
#line 997 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLLValueExpression>(
            ALLOCATOR->newTracked<LSLIdentifier>((yyvsp[0].sval)),
            nullptr
        );
    }
#line 3177 "libtailslide/lslmini.tab.cc"
    break;

  case 131: /* lvalue: IDENTIFIER PERIOD IDENTIFIER  */
#line 1004 "libtailslide/lslmini.y"
    {
        (yyval.expression) = ALLOCATOR->newTracked<LSLLValueExpression>(
            ALLOCATOR->newTracked<LSLIdentifier>((yyvsp[-2].sval)),
            ALLOCATOR->newTracked<LSLIdentifier>((yyvsp[0].sval))
        );
    }
#line 3188 "libtailslide/lslmini.tab.cc"
    break;


#line 3192 "libtailslide/lslmini.tab.cc"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == TAILSLIDE_EMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyssp, yytoken, &yylloc};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (&yylloc, scanner, yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= TAILSLIDE_EOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == TAILSLIDE_EOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, scanner);
          yychar = TAILSLIDE_EMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, scanner, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != TAILSLIDE_EMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, scanner);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp, scanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}

#line 1012 "libtailslide/lslmini.y"


int yyerror( YYLTYPE *lloc, void *scanner, const char *message ) {
    tailslide_get_extra(scanner)->logger->error( lloc, E_SYNTAX_ERROR, message );
    tailslide_get_extra(scanner)->ast_sane = false;
    return 0;
}
