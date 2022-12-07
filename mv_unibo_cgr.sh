#!/bin/bash

## mv_unibo_cgr.sh
#
#  Utility script to add Unibo-CGR into ION/DTNME.
#  Launch this script without arguments to get help.
#
#
#  Written by Lorenzo Persampieri:  lorenzo.persampieri@studio.unibo.it
#  Supervisor Prof. Carlo Caini:    carlo.caini@unibo.it
#
#  Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
#
#  This file is part of Unibo-CGR.
#
#  Unibo-CGR is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#  Unibo-CGR is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with Unibo-CGR.  If not, see <http://www.gnu.org/licenses/>.
#
##

#set -euxo pipefail
set -euo pipefail

function help_fun() {
	echo "Usage: $0 <ion | dtnme> </path/to/Unibo-CGR/> </path/to/ION/ | /path/to/DTNME/>" 1>&2
	echo "This script includes Unibo-CGR in either ION or DTNME." 1>&2
	echo "Launch this script with the three parameters explicited in the Usage string." 1>&2
}

if test $# -ne 3
then
	help_fun
	exit 1
fi

BP_IMPL_NAME="$1"
UNIBO_CGR_DIR="$2"
BP_IMPL_DIR="$3"

if test "$BP_IMPL_NAME" != "ion" -a "$BP_IMPL_NAME" != "dtnme"
then
	help_fun
	exit 1
fi

if ! test -d "$UNIBO_CGR_DIR"
then
	help_fun
	echo "$UNIBO_CGR_DIR is not a directory" >&2
	exit 1
fi

if ! test -d "$BP_IMPL_DIR"
then
	help_fun
	echo "$BP_IMPL_DIR is not a directory" >&2
	exit 1
fi

function update_config_file() {
	PATH_TO_CONFIG_FILE="$1"
	MACRO="$2"
	VALUE="$3"

#   change macro values inside #ifndef/#endif block (with regex)
	sed -i '/^[[:space:]]*#ifndef[[:space:]]\+'"$MACRO"'[[:space:]]*$/,/^[[:space:]]*#endif[[:space:]]*$/s/^[[:space:]]*#define[[:space:]]\+'"$MACRO"'[[:space:]]\+.*$/#define '"$MACRO $VALUE"'/' "$PATH_TO_CONFIG_FILE"
}

