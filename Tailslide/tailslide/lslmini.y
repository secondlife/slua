%{
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

%}

%define api.prefix {tailslide_}
%define parse.error verbose
%locations
%define api.pure
%parse-param { void *scanner }
%lex-param { void *scanner }

%code requires {
#include "loctype.hh"
}

%union
{
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
};


%token                    INTEGER
%token                    FLOAT_TYPE
%token                    STRING
%token                    LLKEY
%token                    VECTOR
%token                    QUATERNION
%token                    LIST

%token                    STATE
%token                    EVENT
%token                    JUMP
%token                    RETURN

%token <sval>             IDENTIFIER
%token <sval>             STATE_DEFAULT

%token <ival>             INTEGER_CONSTANT

%token <fval>             FP_CONSTANT

%token <sval>             STRING_CONSTANT

%token                    INC_OP
%token                    DEC_OP
%token                    INC_PRE_OP
%token                    DEC_PRE_OP
%token                    INC_POST_OP
%token                    DEC_POST_OP
%token                    ADD_ASSIGN
%token                    SUB_ASSIGN
%token                    MUL_ASSIGN
%token                    DIV_ASSIGN
%token                    MOD_ASSIGN

%token                    EQ
%token                    NEQ
%token                    GEQ
%token                    LEQ

%token                    BOOLEAN_AND
%token                    BOOLEAN_OR

%token                    SHIFT_LEFT
%token                    SHIFT_RIGHT

%token                    IF
%token                    ELSE
%token                    FOR
%token                    DO
%token                    WHILE

%token                    PRINT

%token                    PERIOD

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE


%type <script>            lscript_program
%type <global_store>      globals
%type <global_store>      global
%type <global>            global_variable
%type <constant>          constant
%type <type>              typename
%type <global_funcs>      global_function
%type <identifier>        function_parameters
%type <identifier>        function_parameter
%type <identifier>        event_parameters
%type <identifier>        event_parameter
%type <state>             states
%type <state>             other_states
%type <state>             default
%type <state>             state
%type <handler>           state_body
%type <handler>           event
%type <statement>         compound_statement
%type <statement>         statement
%type <statement>         statements
%type <statement>         declaration
%type <statement>         ';'
%type <statement>         '@'
%type <expression>        nextforexpressionlist
%type <expression>        forexpressionlist
%type <expression>        nextfuncexpressionlist
%type <expression>        funcexpressionlist
%type <expression>        nextlistexpressionlist
%type <expression>        listexpressionlist
%type <expression>        unarypostfixexpression
%type <expression>        vector_initializer
%type <expression>        quaternion_initializer
%type <expression>        list_initializer
%type <expression>        lvalue
%type <expression>        '-'
%type <expression>        '!'
%type <expression>        '~'
%type <expression>        '='
%type <expression>        '<'
%type <expression>        '>'
%type <expression>        '+'
%type <expression>        '*'
%type <expression>        '/'
%type <expression>        '%'
%type <expression>        '&'
%type <expression>        '|'
%type <expression>        '^'
%type <expression>        ADD_ASSIGN
%type <expression>        SUB_ASSIGN
%type <expression>        MUL_ASSIGN
%type <expression>        DIV_ASSIGN
%type <expression>        MOD_ASSIGN
%type <expression>        EQ
%type <expression>        NEQ
%type <expression>        LEQ
%type <expression>        GEQ
%type <expression>        BOOLEAN_AND
%type <expression>        BOOLEAN_OR
%type <expression>        SHIFT_LEFT
%type <expression>        SHIFT_RIGHT
%type <expression>        INC_OP
%type <expression>        DEC_OP
%type <expression>        '('
%type <expression>        ')'
%type <expression>        PRINT
%type <identifier>        name_type
%type <expression>        expression
%type <expression>        unaryexpression
%type <expression>        typecast

%right    '=' MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN SUB_ASSIGN
%left     BOOLEAN_AND BOOLEAN_OR
%left     '|'
%left     '^'
%left     '&'
%left     EQ NEQ
%left     '<' LEQ '>' GEQ
%left     SHIFT_LEFT SHIFT_RIGHT
%left     '+' '-'
%left     '*' '/' '%'
%right    '!' '~' INC_OP DEC_OP
%nonassoc INITIALIZER

%%

