#!/usr/bin/env python

import os, sys, shutil
import argparse

parser = argparse.ArgumentParser(description='Copy files')
parser.add_argument('-i','--input', nargs='+', 
                    help='<Required> Input files', required=True)

parser.add_argument('-o','--output', nargs='+', 
                    help='<Required> Output files', required=True)

args = parser.parse_args()

if len(args.input) != len(args.output):
    print("Count of input and output files must be equal!")
    exit(1)

for i in range(len(args.input)):
    shutil.copy(args.input[i], args.output[i])
