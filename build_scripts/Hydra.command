#!/bin/sh

realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}


BASEPATH=$(dirname "$(realpath "$0")")
export PATH=$PATH:$BASEPATH/bin:$PATH;
export PYTHONPATH=$PYTHONPATH:$BASEPATH/lib/python
export PYTHONPATH=$PYTHONPATH:$BASEPATH/lib/python/PySide2

$SHELL