lscript_program
    : globals states
    {
        tailslide_get_extra(scanner)->script = ALLOCATOR->newTracked<LSLScript>(
            ALLOCATOR->newTracked<LSLASTNodeList<LSLASTNode>>($1),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLState>>($2)
        );
    }
    | states
    {
        tailslide_get_extra(scanner)->script = ALLOCATOR->newTracked<LSLScript>(
            ALLOCATOR->newTracked<LSLASTNodeList<LSLASTNode>>(),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLState>>($1)
        );
    }
    ;

globals
    : global
    {
        DEBUG( LOG_DEBUG_SPAM, nullptr, "** global\n");
        $$ = $1;
    }
    | global globals
    {
        if ( $1 ) {
            DEBUG( LOG_DEBUG_SPAM, nullptr, "** global [%p,%p] globals [%p,%p]\n", $1->getPrev(), $1->getNext(), $2->getPrev(), $2->getNext());
            $1->setNext($2);
            $$ = $1;
        } else {
            $$ = $2;
        }
    }
    ;

global
    : global_variable
    {
        $$ = $1;
    }
    | global_function
    {
        $$ = $1;
    }
    ;

name_type
    : typename IDENTIFIER
    {
        $$ = ALLOCATOR->newTracked<LSLIdentifier>($1, $2, &@2);
    }
    ;

global_variable
    : name_type ';'
    {
        $$ = ALLOCATOR->newTracked<LSLGlobalVariable>($1, nullptr);
    }
    | name_type '=' expression ';'
    {
        $$ = ALLOCATOR->newTracked<LSLGlobalVariable>($1, $3);
    }
    | name_type '=' error ';'
    {
        $$ = ALLOCATOR->newTracked<LSLGlobalVariable>($1, nullptr);
    }
    ;

constant
    : '-' INTEGER_CONSTANT
    {
        $$ = ALLOCATOR->newTracked<LSLIntegerConstant>(-$2);
        $$->setWasNegated(true);
    }
    | INTEGER_CONSTANT
    {
        $$ = ALLOCATOR->newTracked<LSLIntegerConstant>($1);
    }
    | '-' FP_CONSTANT
    {
        $$ = ALLOCATOR->newTracked<LSLFloatConstant>(-$2);
        $$->setWasNegated(true);
    }
    | FP_CONSTANT
    {
        $$ = ALLOCATOR->newTracked<LSLFloatConstant>($1);
    }
    | STRING_CONSTANT
    {
        $$ = ALLOCATOR->newTracked<LSLStringConstant>($1);
    }
    ;

typename
    : INTEGER
    {
        $$ = TYPE(LST_INTEGER);
    }
    | FLOAT_TYPE
    {
        $$ = TYPE(LST_FLOATINGPOINT);
    }
    | STRING
    {
        $$ = TYPE(LST_STRING);
    }
    | LLKEY
    {
        $$ = TYPE(LST_KEY);
    }
    | VECTOR
    {
        $$ = TYPE(LST_VECTOR);
    }
    | QUATERNION
    {
        $$ = TYPE(LST_QUATERNION);
    }
    | LIST
    {
        $$ = TYPE(LST_LIST);
    }
    ;

global_function
    : IDENTIFIER '(' ')' compound_statement
    {
        $$ = ALLOCATOR->newTracked<LSLGlobalFunction>(
            MAKEID(LST_NULL, $1, @1),
            ALLOCATOR->newTracked<LSLFunctionDec>(nullptr),
            $4
        );
    }
    | name_type '(' ')' compound_statement
    {
        $$ = ALLOCATOR->newTracked<LSLGlobalFunction>(
            $1,
            ALLOCATOR->newTracked<LSLFunctionDec>(nullptr),
            $4
        );
    }
    | IDENTIFIER '(' function_parameters ')' compound_statement
    {
        $$ = ALLOCATOR->newTracked<LSLGlobalFunction>(
            MAKEID(LST_NULL, $1, @1),
            ALLOCATOR->newTracked<LSLFunctionDec>($3),
            $5
        );
    }
    | name_type '(' function_parameters ')' compound_statement
    {
        $$ = ALLOCATOR->newTracked<LSLGlobalFunction>(
            $1,
            ALLOCATOR->newTracked<LSLFunctionDec>($3),
            $5
        );
    }
    ;

function_parameters
    : function_parameter
    {
        $$ = $1;
    }
    | function_parameter ',' function_parameters
    {
        if ( $1 ) {
            $1->setNext($3);
            $$ = $1;
        } else {
            $$ = $3;
        }
    }
    ;

