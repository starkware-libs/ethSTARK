namespace starkware {

template <size_t N>
BaseFieldElement InnerProduct(
    const std::array<BaseFieldElement, N>& vector_a,
    const std::array<BaseFieldElement, N>& vector_b) {
  BaseFieldElement sum = BaseFieldElement::Zero();
  for (size_t i = 0; i < N; ++i) {
    sum += vector_a.at(i) * vector_b.at(i);
  }
  return sum;
}

template <size_t N>
void LinearTransformation(
    const std::array<std::array<BaseFieldElement, N>, N>& matrix,
    const std::array<BaseFieldElement, N>& vector, std::array<BaseFieldElement, N>* output) {
  for (size_t i = 0; i < N; ++i) {
    output->at(i) = InnerProduct(matrix.at(i), vector);
  }
}

}  // namespace starkware
