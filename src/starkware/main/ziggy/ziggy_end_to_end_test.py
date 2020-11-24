#!/usr/bin/env python3

import hashlib
import json
import os
import random
import subprocess
import sys
import tempfile
from math import ceil, log2

import pytest

from starkware.air.rescue.rescue_constants import PRIME
from starkware.air.rescue.rescue_hash import rescue_hash
from starkware.main.test_utils import ModifyProverOutput, compare_prover_and_verifier_annotations

DIR = 'src/starkware/main/ziggy'
PROVER_EXE = 'ziggy_prover'
VERIFIER_EXE = 'ziggy_verifier'
MAX_NUMBER_QUERIES = 30


precomputed_keys = {
    'private_key': '0x000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    'public_key': ['0x9fe62e51380bdfc', '0x176df573b3bf5d69', '0xdac18e0d0724b33',
                   '0x1dbba6f8ff63183a'],
}


def get_prover_and_verifier_from_flavor(flavor):
    prover = f'build/{flavor}/{DIR}/{PROVER_EXE}'
    verifier = f'build/{flavor}/{DIR}/{VERIFIER_EXE}'
    if (os.path.isfile(prover) and
            os.path.isfile(verifier)):
        return prover, verifier
    return None, None


def hash_chain(chain_digest):
    i = 0
    while True:
        next_bytes = hashlib.blake2s(chain_digest + i.to_bytes(32, 'big')).digest()
        for j in range(4):
            yield int(next_bytes[j*8: j*8 + 8].hex(), 16)
        i += 1


