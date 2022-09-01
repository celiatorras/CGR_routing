#!/bin/bash

set -euo pipefail

AUX_BPV6="$1"
ION="$2"
ION_BPV6="$ION/bpv6"

cp -pf "$AUX_BPV6/bpextensions.c" "$ION_BPV6/library/ext/"
cp -pf "$AUX_BPV6/cgr.h"          "$ION_BPV6/library/"
cp -pf "$AUX_BPV6/cgrfetch.c"     "$ION_BPV6/utils/"
cp -pf "$AUX_BPV6/ipnfw.c"        "$ION_BPV6/ipn/"
cp -pf "$AUX_BPV6/libcgr.c"       "$ION_BPV6/cgr/"
#cp -pf "$AUX_BPV6/Makefile.am"    "$ION/"
cp -pf "$AUX_BPV6/configure.ac"   "$ION/"