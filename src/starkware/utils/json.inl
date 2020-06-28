namespace starkware {

template <typename FieldElementT>
FieldElementT JsonValue::AsFieldElement() const {
  AssertString();
  return FieldElementT::FromString(AsString());
}

template <typename T, typename Func>
std::vector<T> JsonValue::AsVector(const Func& func) const {
  AssertArray();
  std::vector<T> res;
  res.reserve(ArrayLength());
  for (size_t i = 0; i < ArrayLength(); i++) {
    res.push_back(func((*this)[i]));
  }
  return res;
}

}  // namespace starkware
