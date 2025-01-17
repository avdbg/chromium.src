#!/usr/bin/env python
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate SWA files app main.html from files app main.html"""

from __future__ import print_function

import fileinput
import optparse
import os
import shutil
import sys

_SWA = '<script type="module"'\
       ' src="chrome://file-manager/main.rollup.js"></script>'

def GenerateSwaMainHtml(source, target):
  """Copy source file to target, do SWA edits, then add BUILD time stamp."""

  # Copy source (main.html) file to the target (main.html) file.
  shutil.copyfile(source, target)

  # Edit the target file.
  for line in fileinput.input(target, inplace=True):
    # Add _SWA <script> tag after the <head> tag.
    if line.find('<head>') >= 0:
      print(line + '    ' + _SWA)
    # Add <meta> charset="utf-8" attribute.
    elif line.find('<meta ') >= 0:
      sys.stdout.write(line.replace('<meta ', '<meta charset="utf-8" '))
    # Remove files app foreground/js <script> tags: SWA app must load
    # them after the SWA app has initialized needed resources.
    elif line.find('<script src="foreground/js/') == -1:
      sys.stdout.write(line)

  # Create a BUILD time stamp for the target file.
  open(target + '.stamp', 'a').close()

def main(args):
  parser = optparse.OptionParser()

  parser.add_option('--source', help='Files app main.html source file.')
  parser.add_option('--target', help='Target SWA main.html for output.')

  options, _ = parser.parse_args(args)

  if options.source and options.target:
    target = os.path.join(os.getcwd(), options.target)
    GenerateSwaMainHtml(options.source, target)
    return

  raise SyntaxError('Usage: all arguments are required.')

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
