#!/usr/bin/env python3

import argparse
import json
import sys

from starkware.air.rescue.rescue_constants import PRIME
from starkware.main.rescue.rescue_end_to_end_test import generate_random_hex_witness_and_output

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Generates random private and public inputs for the rescue prover.')
    parser.add_argument('--chain_length', type=int)

    args = parser.parse_args()
    chain_length = args.chain_length
    if chain_length % 3 != 0:
        print('chain_length must be divisible by 3.')
        exit(1)

    witness_and_output = generate_random_hex_witness_and_output(PRIME, chain_length)
    witness, output = witness_and_output['witness'], witness_and_output['output']
    json.dump({'witness': witness}, open('example/rescue_private_input.json', 'w'), indent=4)
    json.dump(
        {'output': output, 'chain_length': chain_length},
        open('example/rescue_public_input.json', 'w'),
        indent=4)
