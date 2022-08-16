#!/usr/bin/env python

import os, sys

# Usage write-file.py filename content

if len(sys.argv) != 3:
    print("Wrong number of arguments! Usage: write-file.py filename content")
    sys.exit(1)

filename = sys.argv[1]
content = sys.argv[2]

with open(filename, "w+") as text_file:
    text_file.write(content)

exit(0)