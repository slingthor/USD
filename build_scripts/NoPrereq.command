#!/bin/sh

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}


BASEPATH=$(dirname "$(realpath "$0")")
export PATH=$BASEPATH/../lib/CMake.app/Contents/bin/:$PATH
export PATH=$BASEPATH/../lib/python:$PATH
export PYTHONPATH=$BASEPATH/../lib/python:$PYTHONPATH

$SHELL
