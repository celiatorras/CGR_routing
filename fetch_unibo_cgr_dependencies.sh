#!/bin/bash

## fetch_unibo_cgr_dependencies.sh
#
#  Utility script to fetch Unibo-CGR dependencies.
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

set -euo pipefail

function help_fun() {
	echo "Usage: $0 </path/to/Unibo-CGR/> " 1>&2
	echo "This script fetch Unibo-CGR's dependencies." 1>&2
}

if test $# -ne 1
then
	help_fun
	exit 1
fi

UNIBO_CGR_DIR="$1"

if ! test -d "$UNIBO_CGR_DIR"
then
	help_fun
	echo "$UNIBO_CGR_DIR is not a directory" >&2
	exit 1
fi

function url_encode() {
    echo "$@" \
    | sed \
        -e 's/%/%25/g' \
        -e 's/ /%20/g' \
        -e 's/!/%21/g' \
        -e 's/"/%22/g' \
        -e "s/'/%27/g" \
        -e 's/#/%23/g' \
        -e 's/(/%28/g' \
        -e 's/)/%29/g' \
        -e 's/+/%2b/g' \
        -e 's/,/%2c/g' \
        -e 's/-/%2d/g' \
        -e 's/:/%3a/g' \
        -e 's/;/%3b/g' \
        -e 's/?/%3f/g' \
        -e 's/@/%40/g' \
        -e 's/\$/%24/g' \
        -e 's/\&/%26/g' \
        -e 's/\*/%2a/g' \
        -e 's/\./%2e/g' \
        -e 's/\//%2f/g' \
        -e 's/\[/%5b/g' \
        -e 's/\\/%5c/g' \
        -e 's/\]/%5d/g' \
        -e 's/\^/%5e/g' \
        -e 's/_/%5f/g' \
        -e 's/`/%60/g' \
        -e 's/{/%7b/g' \
        -e 's/|/%7c/g' \
        -e 's/}/%7d/g' \
        -e 's/~/%7e/g'
}


function fetch_gitlab_file() {
    local PROJECT_ID="$1"
    local BRANCH="$2"
    local REMOTE_FILE=$(url_encode "$3")
    
    curl "https://gitlab.com/api/v4/projects/$PROJECT_ID/repository/files/$REMOTE_FILE/raw?ref=$BRANCH" --silent
}

function fetch_unibo_dtnme_file() {
    local REMOTE_FILE="$1"
    local PROJECT_ID="30169226"
    local BRANCH="developPersampieri"

    fetch_gitlab_file "$PROJECT_ID" "$BRANCH" "$REMOTE_FILE"
}

function fetch_unibo_dtnme_dependencies() {
    local UNIBO_CGR="$1"

    # fetch ContactPlanManager dependency
    local CPM_DIR="$UNIBO_CGR/dtnme/Unibo-DTNME-CPM"
    mkdir -p "$CPM_DIR"
    # download fetch_unibo_dtnme_cpm.sh from Unibo-DTNME-CPM GitLab repository
    fetch_unibo_dtnme_file "fetch_unibo_dtnme_cpm.sh" > "$CPM_DIR/fetch_unibo_dtnme_cpm.sh"
    # add execute permission
    chmod +x "$CPM_DIR/fetch_unibo_dtnme_cpm.sh"
    # launch fetch_unibo_dtnme_cpm.sh to download all other Unibo-DTNME-CPM files from GitLab
    "$CPM_DIR/fetch_unibo_dtnme_cpm.sh" "$CPM_DIR"
}

fetch_unibo_dtnme_dependencies "$UNIBO_CGR_DIR"
