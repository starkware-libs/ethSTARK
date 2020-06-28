namespace starkware {

template <typename FieldElementT>
FieldElementT PeriodicColumn::EvalAtPoint(const FieldElementT& x) const {
  const FieldElementT point = Pow(x, n_copies_);
  FieldElementT output = FieldElementT::Uninitialized();
  lde_manager_.EvalAtPoints<FieldElementT>(
      0, gsl::make_span(&point, 1), gsl::make_span(&output, 1));

  return output;
}

}  // namespace starkware
