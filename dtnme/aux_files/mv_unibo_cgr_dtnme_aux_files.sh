#!/bin/bash

set -euo

AUX_DIR="$1"
DTNME="$2"
DTNME_ROUTING="$DTNME/servlib/routing"

cp -pf "$AUX_DIR/BundleRouter.cc"         "$DTNME_ROUTING/"
cp -pf "$AUX_DIR/UniboCGRBundleRouter.cc" "$DTNME_ROUTING/"
cp -pf "$AUX_DIR/UniboCGRBundleRouter.h"  "$DTNME_ROUTING/"