function mv_unibo_cgr_to_ion() {

	local UNIBO_CGR="$1"
	local ION="$2"
	local ION_BPV7="$ION/bpv7"
	local AUX_BPV7="$UNIBO_CGR/ion_bpv7/aux_files"
	local EXT_BPV7="$UNIBO_CGR/ion_bpv7/extensions"
	local CONFIG_FILE_BPV7="$ION_BPV7/cgr/Unibo-CGR/core/config.h"
	local ION_BPV6="$ION/bpv6"
	local AUX_BPV6="$UNIBO_CGR/ion_bpv6/aux_files"
	local EXT_BPV6="$UNIBO_CGR/ion_bpv6/extensions"
	local CONFIG_FILE_BPV6="$ION_BPV6/cgr/Unibo-CGR/core/config.h"

	echo ""
	echo "Moving Unibo-CGR into ION..."
	echo ""

	echo "Moving Unibo-CGR into bpv6..."
	rm -rf "$ION_BPV6/cgr/Unibo-CGR"
	cp -rpf "$UNIBO_CGR" "$ION_BPV6/cgr/Unibo-CGR"

	echo "Moving auxiliary files for bpv6..."
	"$AUX_BPV6/mv_unibo_cgr_ion_bpv6_aux_files.sh" "$AUX_BPV6" "$ION"
	echo "Moving extensions for bpv6..."
	"$EXT_BPV6/mv_unibo_cgr_ion_bpv6_extensions.sh" "$EXT_BPV6" "$ION"

	echo "Removing unnecessary files from Unibo-CGR for bpv6..."
	rm -rf "$ION_BPV6/cgr/Unibo-CGR/ion_bpv7"
	rm -rf "$ION_BPV6/cgr/Unibo-CGR/dtnme"
	rm -f "$ION_BPV6/cgr/Unibo-CGR/mv_unibo_cgr.sh"
	rm -f "$ION_BPV6/cgr/Unibo-CGR/.gitlab-ci.yml"
	rm -rf "$ION_BPV6/cgr/Unibo-CGR/docs/.doxygen"
#	rm -rf "$ION_BPV6/cgr/Unibo-CGR/ion_bpv6/aux_files"
#	rm -rf "$ION_BPV6/cgr/Unibo-CGR/ion_bpv6/extensions"

	echo "Moving Unibo-CGR into bpv7..."
	rm -rf "$ION_BPV7/cgr/Unibo-CGR"
	cp -rpf "$UNIBO_CGR" "$ION_BPV7/cgr/Unibo-CGR"

	echo "Moving auxiliary files for bpv7..."
  "$AUX_BPV7/mv_unibo_cgr_ion_bpv7_aux_files.sh" "$AUX_BPV7" "$ION"

	echo "Moving extensions for bpv7..."
	"$EXT_BPV7/mv_unibo_cgr_ion_bpv7_extensions.sh" "$EXT_BPV7" "$ION"

	echo "Removing unnecessary files from Unibo-CGR for bpv7..."
	rm -rf "$ION_BPV7/cgr/Unibo-CGR/ion_bpv6"
	rm -rf "$ION_BPV7/cgr/Unibo-CGR/dtnme"
	rm -f "$ION_BPV7/cgr/Unibo-CGR/mv_unibo_cgr.sh"
	rm -f "$ION_BPV7/cgr/Unibo-CGR/.gitlab-ci.yml"
	rm -rf "$ION_BPV7/cgr/Unibo-CGR/docs/.doxygen"
#	rm -rf "$ION_BPV7/cgr/Unibo-CGR/ion_bpv7/aux_files"
#	rm -rf "$ION_BPV7/cgr/Unibo-CGR/ion_bpv7/extensions"

	echo ""

	AUTORECONF=""
	LIBTOOLIZE=""

	if ! command -v autoreconf > /dev/null 2>&1
	then
		echo "autoreconf: not found. Please install autoconf package." 1>&2
		AUTORECONF="missing"
	fi

	if ! command -v libtoolize > /dev/null 2>&1
	then
		echo "libtoolize: not found. Please install libtool package." 1>&2
		LIBTOOLIZE="missing"
	fi

	if test "$AUTORECONF" != "missing" -a "$LIBTOOLIZE" != "missing"
	then
		echo "Updating the configure script..."
		cd "$ION" && autoreconf -fi && echo -e "\nNow you can compile and install in the usual way with configure/make/make install\n" \
		&& echo "configure sintax from ION's root directory:" \
		&& echo "./configure --enable-unibo-cgr CPPFLAGS='-DRGREB=1 -DCGRREB=1'" \
		&& echo "If you don't want RGR or CGRR just don't specify them in CPPFLAGS."
	else
		echo -e "\nPlease install missing packages and launch autoreconf -fi in $ION to update the configure script.\n" 1>&2
	fi
	
	echo ""
}

