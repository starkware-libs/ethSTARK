#ifndef STARKWARE_UTILS_FLAG_VALIDATORS_H_
#define STARKWARE_UTILS_FLAG_VALIDATORS_H_

#include <string>

#include "glog/logging.h"

namespace starkware {

/*
  Returns true if the file file_name is readable, and false otherwise.
*/
bool ValidateInputFile(const char* /*flagname*/, const std::string& file_name);

/*
  Returns true if the file file_name is writable, and false otherwise.
*/
bool ValidateOutputFile(const char* /*flagname*/, const std::string& file_name);

/*
  Returns true if the file file_name is either empty or writable, and
  false otherwise.
*/
bool ValidateOptionalOutputFile(const char* flagname, const std::string& file_name);

}  // namespace starkware

#endif  // STARKWARE_UTILS_FLAG_VALIDATORS_H_
