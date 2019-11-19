#!/bin/sh
BASEPATH=$(dirname "$0")
export PATH=$PATH:$BASEPATH/bin:$PATH;
export PYTHONPATH=$PYTHONPATH:$BASEPATH/lib/python

$SHELL
