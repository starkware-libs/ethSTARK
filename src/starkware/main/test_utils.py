import json
import random
from abc import ABC, abstractmethod


class ModifyProverOutput(ABC):
    @abstractmethod
    def modify(self):
        pass

    def modify_and_save_as(self, from_file_name, to_file_name):
        """
        Modifies the prover output using a virtual method which is implemented in derived classes
        and saves it to a new file.
        """
        with open(from_file_name) as from_file:
            data = json.load(from_file)

        self.modify(data)

        with open(to_file_name, 'w') as to_file:
            json.dump(data, to_file, indent=4)

    @staticmethod
    def no_change(proof):
        return proof

    @staticmethod
    def remove_last_byte(proof):
        if len(proof) <= 4:  # If the proof contains a single byte, an empty proof must be returned.
            return '0x0'
        return proof[:-2]

    @staticmethod
    def empty_proof(proof):
        return '0x0'

    @staticmethod
    def remove_random_byte(proof):
        if len(proof) <= 4:  # If the proof contains a single byte, an empty proof must be returned.
            return '0x0'
        if len(proof) % 2 != 0:  # If the proof contains an odd number of nibbles, align bytes.
            proof = '0x0' + proof[2:]

        # The first 2 digits of the proof are '0x'. Each following pair of hex digits is a byte.
        byte_index = random.randint(1, len(proof)/2 - 1)
        return proof[:2*byte_index] + proof[2*byte_index + 2:]

    @staticmethod
    def modify_random_bit(proof):
        nibble_index = random.randint(2, len(proof) - 1)    # Select a random nibble.
        bit_mask = 1 << random.randint(0, 3)                # Select a random bit and create a mask.
        nibble = hex(int(proof[nibble_index], 16) ^ bit_mask)[2:]  # Flip the randomly selected bit.
        return proof[:nibble_index] + nibble + proof[nibble_index + 1:]


def compare_prover_and_verifier_annotations(prover_output_file, verifier_annotations_file):
    """
    Compares the annotations from a prover output file with the annotations from a text file.
    """
    with open(prover_output_file) as prover_file:
        data = json.load(prover_file)
    with open(verifier_annotations_file) as annotation_file:
        verifier_annotations_string = annotation_file.read()
    prover_annotations_string = '\n'.join(data['annotations']) + '\n'
    return prover_annotations_string == verifier_annotations_string
