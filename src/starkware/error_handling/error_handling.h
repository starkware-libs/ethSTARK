#ifndef STARKWARE_ERROR_HANDLING_ERROR_HANDLING_H_
#define STARKWARE_ERROR_HANDLING_ERROR_HANDLING_H_

#include <cassert>
#include <exception>
#include <string>
#include <utility>

namespace starkware {

class StarkwareException : public std::exception {
 public:
  explicit StarkwareException(std::string message, size_t message_len)
      : message_(std::move(message)), message_len_(message_len) {}
  const char* what() const noexcept { return message_.c_str(); }  // NOLINT

  const std::string Message() const { return message_.substr(0, message_len_); }

 private:
  std::string message_;
  size_t message_len_;
};

[[noreturn]] void ThrowStarkwareException(
    const std::string& message, const char* file, size_t line_num) noexcept(false);

/*
  Overload to accept msg as char* when possible to move the std::string construction
  from the call site to the ThrowStarkwareException function.
*/
[[noreturn]] void ThrowStarkwareException(
    const char* msg, const char* file, size_t line_num) noexcept(false);

#define THROW_STARKWARE_EXCEPTION(msg) ::starkware::ThrowStarkwareException(msg, __FILE__, __LINE__)

/*
  We use "do {} while(false);" pattern to force the user to use ; after the macro.
*/
#define ASSERT_IMPL(cond, msg)        \
  do {                                \
    if (!(cond)) {                    \
      THROW_STARKWARE_EXCEPTION(msg); \
    }                                 \
  } while (false)

#ifndef NDEBUG
#define ASSERT_DEBUG(cond, msg) ASSERT_IMPL(cond, msg)
#else
#define ASSERT_DEBUG(cond, msg) \
  do {                          \
  } while (false)
#endif

#define ASSERT_RELEASE(cond, msg) ASSERT_IMPL(cond, msg)

}  // namespace starkware

#endif  // STARKWARE_ERROR_HANDLING_ERROR_HANDLING_H_
