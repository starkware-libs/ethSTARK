from hashlib import sha256

PRIME = 2 ** 61 + 20 * 2 ** 32 + 1


def generate_round_constant(name, idx, field=GF(PRIME)):
    """
    Returns a field element based on the result of sha256.
    The input to sha256 is the concatenation of the name of the hash function and an index.
    """
    return field(int(sha256('%s%d' % (name, idx)).hexdigest(), 16))


def generate_mds_matrix(field=GF(PRIME), m=12):
    """
    Generates an MDS matrix of size m x m over the given field, with no eigenvalues in the field.
    Given two disjoint sets of size m: {x_1, ..., x_m}, {y_1, ..., y_m} we set
    A_{ij} = 1 / (x_i - y_j).
    """
    name = 'MarvellousMDS'
    for attempt in xrange(100):
        x_values = [generate_round_constant(name + 'x', attempt * m + i, field)
                    for i in xrange(m)]
        y_values = [generate_round_constant(name + 'y', attempt * m + i, field)
                    for i in xrange(m)]
        # Make sure the values are distinct.
        assert len(set(x_values + y_values)) == 2 * m, \
            'The values of x_values and y_values are not distinct.'
        mds = matrix([[1 / (x_values[i] - y_values[j]) for j in xrange(m)]
                      for i in xrange(m)])

        # Sanity check: check the determinant of the matrix.
        x_prod = product(
            [x_values[i] - x_values[j] for i in xrange(m) for j in xrange(i)])
        y_prod = product(
            [y_values[i] - y_values[j] for i in xrange(m) for j in xrange(i)])
        xy_prod = product(
            [x_values[i] - y_values[j] for i in xrange(m) for j in xrange(m)])
        expected_det = (1 if m % 4 < 2 else -1) * x_prod * y_prod / xy_prod
        det = mds.determinant()
        assert det != 0
        assert det == expected_det, \
            'Expected determinant %s. Found %s.' % (expected_det, det)

        if len(mds.characteristic_polynomial().roots()) == 0:
            # There are no eigenvalues in the field.
            return mds
    raise Exception("Couldn't find a good MDS matrix.")


def generate_marvellous_round_constants(field=GF(PRIME), m=12):
    round_constants = [
        vector(generate_round_constant('MarvellousK', m * i + j, field)
               for j in xrange(m))
        for i in xrange(21)]

    # Check differential uniformity, as described in the report on the security of the Rescue hash
    # function.
    mds_inverse = generate_mds_matrix().inverse()
    for i, round_c in enumerate(round_constants):
        round_c = vector(field, round_c)
        # The variable weight_round_c is the weight of the round constant.
        # Weight is defined as the number of non-zero elements in the vector.
        weight_round_c = len(round_c.coefficients())
        weight_mds_inverse_round_c = len((mds_inverse * round_c).coefficients())
        assert weight_round_c == m, \
            'round_c_%d = %s failed check. wt(round_c_%d) = %d.' % (i, round_c, i, weight_round_c)
        assert weight_mds_inverse_round_c == m, \
            'round_c_%d = %s failed check. wt(MDS^{-1} * round_c_%d) = %d.' % (
                i,
                round_c, i,
                weight_mds_inverse_round_c)
    return round_constants
