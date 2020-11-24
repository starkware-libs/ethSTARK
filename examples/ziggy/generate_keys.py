#!/usr/bin/env python3

import argparse
import json
import os
import random
import sys

from starkware.air.rescue.rescue_constants import PRIME
from starkware.main.ziggy.ziggy_end_to_end_test import generate_key_pair

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Generates random private and public keys for the ziggy scheme.')
    parser.add_argument('--private_key', type=str)
    parser.add_argument('--message', type=str)

    args = parser.parse_args()
    message = args.message if args.message is not None else 'Hello World!'
    private_key = args.private_key
    if private_key is not None:
        private_key = int(private_key, 16).to_bytes(32, 'big')

    keys = generate_key_pair(PRIME, private_key)
    private_key, public_key = keys['private_key'], keys['public_key']
    example_dir = os.path.dirname(__file__)
    json.dump({'private_key': private_key}, open(
        os.path.join(example_dir, 'ziggy_private_input.json'), 'w'), indent=4)
    json.dump(
        {'message': message, 'public_key': public_key},
        open(os.path.join(example_dir, 'ziggy_public_input.json'), 'w'),
        indent=4)