function update_dtnme_srcs() {
	local DTNME_MAKEFILE="$1"
	local TO_ADD="$2"
	local SRCS_NAME="$3"

#	check if we tweaked the makefile in a previous run
	if grep -q "$SRCS_NAME" "$DTNME_MAKEFILE" ; then
		return
	fi

	local TEMP_FILE=".temp_dtnme_makefile"
	touch "$TEMP_FILE"
	chown --reference="$DTNME_MAKEFILE" "$TEMP_FILE"

#	find first occurrence of "SERVLIB_SRCS" in Makefile
	local lineNum=$(grep -n 'SERVLIB_SRCS' "$DTNME_MAKEFILE" | cut -d":" -f 1 | head -n 1)

#	copy first part of Makefile
	head -n $(($lineNum-1)) "$DTNME_MAKEFILE" > "$TEMP_FILE"

#	insert section "$SRCS_NAME" with source filenames before SERVLIB_SRCS
	echo -e "$SRCS_NAME := \t\\" >> "$TEMP_FILE"
	while read srcLine; do
#	skip empty lines
	if test -z "$srcLine" ; then
		break
	fi
	echo -e "\t$srcLine\t\\"
	done < "$TO_ADD" >> "$TEMP_FILE"
	echo >> "$TEMP_FILE"

#	copy SERVLIB_SRCS line
	sed -n "$lineNum"'p' < "$DTNME_MAKEFILE" >> "$TEMP_FILE"

#	insert line "$(SRCS_NAME) \" in SERVLIB_SRCS
	echo -e "\t"'$('"$SRCS_NAME"')'"\t\t\t\t\\" >> "$TEMP_FILE"

#	copy remaining part of Makefile
	tail -n +$(($lineNum + 1)) "$DTNME_MAKEFILE" >> "$TEMP_FILE"

	mv "$TEMP_FILE" "$DTNME_MAKEFILE"
}


function mv_unibo_cgr_to_dtnme() {

	local UNIBO_CGR="$1"
	local DTNME="$2"
	local DTNME_ROUTING="$DTNME/servlib/routing"
	local CONFIG_FILE="$DTNME_ROUTING/Unibo-CGR/core/config.h"
	local AUX_DIR="$UNIBO_CGR/dtnme/aux_files"

	echo ""
	echo "Moving Unibo-CGR into DTNME..."
	rm -rf "$DTNME_ROUTING/Unibo-CGR"
	cp -rpf "$UNIBO_CGR" "$DTNME_ROUTING/Unibo-CGR"

	echo "Moving auxiliary files into DTNME..."
	"$AUX_DIR/mv_unibo_cgr_dtnme_aux_files.sh" "$AUX_DIR" "$DTNME"

	echo "------------------------------------"
	echo "dependency: ContactPlanManager"
	echo "Launch mv_unibo_dtnme_cpm.sh to include the Unibo-DTNME-CPM into DTNME"
	local CONTACT_PLAN_MANAGER_DIR="$UNIBO_CGR/dtnme/Unibo-DTNME-CPM"
	"$CONTACT_PLAN_MANAGER_DIR/mv_unibo_dtnme_cpm.sh" "$CONTACT_PLAN_MANAGER_DIR/" "$DTNME"
	echo "Unibo-DTNME-CPM has been included"
	echo "------------------------------------"

	echo "Update DTNME/servlib/Makefile..."
	update_dtnme_srcs "$DTNME/servlib/Makefile" "$UNIBO_CGR/dtnme/aux_files/SourceList.txt" "UNIBOCGR_SRCS"

	echo "Removing unnecessary files from Unibo-CGR for DTNME..."
	rm -rf "$DTNME_ROUTING/Unibo-CGR/ion_bpv6"
	rm -rf "$DTNME_ROUTING/Unibo-CGR/ion_bpv7"
	rm -f "$DTNME_ROUTING/Unibo-CGR/mv_unibo_cgr.sh"
	rm -f "$DTNME_ROUTING/Unibo-CGR/.gitlab-ci.yml"
	rm -rf "$DTNME_ROUTING/Unibo-CGR/docs/.doxygen"
#	rm -rf "$DTNME_ROUTING/Unibo-CGR/dtnme/aux_files"

	echo

}

if test "$BP_IMPL_NAME" = "ion"
then
	mv_unibo_cgr_to_ion "$UNIBO_CGR_DIR" "$BP_IMPL_DIR"
elif test "$BP_IMPL_NAME" = "dtnme"
then
	mv_unibo_cgr_to_dtnme "$UNIBO_CGR_DIR" "$BP_IMPL_DIR"
else
	help_fun
	exit 1
fi
