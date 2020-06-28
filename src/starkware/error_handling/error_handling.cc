#include "starkware/error_handling/error_handling.h"

#include <memory>
#include <sstream>
#include <string>

#define BACKWARD_HAS_DW 1  // Enable usage of libdw from the elfutils for stack trace annotation.
#include "third_party/backward-cpp/backward.hpp"

namespace {
/*
  Prints the current stack trace to the given stream.
*/
void PrintStackTrace(std::ostream* s) {
  const size_t max_stack_depth = 64;

  backward::StackTrace st;
  st.load_here(max_stack_depth);
  backward::Printer p;
  p.print(st, *s);
}
}  // namespace

namespace starkware {

void ThrowStarkwareException(
    const std::string& message, const char* file, size_t line_num) noexcept(false) {
  // Receive file as char* to do the string construction here, rather than at the call site.
  std::stringstream s;
  s << file << ":" << line_num << ": " << message << "\n";

  size_t orig_message_len = s.tellp();
  PrintStackTrace(&s);

  std::string exception_str = s.str();

  // Remove stack trace items, starting from error_handling.cc (which are usually irrelevant).
  size_t first_error_handling_line_pos =
      exception_str.find("src/starkware/error_handling/error_handling.cc\", line ");
  if (first_error_handling_line_pos != std::string::npos) {
    size_t line_start_pos = exception_str.rfind('\n', first_error_handling_line_pos);
    if (line_start_pos != std::string::npos) {
      exception_str = exception_str.substr(0, line_start_pos);
    }
  }
  throw StarkwareException(exception_str, orig_message_len);
}

void ThrowStarkwareException(const char* msg, const char* file, size_t line_num) noexcept(false) {
  const std::string message(msg);
  ThrowStarkwareException(message, file, line_num);
}

}  // namespace starkware
