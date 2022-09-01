#!/bin/bash

set -euo

EXT_BPV6="$1"
ION="$2"
ION_BPV6="$ION/bpv6"

rm -rf "$ION_BPV6/library/ext/cgrr"
cp -rpf "$EXT_BPV6/cgrr" "$ION_BPV6/library/ext/"
rm -rf "$ION_BPV6/library/ext/rgr"
cp -rpf "$EXT_BPV6/rgr" "$ION_BPV6/library/ext/"