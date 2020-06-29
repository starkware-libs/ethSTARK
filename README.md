# ethSTARK

## Run with Docker

```bash
$ docker build -t eth_stark/eth_stark .
```

```bash
$ docker run -it --rm eth_stark/eth_stark:latest bash
```

After that you should be in the container

```
root@fb94beb33143:/app#
```

## Generate random Witness

```bash
$ PYTHONPATH=src example/random_private_input.py --chain_length=6
```

This generates random witness in the `example/rescue_private_input.json` and public outputs in `example/rescue_public_input.json`

## Generate a proof

```bash
$ ./build/Release/src/starkware/main/rescue/rescue_prover \
--parameter_file example/rescue_params.json \
--prover_config_file example/rescue_prover_config.json \
--public_input_file example/rescue_public_input.json \
--private_input_file example/rescue_private_input.json \
--out_file example/proof.json \
--logtostderr
I0629 17:04:59.215517    24 prover_main_helper.cc:80] Byte count: 11940
Hash count: 107
Commitment count: 4
Field element count: 924
Data count: 1
```

## Verify a proof

```bash
$ ./build/Release/src/starkware/main/rescue/rescue_verifier \
--in_file example/proof.json \
--logtostderr
I0629 17:12:49.709878    30 rescue_verifier_main.cc:58] Proof verified successfully.
```