function_parameter
    : typename IDENTIFIER
    {
        $$ = ALLOCATOR->newTracked<LSLIdentifier>($1, $2, &@2);
    }
    ;

event_parameters
    : event_parameter
    {
        $$ = $1;
    }
    | event_parameter ',' event_parameters
    {
        if ( $1 ) {
            $1->setNext($3);
            $$ = $1;
        } else {
            $$ = $3;
        }
    }
    ;

event_parameter
    : typename IDENTIFIER
    {
        $$ = ALLOCATOR->newTracked<LSLIdentifier>($1, $2, &@2);
    }
    ;

states
    : default
    {
        $$ = $1;
    }
    | default other_states
    {
        if ( $1 ) {
            DEBUG( LOG_DEBUG_SPAM, nullptr, "---- default [%p,%p] other_states [%p,%p]\n", $1->getPrev(), $1->getNext(), $2->getPrev(), $2->getNext());
            $1->setNext($2);
            $$ = $1;
        } else {
            $$ = $2;
        }
    }
    ;

other_states
    : state
    {
        //DEBUG(200,"--(%d)-- state\n", yylloc.first_line);
        $$ = $1;
    }
    | state other_states
    {
        //DEBUG(200,"--(%d)-- state other_states\n", yylloc.first_line);
        if ( $1 ) {
            $1->setNext($2);
            $$ = $1;
        } else {
            $$ = $2;
        }
    }
    ;

default
    : STATE_DEFAULT '{' state_body '}'
    {
        $$ = ALLOCATOR->newTracked<LSLState>(
            MAKEID(LST_NULL, $1, @1),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLEventHandler>>($3)
        );
    }
    | STATE_DEFAULT '{' '}'
    {
        tailslide_get_extra(scanner)->logger->error( &@1, E_NO_EVENT_HANDLERS );
        $$ = ALLOCATOR->newTracked<LSLState>(
            MAKEID(LST_NULL, $1, @1),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLEventHandler>>(nullptr)
        );
    }
    ;

state
    : STATE IDENTIFIER '{' state_body '}'
    {
        $$ = ALLOCATOR->newTracked<LSLState>(
            MAKEID(LST_NULL, $2, @2),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLEventHandler>>($4)
        );
    }
    | STATE IDENTIFIER '{' '}'
    {
        tailslide_get_extra(scanner)->logger->error( &@1, E_NO_EVENT_HANDLERS );
        $$ = ALLOCATOR->newTracked<LSLState>(
            MAKEID(LST_NULL, $2, @2),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLEventHandler>>(nullptr)
        );
    }
    ;

state_body
    : event
    {
        $$ = $1;
    }
    | event state_body
    {
        if ( $1 ) {
            $1->setNext($2);
            $$ = $1;
        } else {
            $$ = $2;
        }
    }
    ;

event
    : IDENTIFIER '(' ')' compound_statement
    {
        $$ = ALLOCATOR->newTracked<LSLEventHandler>(
            MAKEID(LST_NULL, $1, @1),
            ALLOCATOR->newTracked<LSLEventDec>(nullptr),
            $4
        );
    }
    | IDENTIFIER '(' event_parameters ')' compound_statement
    {
        $$ = ALLOCATOR->newTracked<LSLEventHandler>(
            MAKEID(LST_NULL, $1, @1),
            ALLOCATOR->newTracked<LSLEventDec>($3),
            $5
        );
    }
   ;

compound_statement
    : '{' '}'
    {
        $$ = ALLOCATOR->newTracked<LSLCompoundStatement>(nullptr);
    }
    | '{' statements '}'
    {
        // $2 is the tail of the statement list, walk back to find the head.
        auto *head = $2;
        while (auto *prev_head=(LSLStatement*)head->getPrev()) {
            head = prev_head;
        }
        $$ = ALLOCATOR->newTracked<LSLCompoundStatement>(head);
    }
    ;

statements
    : statement
    {
        //DEBUG( LOG_DEBUG_SPAM, nullptr, "statement %d\n", yylloc.first_line );
        $$ = $1;
    }
    | statements statement
    {
        if ( $1 ) {
            // Not setNext() due to left recursion, we keep track of the tail of the list instead
            // and walk back to find the head once we've added all statements.
            $2->setPrev($1);
            $$ = $2;
        } else {
            $$ = $2;
        }
    }
    ;

