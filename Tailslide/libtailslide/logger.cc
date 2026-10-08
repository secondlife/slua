#include <cstdarg>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "logger.hh"
#include "lslmini.hh"

namespace Tailslide {

struct LogMessageSort {
  bool operator()(LogMessage *const &left, LogMessage *const &right) {
    if (left->getType() < right->getType())
      return true;
    else if (left->getType() > right->getType())
      return false;

    if (left->getLoc()->first_line < right->getLoc()->first_line)
      return true;
    else if (left->getLoc()->first_line > right->getLoc()->first_line)
      return false;

    if (left->getLoc()->first_column < right->getLoc()->first_column)
      return true;

    return false;
  }
};

void Logger::reset() {
  _mMessages.clear();
  _mErrors = 0;
  _mWarnings = 0;
}

void Logger::log(LogLevel level, YYLTYPE *yylloc, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  logv(level, yylloc, fmt, args);
  va_end(args);
}

void Logger::error(YYLTYPE *yylloc, int error, ...) {
  char buf[1025] = {0};
  std::ostringstream oss;
  const char *fmt;
  LogLevel level = (error < W_WARNING) ? LOG_ERROR : LOG_WARN;
  va_list args;

  // pick out the format string for the error message
  if (level == LOG_ERROR) {
    fmt = _sErrorMessages[(int) (error - E_ERROR)];
  } else {
    fmt = _sWarningMessages[(int) (error - W_WARNING)];
  }

  // Format the actual error message
  va_start(args, error);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  oss << buf;

  // Update error/warning counts
  if (level == LOG_ERROR) {
    ++_mErrors;
  } else {
    ++_mWarnings;
  }

  std::string message = oss.str();
  _mMessages.push_back(createMessage(level, yylloc, message, (ErrorCode) error));
  if (_mSort)
    std::sort(_mMessages.begin(), _mMessages.end(), LogMessageSort());
}


void Logger::logv(LogLevel level, YYLTYPE *yylloc, const char *fmt, va_list args, int error) {
  switch (level) {
    case LOG_ERROR:
      ++_mErrors;
      break;
    case LOG_WARN:
      ++_mWarnings;
      break;
    case LOG_INFO:
      if (!_mShowInfo) return;
      break;
    case LOG_DEBUG:
    case LOG_DEBUG_MINOR:
    case LOG_DEBUG_SPAM:
#ifdef DEBUG_LEVEL
      if ( DEBUG_LEVEL < level ) return;
#else /* not DEBUG_LEVEL */
      return;
#endif /* not DEBUG_LEVEL */
      break;
    default:
      break;
  }

  char buf[1025] = {0};
  vsnprintf(buf, sizeof(buf), fmt, args);
  _mMessages.push_back(createMessage(level, yylloc, buf, (ErrorCode) error));
  if (_mSort)
    std::sort(_mMessages.begin(), _mMessages.end(), LogMessageSort());
}

void Logger::printReport() {
  std::vector<LogMessage *>::iterator i;
  for (i = _mMessages.begin(); i != _mMessages.end(); ++i)
    fprintf(stderr, "%s\n", (*i)->toString().c_str());

  fprintf(stderr, "TOTAL:: Errors: %d  Warnings: %d\n", _mErrors, _mWarnings);
}

LogMessage* Logger::createMessage(LogLevel type, YYLTYPE *loc, const std::string &message, ErrorCode error) {
  return _mAllocator->newTracked<LogMessage>(type, loc, message.c_str(), error);
}

LogMessage::LogMessage(ScriptContext *ctx, LogLevel type, YYLTYPE *loc, const char *message, ErrorCode error)
    : TrackableObject(ctx), _mLogType(type), _mLoc({}), _mErrorCode(error) {
  if (loc) _mLoc = *loc;
  assert (message != nullptr);
  _mMessage = message;
}

std::string LogMessage::toString() const {
  std::ostringstream oss;

  const char *type = nullptr;
  switch (_mLogType) {
    case LOG_ERROR:
      type = "ERROR";
      break;
    case LOG_WARN:
      type = "WARN";
      break;
    case LOG_INFO:
      type = "INFO";
      break;
    case LOG_DEBUG:
    case LOG_DEBUG_MINOR:
    case LOG_DEBUG_SPAM:
      type = "DEBUG";
      break;
    default:
      type = "OTHER";
      break;
  }

  oss << std::setw(5) << type << ":: ";

  if (_mLoc.first_line > 0 || _mLoc.first_column > 0) {
    oss << "(" << std::setw(3) << _mLoc.first_line << ","
        << std::setw(3) << _mLoc.first_column << "): ";
  }

  if (_mErrorCode != 0) {
    oss << "[E" << _mErrorCode << "] ";
  }

  oss << _mMessage;

  return oss.str();
}


/// ERROR MESSAGE

// Sized off the enum so the table can never be indexed out of bounds by a
// valid ErrorCode; a missing message reads as null rather than as garbage.
const char *Logger::_sErrorMessages[E_LAST - E_ERROR] = {
        "ERROR",
        "Duplicate declaration of `%s'; previously declared at (%d, %d).",
        "Invalid operator: %s %s %s.",
        "",
        "",
        "Attempting to use `%s' as a %s, but it is a %s.",
        "`%s' is undeclared.",
        "`%s' is undeclared; did you mean %s?",
        "Invalid member: `%s.%s'.",
        "Trying to access `%s.%s', but `%1$s' is a %3$s",
        "Attempting to access `%s.%s', but `%1$s' is not a vector or rotation.",
        "Passing %s as argument %d of `%s' which is declared as `%s %s'.",
        "Too many arguments to function `%s'.",
        "Too few arguments to function `%s'.",
        "Functions cannot change state.",
        "`%s %s' assigned a %s value.",
        "%s member assigned %s value (must be float or integer).",
        "Event handlers cannot return a value.",
        "Returning a %s value from a %s function.",
        "Not all code paths return a value.",
        "%s", // Syntax error, bison includes all the info.
        "Global initializer must be constant.",
        "", // deprecated
        "State must have at least one event handler.",
        "Parser stack depth exceeded; SL will throw a syntax error here.",
        "`%s' is a constant and cannot be used as an lvalue.",
        "`%s' is a constant and cannot be used in a variable declaration.",
        "Declaring `%s' as parameter %d of `%s' which should be `%s %s'.",
        "Too many parameters for event `%s'.",
        "Too few parameters for event `%s'.",
        "`%s' is not a valid event name.",
        "`%s' is an event name, and cannot be used as an identifier.",
        "`%s' may not be declared here, create a new scope with { }.",
        "Multiple handlers for event `%s'",
        "Lists may not contain other lists",
        "May not cast %s to %s",
        "Lists may not contain nulls",
        "Stack-heap collision",
        "Void expression used as condition"
};

const char *Logger::_sWarningMessages[W_LAST - W_WARNING] = {
        "WARN",
        "Declaration of `%s' in this scope shadows previous declaration at (%d, %d)",
        "Suggest parentheses around assignment used as truth value.",
        "Changing state to current state acts the same as return, use return instead.",
        "Changing state in a list or string function will corrupt the stack",
        "Using an if statement to change state in a function is a hack and may have unintended side-effects.",
        "", // deprecated
        "Empty if statement.",
        "", // deprecated
        "%s `%s' declared but never used.",
        "Unused event parameter `%s'.",
        "Using == on lists only compares lengths.",
        "Condition is always true.",
        "Condition is always false.",
        "Empty loop body.",
        "`i_val *= f_val' can have unpredictable runtime behavior, prefer `i_val = (integer)(i_val * f_val)'",
        "`jump %s;' may jump to the wrong label due to label name clashes within function",
        "label `@%s' is declared multiple times in the same function, which may cause undesired behavior",
        "== comparison used as a statement",
        "`%s' is deprecated.",
        "`%s' is deprecated, use %s instead.",
};

}
