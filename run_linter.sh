#!/bin/bash

set -e

cpplint --extensions=cc,h $(find src/starkware | grep -E '\.(cc|h)$')
