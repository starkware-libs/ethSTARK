#!/usr/bin/env python3

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

DIR = 'src/starkware/main/rescue'
PROVER_EXE = 'rescue_prover'
VERIFIER_EXE = 'rescue_verifier'
MAX_NUMBER_QUERIES = 30
AIR_HEIGHT = 32
HASHES_PER_INSTANCE = 3
WORD_SIZE = 4


precomputed_witness_output = {
    'witness': [['0x1', '0x2', '0x3', '0x4'], ['0x5', '0x6', '0x7', '0x8'],
                ['0x1', '0x2', '0x3', '0x4'], ['0x5', '0x6', '0x7', '0x8']],
    'output': ['0x68956e24301f0d6', '0x1db6f48d1e36f2b4', '0x15ba54b9652e4c33',
               '0x199d7c05484a2ff8']
}


def get_prover_and_verifier_from_flavor(flavor):
    prover = f'build/{flavor}/{DIR}/{PROVER_EXE}'
    verifier = f'build/{flavor}/{DIR}/{VERIFIER_EXE}'
    if (os.path.isfile(prover) and
            os.path.isfile(verifier)):
        return prover, verifier
    return None, None


def generate_random_hex_witness_and_output(prime, chain_length=None):
    if chain_length is None:
        chain_length = HASHES_PER_INSTANCE * random.randint(1, 32)
    witness = [[hex(random.randint(0, prime - 1)) for _ in range(WORD_SIZE)]
               for _ in range(chain_length + 1)]
    output = [int(i, 0) for i in witness[0]]
    for w in witness[1:]:
        output = rescue_hash(list(output) + [int(i, 0) for i in w])
    return {
        'witness': witness,
        'output': [hex(i) for i in output],
    }


class RescueModifyProverOutput(ModifyProverOutput):
    def __init__(self, proof_modifier, claim_modification):
        self.proof_modifier = proof_modifier
        self.claim_modification = claim_modification

    def modify(self, prover_output):
        prover_output['proof_hex'] = self.proof_modifier(prover_output['proof_hex'])
        prover_output['public_input'] = self.__claim_modifier(prover_output['public_input'])

    def __claim_modifier(self, public_input):
        if self.claim_modification == 'chain_length':
            self.__change_path_length(public_input)
        if self.claim_modification == 'output':
            self.__flip_bit_in_output(public_input, random.randint(0, 3))
        return public_input

    def __change_path_length(self, public_input_json):
        # Change path_length by +-3 while keeping it positive.
        chain_length = public_input_json['chain_length']
        chain_length += 3 - 6 * random.randint(0, 1)
        public_input_json['chain_length'] = chain_length if chain_length > 0 else 6

    def __flip_bit_in_output(self, public_input_json, word_idx):
        # Corrupt one bit of output.
        value = int(public_input_json['output'][word_idx], 0)
        value ^= 1 << random.randint(0, 60)
        public_input_json['output'][word_idx] = hex(value)


def create_prover_input_files(
    queries, witness, chain_length, output, is_zero_knowledge, parameter_file,
        private_input_file, public_input_file, prover_config_file):
    original_trace_length = 2**ceil(log2(chain_length * AIR_HEIGHT * 1.0 / HASHES_PER_INSTANCE))
    if(is_zero_knowledge):
        slackness_factor = 2**ceil(log2((original_trace_length + queries + 2.0)
                                        / original_trace_length))
    else:
        slackness_factor = 1
    json.dump({
        'stark': {
            'log_n_cosets': 4,
            'enable_zero_knowledge': is_zero_knowledge,
            'fri': {
                'last_layer_degree_bound': 1,
                'n_queries': queries,
                'fri_step_list': [0] + [1] * ceil(log2(slackness_factor * original_trace_length)),
                'proof_of_work_bits': 15
            }
        }
    }, parameter_file)
    json.dump({
        'output': output,
        'chain_length': chain_length
    }, public_input_file)
    json.dump({
        'witness': witness
    }, private_input_file)
    json.dump({
        'constraint_polynomial_task_size': 256
    }, prover_config_file)

    parameter_file.flush()
    public_input_file.flush()
    private_input_file.flush()
    prover_config_file.flush()


