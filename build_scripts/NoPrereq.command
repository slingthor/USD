#!/bin/sh

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}


BASEPATH=$(dirname "$(realpath "$0")")
export PATH=$PATH:$BASEPATH/../lib/CMake.app/Contents/bin/:$PATH
export PATH=$PATH:$BASEPATH/../lib/python
export PYTHONPATH=$PYTHONPATH:$BASEPATH/../lib/python

$SHELL
