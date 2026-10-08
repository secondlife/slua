#ifndef _LOGGER_HH
#define _LOGGER_HH 1

#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include <utility>  // pair

#include "allocator.hh"
#include "loctype.hh"

namespace Tailslide {

enum LogLevel {
  LOG_ERROR,            // errors
  LOG_WARN,             // warnings
  LOG_INTERNAL_ERROR,   // internal errors
  LOG_INFO,             // what we're up to
  LOG_DEBUG,            // base debug messages
  LOG_DEBUG_MINOR,      // minor debug messages
  LOG_DEBUG_SPAM,       // spammy debug messages
};

enum ErrorCode {
    // errors
    E_ERROR                 = 10000,
    E_DUPLICATE_DECLARATION = 10001,
    E_INVALID_OPERATOR = 10002,
    E_UNUSED_1 = 10003,
    E_UNUSED_2 = 10004,
    E_WRONG_TYPE = 10005,
    E_UNDECLARED = 10006,
    E_UNDECLARED_WITH_SUGGESTION = 10007,
    E_INVALID_MEMBER = 10008,
    E_MEMBER_NOT_VARIABLE = 10009,
    E_MEMBER_WRONG_TYPE = 10010,
    E_ARGUMENT_WRONG_TYPE = 10011,
    E_TOO_MANY_ARGUMENTS = 10012,
    E_TOO_FEW_ARGUMENTS = 10013,
    E_CHANGE_STATE_IN_FUNCTION = 10014,
    E_WRONG_TYPE_IN_ASSIGNMENT = 10015,
    E_WRONG_TYPE_IN_MEMBER_ASSIGNMENT = 10016,
    E_RETURN_VALUE_IN_EVENT_HANDLER = 10017,
    E_BAD_RETURN_TYPE = 10018,
    E_NOT_ALL_PATHS_RETURN = 10019,
    E_SYNTAX_ERROR = 10020,
    E_GLOBAL_INITIALIZER_NOT_CONSTANT = 10021,
    E_DEPRECATED_NO_OPERATOR = 10022,
    E_NO_EVENT_HANDLERS = 10023,
    E_PARSER_STACK_DEPTH = 10024,
    E_BUILTIN_LVALUE = 10025,
    E_SHADOW_CONSTANT = 10026,
    E_ARGUMENT_WRONG_TYPE_EVENT = 10027,
    E_TOO_MANY_ARGUMENTS_EVENT = 10028,
    E_TOO_FEW_ARGUMENTS_EVENT = 10029,
    E_INVALID_EVENT = 10030,
    E_EVENT_AS_IDENTIFIER = 10031,
    E_DECLARATION_INVALID_HERE = 10032,
    E_MULTIPLE_EVENT_HANDLERS = 10033,
    E_LIST_IN_LIST = 10034,
    E_ILLEGAL_CAST = 10035,
    E_NULL_IN_LIST = 10036,
    E_STACK_HEAP_COLLISION = 10037,
    E_VOID_IN_CONDITION = 10038,
    E_LAST,




    // warnings
    W_WARNING               = 20000,
    W_SHADOW_DECLARATION = 20001,
    W_ASSIGNMENT_IN_COMPARISON = 20002,
    W_CHANGE_TO_CURRENT_STATE = 20003,
    W_CHANGE_STATE_HACK_CORRUPT = 20004,
    W_CHANGE_STATE_HACK = 20005,
    W_DEPRECATED_1 = 20006,
    W_EMPTY_IF = 20007,
    W_DEPRECATED_BAD_DECIMAL_LEX = 20008,
    W_DECLARED_BUT_NOT_USED = 20009,
    W_UNUSED_EVENT_PARAMETER = 20010,
    W_LIST_COMPARE = 20011,
    W_CONDITION_ALWAYS_TRUE = 20012,
    W_CONDITION_ALWAYS_FALSE = 20013,
    W_EMPTY_LOOP = 20014,
    W_INT_FLOAT_MUL_ASSIGN = 20015,
    W_JUMP_TO_WRONG_LABEL = 20016,
    W_DUPLICATE_LABEL_NAME = 20017,
    W_EQ_AS_STATEMENT = 20018,
    W_DEPRECATED = 20019,
    W_DEPRECATED_WITH_REPLACEMENT = 20020,
    W_LAST,
};

#ifndef HIDE_TAILSLIDE_INTERNALS
#define LINECOL(l)  (l)->first_line, (l)->first_column
#define NODE_ERROR(node, ...) do {(node)->mContext->logger->error((node)->getLoc(), __VA_ARGS__);} while(0)
#endif

class Logger {
  public:
    explicit Logger(ScriptAllocator *allocator) :
        _mErrors(0), _mWarnings(0), _mShowEnd(false), _mShowInfo(false), _mSort(true),
        _mAllocator(allocator) {};
    void log(LogLevel type, YYLTYPE *loc, const char *fmt, ...);
    void logv(LogLevel type, YYLTYPE *loc, const char *fmt, va_list args, int error=0);
    void error( YYLTYPE *loc, int error, ... );
    void printReport();
    void reset();

    const std::vector<class LogMessage*> & getMessages() const { return _mMessages; };
    int     getErrors() const    { return _mErrors;    }
    int     getWarnings() const  { return _mWarnings;  }
    void    setShowEnd(bool v) { _mShowEnd = v; }
    void    setShowInfo(bool v){ _mShowInfo = v;}
    void    setSort(bool v)     { _mSort = v;     }

    // Create a LogMessage without adding it to the internal message list
    LogMessage* createMessage(LogLevel type, YYLTYPE *loc, const std::string &message, ErrorCode error);

  protected:
    int     _mErrors;
    int     _mWarnings;
    bool    _mShowEnd;
    bool    _mShowInfo;
    bool    _mSort;
    ScriptAllocator *_mAllocator;

    std::vector<class LogMessage*>    _mMessages;
    static const char *_sErrorMessages[];
    static const char *_sWarningMessages[];
};

inline void DEBUG(LogLevel level, YYLTYPE *yylloc, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  //Logger::get()->logv(level, yylloc, fmt, args);
  va_end(args);
}

////////////////////////////////////////////////////////////////////////////////
// Log message entry, for sorting
class LogMessage: public TrackableObject {
  public:
    LogMessage( ScriptContext *ctx, LogLevel type, YYLTYPE *loc, const char *message, ErrorCode error );

    LogLevel    getType() { return _mLogType; }
    YYLTYPE    *getLoc()  { return &_mLoc;  }
    ErrorCode   getError() { return _mErrorCode; }
    const std::string &getMessage() { return _mMessage; }
    std::string toString() const;

  private:
    LogLevel            _mLogType;

    // we need our own copy of loc, because messages logged in the parser will be
    // handing us a copy of a loc structure that is constantly changing, and will
    // be invalid when we go to sort.
    YYLTYPE             _mLoc;

    std::string         _mMessage;
    ErrorCode           _mErrorCode;
};

}

#endif