class Params:
    def __init__(self, expect_success=True, witness_and_output=None, is_zero_knowledge=False,
                 proof_modification=ModifyProverOutput.no_change,
                 claim_modification='no_change'):
        """"
        Setting witness_and_output to None (default) generates a random witness with a corresponding
        output.
        """
        self.expect_success = expect_success
        self.witness_and_output = witness_and_output
        self.is_zero_knowledge = is_zero_knowledge
        self.proof_modification = proof_modification
        self.claim_modification = claim_modification


@pytest.mark.parametrize(
    'params', [
        Params(),
        Params(is_zero_knowledge=True),
        Params(witness_and_output=precomputed_witness_output),
        Params(witness_and_output=precomputed_witness_output, is_zero_knowledge=True),
        Params(expect_success=False, proof_modification=ModifyProverOutput.remove_last_byte),
        Params(expect_success=False, proof_modification=ModifyProverOutput.empty_proof),
        Params(expect_success=False, proof_modification=ModifyProverOutput.remove_random_byte),
        Params(expect_success=False, proof_modification=ModifyProverOutput.modify_random_bit),
        Params(expect_success=False, claim_modification='chain_length'),
        Params(expect_success=False, claim_modification='output'),
    ])
def test_rescue(params, flavor):
    if params.witness_and_output:
        witness_and_output = params.witness_and_output
    else:
        witness_and_output = generate_random_hex_witness_and_output(PRIME)
    witness = witness_and_output['witness']
    chain_length = len(witness) - 1
    output = witness_and_output['output']

    queries = random.randint(1, MAX_NUMBER_QUERIES)

    prover_proof_file = tempfile.NamedTemporaryFile()
    modified_proof_file = tempfile.NamedTemporaryFile()
    verifier_annotation_file = tempfile.NamedTemporaryFile()
    parameter_file = tempfile.NamedTemporaryFile(mode='w+')
    public_input_file = tempfile.NamedTemporaryFile(mode='w+')
    private_input_file = tempfile.NamedTemporaryFile(mode='w+')
    prover_config_file = tempfile.NamedTemporaryFile(mode='w+')

    create_prover_input_files(
        queries, witness, chain_length, output, params.is_zero_knowledge, parameter_file,
        private_input_file, public_input_file, prover_config_file)

    prover, verifier = get_prover_and_verifier_from_flavor(flavor)

    subprocess.run([prover,
                    f'--parameter_file={parameter_file.name}',
                    f'--public_input_file={public_input_file.name}',
                    f'--private_input_file={private_input_file.name}',
                    f'--out_file={prover_proof_file.name}',
                    '--generate_annotations',
                    f'--prover_config_file={prover_config_file.name}',
                    '--logtostderr'])

    modifier = RescueModifyProverOutput(params.proof_modification, params.claim_modification)
    modifier.modify_and_save_as(prover_proof_file.name, modified_proof_file.name)

    verifier_process = subprocess.run([verifier,
                                       f'--in_file={modified_proof_file.name}',
                                       f'--annotation_file={verifier_annotation_file.name}',
                                       '--logtostderr'],
                                      stderr=subprocess.PIPE)
    sys.stderr.write(verifier_process.stderr.decode('ascii'))

    if params.expect_success:
        assert verifier_process.returncode == 0
        assert 'Proof verified successfully.' in verifier_process.stderr.decode('ascii')
        assert compare_prover_and_verifier_annotations(prover_proof_file.name,
                                                       verifier_annotation_file.name)
    else:
        assert verifier_process.returncode != 0
        assert 'Invalid proof.' in verifier_process.stderr.decode('ascii')