statement
    : ';'
    {
        $$ = ALLOCATOR->newTracked<LSLNopStatement>();
    }
    | STATE IDENTIFIER ';'
    {
        $$ = ALLOCATOR->newTracked<LSLStateStatement>(MAKEID(LST_NULL, $2, @2));
    }
    | STATE STATE_DEFAULT ';'
    {
        $$ = ALLOCATOR->newTracked<LSLStateStatement>(MAKEID(LST_NULL, $2, @2));
    }
    | JUMP IDENTIFIER ';'
    {
        $$ = ALLOCATOR->newTracked<LSLJumpStatement>(MAKEID(LST_NULL, $2, @2));
    }
    | '@' IDENTIFIER ';'
    {
        $$ = ALLOCATOR->newTracked<LSLLabel>(MAKEID(LST_NULL, $2, @2));
    }
    | RETURN expression ';'
    {
        $$ = ALLOCATOR->newTracked<LSLReturnStatement>($2);
    }
    | RETURN ';'
    {
        $$ = ALLOCATOR->newTracked<LSLReturnStatement>(nullptr);
    }
    | expression ';'
    {
        $$ = ALLOCATOR->newTracked<LSLExpressionStatement>($1);
    }
    | declaration ';'
    {
        $$ = $1;
    }
    | compound_statement
    {
        $$ = $1;
    }
    | IF '(' expression ')' statement    %prec LOWER_THAN_ELSE
    {
        $$ = ALLOCATOR->newTracked<LSLIfStatement>($3, $5, nullptr);
        $5->setDeclarationAllowed(false);
    }
    | IF '(' expression ')' statement ELSE statement
    {
        $$ = ALLOCATOR->newTracked<LSLIfStatement>($3, $5, $7);
        $5->setDeclarationAllowed(false);
        $7->setDeclarationAllowed(false);
    }
    | FOR '(' forexpressionlist ';' expression ';' forexpressionlist ')' statement
    {
        $$ = ALLOCATOR->newTracked<LSLForStatement>(
            ALLOCATOR->newTracked<LSLASTNodeList<LSLExpression>>($3),
            $5,
            ALLOCATOR->newTracked<LSLASTNodeList<LSLExpression>>($7),
            $9
        );
        $9->setDeclarationAllowed(false);
    }
    | DO statement WHILE '(' expression ')' ';'
    {
        $$ = ALLOCATOR->newTracked<LSLDoStatement>($2, $5);
        $2->setDeclarationAllowed(false);
    }
    | WHILE '(' expression ')' statement
    {
        $$ = ALLOCATOR->newTracked<LSLWhileStatement>($3, $5);
        $5->setDeclarationAllowed(false);
    }
    | error ';'
    {
        $$ = ALLOCATOR->newTracked<LSLNopStatement>();
    }
    ;

declaration
    : typename IDENTIFIER
    {
        $$ = ALLOCATOR->newTracked<LSLDeclaration>(ALLOCATOR->newTracked<LSLIdentifier>($1, $2, &@2), nullptr);
    }
    | typename IDENTIFIER '=' expression
    {
        DEBUG( LOG_DEBUG_SPAM, nullptr, "= %s\n", $4->getNodeName().c_str());
        $$ = ALLOCATOR->newTracked<LSLDeclaration>(ALLOCATOR->newTracked<LSLIdentifier>($1, $2, &@2), $4);
    }
    ;


forexpressionlist
    : /* empty */
    {
        $$ = nullptr;
    }
    | nextforexpressionlist
    {
        $$ = $1;
    }
    ;

nextforexpressionlist
    : expression
    {
        $$ = $1;
    }
    | expression ',' nextforexpressionlist
    {
        if ( $1 ) {
            $1->setNext($3);
            $$ = $1;
        } else {
            $$ = $3;
        }
    }
    ;

funcexpressionlist
    : /* empty */
    {
        //$$ = ALLOCATOR->newTracked<LSLExpression>(0);
        $$ = nullptr;
    }
    | nextfuncexpressionlist
    {
        $$ = $1;
    }
    ;

nextfuncexpressionlist
    : expression
    {
        $$ = $1;
    }
    | expression ',' nextfuncexpressionlist
    {
        if ( $1 ) {
            $1->setNext($3);
            $$ = $1;
        } else {
            $$ = $3;
        }
    }
    ;

