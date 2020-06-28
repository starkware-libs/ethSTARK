#include "starkware/utils/flag_validators.h"

#include <fstream>
#include <string>

#include "glog/logging.h"

namespace starkware {

bool ValidateInputFile(const char* /*flagname*/, const std::string& file_name) {
  std::ifstream file(file_name);
  return static_cast<bool>(file);
}

bool ValidateOutputFile(const char* /*flagname*/, const std::string& file_name) {
  bool file_existed = std::ifstream(file_name).good();
  std::ofstream file(file_name, std::ofstream::app);
  bool can_write_file = static_cast<bool>(file);
  file.close();

  if (!file_existed && can_write_file) {
    std::remove(file_name.c_str());
  }
  return can_write_file;
}

bool ValidateOptionalOutputFile(const char* flagname, const std::string& file_name) {
  return (file_name.empty() || ValidateOutputFile(flagname, file_name));
}

}  // namespace starkware
