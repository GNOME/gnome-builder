#!/usr/bin/env python3

"""
For every line in the makefile ending in a backslash, locate the longest
line and then reformat all lines to match the proper tailing whitespace.
"""

import os
import sys

filenames = sys.argv[1:]

def count_prefix_tabs(line):
    count = 0
    for ch in line:
        if ch == '\t':
            count += 1
        else:
            break
    return count

def count_visual_chars(line):
    n_tabs = count_prefix_tabs(line)
    return n_tabs * 8 + len(line.strip())

for path in filenames:
    lines = None
    longest = 0

    # find the longest line, stripping the prefix \t (which will be
    # rewritten) and the trailing space before the \\.
    with open(path, 'r') as stream:
        lines = list(stream.readlines())
        for line in lines:
            if line.endswith('\\\n'):
                line = line[:-2].rstrip()
                length = count_visual_chars(line)
                longest = max(longest, length)

    # Now rewrite the file with the line modifications
    with open(path + '.tmp', 'w') as stream:
        for line in lines:
            if not line.endswith('\\\n'):
                stream.write(line)
                continue

            line = line[:-2].rstrip()
            short = count_visual_chars(line)

            line = line + (longest - short) * ' ' + ' \\\n'
            stream.write(line)

    os.rename(path + '.tmp', path)