def deserialize_montgomery_representation(candidate, prime):
    # normalization_factor = 2^-64 (mod prime)
    normalization_factor = pow((prime + 1) // 2, 64, prime)
    return (candidate * normalization_factor) % prime


def generate_key_pair(prime, private_key=None):
    if(private_key is None):
        private_key = bytearray(random.getrandbits(8) for i in range(32))
    chain_digest = hashlib.blake2s(b'Ziggy secret preimage seed\x00' + private_key).digest()
    secret_preimage = []
    for randint in hash_chain(chain_digest):
        candidate = randint & ((1 << len(bin(prime)[2:])) - 1)
        if(candidate < prime):
            secret_preimage.append(deserialize_montgomery_representation(candidate, prime))
        if len(secret_preimage) == 8:
            break
    public_key = rescue_hash(secret_preimage)
    return {
        'private_key': '0x' + private_key.hex(),
        'public_key': [hex(i) for i in public_key],
    }


class ZiggyModifyProverOutput(ModifyProverOutput):
    def __init__(self, proof_modifier, claim_modification):
        self.proof_modifier = proof_modifier
        self.claim_modification = claim_modification

    def modify(self, prover_output):
        prover_output['proof_hex'] = self.proof_modifier(prover_output['proof_hex'])
        prover_output['public_input'] = self.__claim_modifier(prover_output['public_input'])

    def __claim_modifier(self, public_input):
        if self.claim_modification == 'message':
            self.__flip_bit_in_message(public_input)
        if self.claim_modification == 'public_key':
            self.__flip_bit_in_public_key(public_input, random.randint(0, 3))
        return public_input

    def __flip_bit_in_message(self, public_input_json):
        # Change one bit of message.
        message = public_input_json['message']
        idx = random.randint(0, len(message) - 1)
        mask = 1 << random.randint(0, 7)
        message = message[:idx] + chr(ord(message[idx]) ^ mask) + message[idx + 1:]
        public_input_json['message'] = message

    def __flip_bit_in_public_key(self, public_input_json, word_idx):
        # Corrupt one bit of public_key.
        value = int(public_input_json['public_key'][word_idx], 16)
        value ^= 1 << random.randint(0, 60)
        public_input_json['public_key'][word_idx] = hex(value)


def create_prover_input_files(queries, private_key, public_key, parameter_file,
                              private_input_file, public_input_file, prover_config_file):
    original_trace_length = 16
    slackness_factor = 2**ceil(log2((18 + queries) / 16.0))
    json.dump({
        'stark': {
            'log_n_cosets': 4,
            'enable_zero_knowledge': True,
            'fri': {
                'last_layer_degree_bound': 1,
                'n_queries': queries,
                'fri_step_list': [0] + [1] * ceil(log2(slackness_factor * original_trace_length)),
                'proof_of_work_bits': 15
            }
        }
    }, parameter_file)
    json.dump({
        'message': 'test message',
        'public_key': public_key
    }, public_input_file)
    json.dump({
        'private_key': private_key
    }, private_input_file)
    json.dump({
        'constraint_polynomial_task_size': 256
    }, prover_config_file)

    parameter_file.flush()
    public_input_file.flush()
    private_input_file.flush()
    prover_config_file.flush()


class Params:
    def __init__(self, expect_success=True, keys=None,
                 proof_modification=ModifyProverOutput.no_change,
                 claim_modification='no_change'):
        """
        Setting keys to None (default) generates random private and public keys.
        """
        self.expect_success = expect_success
        self.keys = keys
        self.proof_modification = proof_modification
        self.claim_modification = claim_modification


@pytest.mark.parametrize(
    'params', [
        Params(),
        Params(keys=precomputed_keys),
        Params(expect_success=False, proof_modification=ModifyProverOutput.remove_last_byte),
        Params(expect_success=False, proof_modification=ModifyProverOutput.empty_proof),
        Params(expect_success=False, proof_modification=ModifyProverOutput.remove_random_byte),
        Params(expect_success=False, proof_modification=ModifyProverOutput.modify_random_bit),
        Params(expect_success=False, claim_modification='message'),
        Params(expect_success=False, claim_modification='public_key'),
    ])
def test_ziggy(params, flavor):
    if params.keys is not None:
        keys = params.keys
    else:
        keys = generate_key_pair(PRIME)

    private_key = keys['private_key']
    public_key = keys['public_key']

    queries = random.randint(20, MAX_NUMBER_QUERIES)

    prover_proof_file = tempfile.NamedTemporaryFile()
    modified_proof_file = tempfile.NamedTemporaryFile()
    verifier_annotation_file = tempfile.NamedTemporaryFile()
    parameter_file = tempfile.NamedTemporaryFile(mode='w+')
    public_input_file = tempfile.NamedTemporaryFile(mode='w+')
    private_input_file = tempfile.NamedTemporaryFile(mode='w+')
    prover_config_file = tempfile.NamedTemporaryFile(mode='w+')

    create_prover_input_files(queries, private_key, public_key, parameter_file, private_input_file,
                              public_input_file, prover_config_file)

    prover, verifier = get_prover_and_verifier_from_flavor(flavor)

    subprocess.run([prover,
                    f'--parameter_file={parameter_file.name}',
                    f'--public_input_file={public_input_file.name}',
                    f'--private_input_file={private_input_file.name}',
                    f'--out_file={prover_proof_file.name}',
                    '--generate_annotations',
                    f'--prover_config_file={prover_config_file.name}',
                    '--logtostderr'])

    modifier = ZiggyModifyProverOutput(params.proof_modification, params.claim_modification)
    modifier.modify_and_save_as(prover_proof_file.name, modified_proof_file.name)

    verifier_process = subprocess.run([verifier,
                                       f'--in_file={modified_proof_file.name}',
                                       f'--annotation_file={verifier_annotation_file.name}',
                                       '--logtostderr'],
                                      stderr=subprocess.PIPE)
    sys.stderr.write(verifier_process.stderr.decode('ascii'))

    if params.expect_success:
        assert verifier_process.returncode == 0
        assert 'Ziggy signature verified successfully.' in verifier_process.stderr.decode('ascii')
        assert compare_prover_and_verifier_annotations(prover_proof_file.name,
                                                       verifier_annotation_file.name)
    else:
        assert verifier_process.returncode != 0
        assert 'Invalid Ziggy signature.' in verifier_process.stderr.decode('ascii')
