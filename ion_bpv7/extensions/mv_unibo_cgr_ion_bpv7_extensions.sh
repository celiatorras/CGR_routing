#!/bin/bash

set -euo pipefail

EXT_BPV7="$1"
ION="$2"
ION_BPV7="$ION/bpv7"

rm -rf "$ION_BPV7/library/ext/cgrr"
cp -rpf "$EXT_BPV7/cgrr" "$ION_BPV7/library/ext/"
rm -rf "$ION_BPV7/library/ext/rgr"
cp -rpf "$EXT_BPV7/rgr" "$ION_BPV7/library/ext/"