#!/usr/bin/env python3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import sys
import argparse
import json
import xml.etree.ElementTree

_SCRIPT_DIR = os.path.realpath(os.path.dirname(__file__))
_CHROME_SOURCE = os.path.realpath(
    os.path.join(_SCRIPT_DIR, *[os.path.pardir] * 6))

sys.path.append(os.path.join(_CHROME_SOURCE, 'build/android/gyp'))

from util import build_utils

_chr = unichr if sys.version_info.major == 2 else chr


def parse_emoji_annotations(keyword_file):
    names = {}
    keywords = {}

    tree = xml.etree.ElementTree.parse(keyword_file)
    root = tree.getroot()

    for tag in root.iterfind('./annotations/annotation'):
        cp = tag.attrib['cp']
        # The Fuse search library in ChromeOS doesn't support prefix matching.
        # A workaround is appending a space before all name and keyword labels.
        # This allows us to force a prefix matching by prepending a space on
        # users' searches. E.g. for the Emoji "smile face", we store " smile
        # face", if the user searches for "fa", the search will be " fa" and
        # will match " smile face", but not "  infant".
        if tag.attrib.get('type') == 'tts':
            if tag.text.startswith("flag"):
              names[cp] = ' ' + tag.text.replace("flag:","flag of")
            else:
              names[cp] = ' ' + tag.text
        else:
            keywords[cp] = map(lambda k: ' ' + k, tag.text.split(' | '))

    return names, keywords


def parse_emoji_metadata(metadata_file):
    with open(metadata_file, 'r') as file:
        return json.load(file)


def transform_emoji_data(metadata, names, keywords):
    def transform(codepoints):
        # transform array of codepoint values into unicode string.
        string = u''.join(_chr(x) for x in codepoints)

        # keyword data has U+FE0F emoji presentation characters removed.
        if string not in names:
            string = string.replace(u'\ufe0f', u'')

        name = names[string]
        keyword_list = keywords[string]

        return {'string': string, 'name': name, 'keywords': keyword_list}

    for group in metadata:
        for emoji in group['emoji']:
            emoji['base'] = transform(emoji['base'])
            emoji['alternates'] = [transform(e) for e in emoji['alternates']]


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--metadata',
                        required=True,
                        help='emoji metadata ordering file as JSON')
    parser.add_argument('--output',
                        required=True,
                        help='output JSON file path')
    parser.add_argument('--keywords',
                        required=True,
                        nargs='+',
                        help='emoji keyword files as list of XML files')

    options = parser.parse_args(args)

    metadata_file = options.metadata
    keyword_files = options.keywords
    output_file = options.output

    # iterate through keyword files and combine them
    names = {}
    keywords = {}
    for file in keyword_files:
        _names, _keywords = parse_emoji_annotations(file)
        names.update(_names)
        keywords.update(_keywords)

    # parse emoji ordering data
    metadata = parse_emoji_metadata(metadata_file)
    transform_emoji_data(metadata, names, keywords)

    # write output file atomically in utf-8 format.
    with build_utils.AtomicOutput(output_file) as tmp_file:
        tmp_file.write(
            json.dumps(metadata, separators=(',', ':'),
                       ensure_ascii=False).encode('utf-8'))


if __name__ == '__main__':
    main(sys.argv[1:])