listexpressionlist
    : /* empty */
    {
        //$$ = ALLOCATOR->newTracked<LSLExpression>(0);
        //$$ = nullptr;
        $$ = nullptr;
    }
    | nextlistexpressionlist
    {
        $$ = $1;
    }
    ;

nextlistexpressionlist
    : expression
    {
        $$ = $1;
    }
    | expression ',' nextlistexpressionlist
    {
        if ($1) {
            $1->setNext($3);
            $$ = $1;
        } else {
            $$ = $3;
        }
    }
    ;

expression
    : unaryexpression
    {
        $$ = $1;
    }
    | lvalue '=' expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_ASSIGN, $3 );
    }
    | lvalue ADD_ASSIGN expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_ADD_ASSIGN, $3 );
    }
    | lvalue SUB_ASSIGN expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_SUB_ASSIGN, $3 );
    }
    | lvalue MUL_ASSIGN expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_MUL_ASSIGN, $3 );
    }
    | lvalue DIV_ASSIGN expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_DIV_ASSIGN, $3 );
    }
    | lvalue MOD_ASSIGN expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_MOD_ASSIGN, $3 );
    }
    | expression EQ expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_EQ, $3 );
    }
    | expression NEQ expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_NEQ, $3 );
    }
    | expression LEQ expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_LEQ, $3 );
    }
    | expression GEQ expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_GEQ, $3 );
    }
    | expression '<' expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_LESS, $3 );
    }
    | expression '>' expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_GREATER, $3 );
    }
    | expression '+' expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_PLUS, $3 );
    }
    | expression '-' expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_MINUS, $3 );
    }
    | expression '*' expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>( $1, OP_MUL, $3 );
    }
    | expression '/' expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>(  $1, OP_DIV,  $3  );
    }
    | expression '%' expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>(  $1, OP_MOD,  $3  );
    }
    | expression '&' expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>(  $1, OP_BIT_AND,  $3  );
    }
    | expression '|' expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>(  $1, OP_BIT_OR,  $3  );
    }
    | expression '^' expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>(  $1, OP_BIT_XOR,  $3  );
    }
    | expression BOOLEAN_AND expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>(  $1, OP_BOOLEAN_AND,  $3  );
    }
    | expression BOOLEAN_OR expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>(  $1, OP_BOOLEAN_OR,  $3  );
    }
    | expression SHIFT_LEFT expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>(  $1, OP_SHIFT_LEFT,  $3  );
    }
    | expression SHIFT_RIGHT expression
    {
        $$ = ALLOCATOR->newTracked<LSLBinaryExpression>(  $1, OP_SHIFT_RIGHT,  $3  );
    }
    ;

unaryexpression
    : '-' expression
    {
        $$ = ALLOCATOR->newTracked<LSLUnaryExpression>( $2, OP_MINUS );
    }
    | '!' expression
    {
        $$ = ALLOCATOR->newTracked<LSLUnaryExpression>(  $2 , OP_BOOLEAN_NOT );
    }
    | '~' expression
    {
        $$ = ALLOCATOR->newTracked<LSLUnaryExpression>(  $2 , OP_BIT_NOT );
    }
    | INC_OP lvalue
    {
        $$ = ALLOCATOR->newTracked<LSLUnaryExpression>(  $2 , OP_PRE_INCR );
    }
    | DEC_OP lvalue
    {
        $$ = ALLOCATOR->newTracked<LSLUnaryExpression>(  $2 , OP_PRE_DECR );
    }
    | typecast
    {
        $$ = $1;
    }
    | unarypostfixexpression
    {
        $$ = $1;
    }
    | '(' expression ')'
    {
        $$ = ALLOCATOR->newTracked<LSLParenthesisExpression>($2);
    }
    ;

