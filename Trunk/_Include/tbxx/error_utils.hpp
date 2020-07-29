#ifndef TBXX_ERROR_UTILS_H
#define TBXX_ERROR_UTILS_H

#include <tbxx/libc_backtrace.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

#ifdef _DEBUG
#define TBXX_CHECK_POINT \
  std::cout << __FILE__ << "(" << __LINE__ << ")" << std::endl
#define TBXX_CHECK_POINT_MSG(msg) \
  std::cout << msg << " @ " __FILE__ << "(" << __LINE__ << ")" << std::endl
#else
#define TBXX_CHECK_POINT \
  std::cout << "lib file " << "(" << __LINE__ << ")" << std::endl
#define TBXX_CHECK_POINT_MSG(msg) \
  std::cout << msg << " @ " "lib file " << "(" << __LINE__ << ")" << std::endl
#endif

#define TBXX_EXAMINE(A) \
  std::cout << "variable " << (#A) << ": " << A << std::endl

namespace tbxx { namespace error_utils {

  //! Simple debugging aid: add call to code, recompile, run in gdb, use "where"
  inline
  int
  segfault_if(
    bool condition)
  {
    if (condition) {
      int* ptr = 0;
      return *ptr;
    }
    return 0;
  }

  inline
  std::string
  file_and_line_as_string(
    const char* file,
    long line)
  {
    std::ostringstream o;
    o << file << "(" << line << ")";
    return o.str();
  }

  inline
  std::string
  file_and_line_as_string_with_backtrace(
    const char* file,
    long line)
  {
    std::ostringstream o;
    libc_backtrace::show_if_possible(o, 1);
    o << file << "(" << line << ")";
    return o.str();
  }

}} // tbxx::error_utils

#ifdef _DEBUG
#define TBXX_ASSERT(condition) \
  if (!(condition)) { \
    throw std::runtime_error( \
      tbxx::error_utils::file_and_line_as_string( \
        __FILE__, __LINE__) \
      + ": ASSERT(" #condition ") failure."); \
  }

#define TBXX_UNREACHABLE_ERROR() \
  std::runtime_error( \
    "Control flow passes through branch that should be unreachable: " \
    + tbxx::error_utils::file_and_line_as_string(__FILE__, __LINE__))

#define TBXX_NOT_IMPLEMENTED() \
  std::runtime_error( \
    "Not implemented: " \
    + tbxx::error_utils::file_and_line_as_string(__FILE__, __LINE__))
#else
#define TBXX_ASSERT(condition) \
  if (!(condition)) { \
    throw std::runtime_error( \
      tbxx::error_utils::file_and_line_as_string( \
        "lib file ", __LINE__) \
      + ": ASSERT(" #condition ") failure."); \
  }

#define TBXX_UNREACHABLE_ERROR() \
  std::runtime_error( \
    "Control flow passes through branch that should be unreachable: " \
    + tbxx::error_utils::file_and_line_as_string("lib file ", __LINE__))

#define TBXX_NOT_IMPLEMENTED() \
  std::runtime_error( \
    "Not implemented: " \
    + tbxx::error_utils::file_and_line_as_string("lib file ", __LINE__))
#endif // _DEBUG

#endif // GUARD
