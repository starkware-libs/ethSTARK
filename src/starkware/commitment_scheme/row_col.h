#ifndef STARKWARE_COMMITMENT_SCHEME_ROW_COL_H_
#define STARKWARE_COMMITMENT_SCHEME_ROW_COL_H_

#include <string>
#include <utility>

#include "starkware/math/math.h"

namespace starkware {

/*
  Represents a cell in a 2-dimensional array (row and column), with lexicographic comparison,
  i.e. (i,j) < (k,l) if i < k or i == k and j < l.
*/
struct RowCol {
 public:
  RowCol(uint64_t row, uint64_t col) : row_(row), col_(col) {}

  bool operator<(const RowCol& other) const { return AsPair() < other.AsPair(); }
  bool operator==(const RowCol& other) const { return AsPair() == other.AsPair(); }

  std::string ToString() const {
    return "(" + std::to_string(row_) + ", " + std::to_string(col_) + ")";
  }

  uint64_t GetRow() const { return row_; }
  uint64_t GetCol() const { return col_; }

 private:
  uint64_t row_;
  uint64_t col_;

  std::pair<uint64_t, uint64_t> AsPair() const { return {row_, col_}; }
};

inline std::ostream& operator<<(std::ostream& out, const RowCol& row_col) {
  return out << row_col.ToString();
}

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_ROW_COL_H_