typecast
    : '(' typename ')' lvalue
    {
        $$ = ALLOCATOR->newTracked<LSLTypecastExpression>($2, $4);
    }
    | '(' typename ')' '-' IDENTIFIER
    {
        // this branch is necessary because integer and float builtin constants
        // are no longer treated as integer_constant and float_constant lexer tokens
        bool valid_id_typecast = false;
        if (auto *sym = tailslide_get_extra(scanner)->builtins->lookup($5, SYM_VARIABLE))
            valid_id_typecast = sym->getSubType() == SYM_BUILTIN && (sym->getIType() == LST_INTEGER || sym->getIType() == LST_FLOATINGPOINT);

        auto *lvalue = ALLOCATOR->newTracked<LSLLValueExpression>(MAKEID(LST_NULL, $5, @5), nullptr);
        $$ = ALLOCATOR->newTracked<LSLTypecastExpression>($2, ALLOCATOR->newTracked<LSLUnaryExpression>(lvalue, OP_MINUS));
        if (!valid_id_typecast) {
            tailslide_get_extra(scanner)->logger->error(&@2, E_SYNTAX_ERROR, "Typecast requires parentheses");
        }
    }
    | '(' typename ')' constant
    {
        // No, this rule isn't a mistake even though constants are included in the
        // unarypostfixexpression case. _Specifically_ negated constants within do not
        // get parsed as `unary_minus(num_literal)` by LL's compiler because the internal
        // negation case in `constant` gets handled first. just mark this as not negated and
        // wrap it in a ConstantExpression.
        $4->setWasNegated(false);
        $$ = ALLOCATOR->newTracked<LSLTypecastExpression>(
            $2,
            ALLOCATOR->newTracked<LSLConstantExpression>($4)
        );
    }
    | '(' typename ')' unarypostfixexpression
    {
        $$ = ALLOCATOR->newTracked<LSLTypecastExpression>($2, $4);
    }
    | '(' typename ')' '(' expression ')'
    {
        $$ = ALLOCATOR->newTracked<LSLTypecastExpression>($2, $5);
    }
    ;

unarypostfixexpression
    : vector_initializer
    {
        DEBUG( LOG_DEBUG_SPAM, nullptr, "vector intializer..");
        $$ = $1;
    }
    | quaternion_initializer
    {
        $$ = $1;
    }
    | list_initializer
    {
        $$ = $1;
    }
    | lvalue
    {
        $$ = $1;
    }
    | lvalue INC_OP
    {
        $$ = ALLOCATOR->newTracked<LSLUnaryExpression>(  $1 , OP_POST_INCR );
    }
    | lvalue DEC_OP
    {
        $$ = ALLOCATOR->newTracked<LSLUnaryExpression>(  $1 , OP_POST_DECR );
    }
    | IDENTIFIER '(' funcexpressionlist ')'
    {
        $$ = ALLOCATOR->newTracked<LSLFunctionExpression>(
            ALLOCATOR->newTracked<LSLIdentifier>($1),
            ALLOCATOR->newTracked<LSLASTNodeList<LSLExpression>>($3)
        );
    }
    | PRINT '(' expression ')'
    {
    /* This is effectively a no-op to most people in modern LSL, but we might need
       `expression`'s side-effects. Oh well. */
        $$ = ALLOCATOR->newTracked<LSLPrintExpression>($3);
    }
    | constant
    {
        $$ = ALLOCATOR->newTracked<LSLConstantExpression>($1);
    }
    ;

vector_initializer
    : '<' expression ',' expression ',' expression '>'    %prec INITIALIZER
    {
        $$ = ALLOCATOR->newTracked<LSLVectorExpression>($2, $4, $6);
    }
    ;

quaternion_initializer
    : '<' expression ',' expression ',' expression ',' expression '>' %prec INITIALIZER
    {
        $$ = ALLOCATOR->newTracked<LSLQuaternionExpression>($2, $4, $6, $8);
    }
    ;

list_initializer
    : '[' listexpressionlist ']' %prec INITIALIZER
    {
        $$ = ALLOCATOR->newTracked<LSLListExpression>($2);
    }
    ;

lvalue
    : IDENTIFIER
    {
        $$ = ALLOCATOR->newTracked<LSLLValueExpression>(
            ALLOCATOR->newTracked<LSLIdentifier>($1),
            nullptr
        );
    }
    | IDENTIFIER PERIOD IDENTIFIER
    {
        $$ = ALLOCATOR->newTracked<LSLLValueExpression>(
            ALLOCATOR->newTracked<LSLIdentifier>($1),
            ALLOCATOR->newTracked<LSLIdentifier>($3)
        );
    }
    ;

%%

int yyerror( YYLTYPE *lloc, void *scanner, const char *message ) {
    tailslide_get_extra(scanner)->logger->error( lloc, E_SYNTAX_ERROR, message );
    tailslide_get_extra(scanner)->ast_sane = false;
    return 0;
}
