#!/bin/python
import os
import re
import sys
import argparse

# Instantiate the parser
parser = argparse.ArgumentParser(description='Print files in directory')

# Optional positional argument
parser.add_argument('--regex', type=str,
                    help='Use regular expression to chose files. The expression is applied to absolute path')

parser.add_argument('-d', '--dir', type=str,
                    help='Print files from the dir')

parser.add_argument('-r', '--recursive', action='store_true',
                    help='Print files recursively')
                        
args = parser.parse_args()

if args.dir:
    root_directory = args.dir
else:
    root_directory = os.getcwd()

if args.regex:
    regex = re.compile(args.regex)
else:
    regex = None

if args.recursive:
    recursive = True
else:
    recursive = False        

for root, dirs, files in os.walk(root_directory):
    for file in files:
        if regex:
            if regex.match(file):
                print(file)
        else:
            print(file)
    if not recursive:
        break
