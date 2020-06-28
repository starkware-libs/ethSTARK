namespace starkware {

template <typename FieldElementT>
std::enable_if_t<kIsFieldElement<FieldElementT>, std::ostream&> operator<<(
    std::ostream& out, const FieldElementT& element) {
  return out << element.AsDerived().ToString();
}

template <typename Derived>
Derived& FieldElementBase<Derived>::operator+=(const Derived& other) {
  return AsDerived() = AsDerived() + other;
}

template <typename Derived>
Derived& FieldElementBase<Derived>::operator*=(const Derived& other) {
  return AsDerived() = AsDerived() * other;
}

template <typename Derived>
Derived FieldElementBase<Derived>::operator/(const Derived& other) const {
  return AsDerived() * other.Inverse();
}

template <typename Derived>
constexpr bool FieldElementBase<Derived>::operator!=(const Derived& other) const {
  return !(AsDerived() == other);
}

}  // namespace starkware
