#!/bin/bash

set -euo pipefail

AUX_BPV7="$1"
ION="$2"
ION_BPV7="$ION/bpv7"

cp -pf "$AUX_BPV7/Makefile.am"    "$ION/"