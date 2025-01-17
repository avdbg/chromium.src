#!/bin/sh

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if ! command -v gen-bundle > /dev/null 2>&1; then
    echo "gen-bundle is not installed. Please run:"
    echo "  go get -u github.com/WICG/webpackage/go/bundle/cmd/..."
    echo '  export PATH=$PATH:$(go env GOPATH)/bin'
    exit 1
fi

gen-bundle \
  -version b1 \
  -baseURL https://test.example.org/ \
  -primaryURL https://test.example.org/ \
  -dir web_bundle_browsertest/ \
  -manifestURL https://test.example.org/manifest.webmanifest \
  -o web_bundle_browsertest.wbn

# Generate a base WBN which will used to generate broken WBN.
# This WBN must contains 3 entries:
#   [1]: https://test.example.org/
#   [2]: https://test.example.org/index.html
#   [3]: https://test.example.org/script.html
gen-bundle \
  -version b1 \
  -baseURL https://test.example.org/ \
  -primaryURL https://test.example.org/ \
  -dir broken_bundle/ \
  -o broken_bundle_base.wbn

# Rewrite ":status" (3a737461747573) header of the first entry to ":xxxxxx"
# (3a787878787878).
xxd -p broken_bundle_base.wbn |
  tr -d '\n' |
  sed 's/3a737461747573/3a787878787878/' |
  xxd -r -p > broken_bundle_broken_first_entry.wbn

# Rewrite ":status" (3a737461747573) header of the third entry (script.js) to
# ":xxxxxx" (3a787878787878).
xxd -p broken_bundle_base.wbn |
  tr -d '\n' |
  sed 's/3a737461747573/3a787878787878/3' |
  xxd -r -p > broken_bundle_broken_script_entry.wbn

gen-bundle \
  -version b1 \
  -primaryURL https://test.example.org/ \
  -har variants_test.har \
  -o variants_test.wbn

# Generate a WBN which will be used as a cross origin bundle.
gen-bundle \
  -version b1 \
  -har cross_origin.har \
  -primaryURL https://cross-origin.test/web_bundle/resource.json \
  -o cross_origin.wbn

# Generate a WBN which will be used as a same origin bundle.
gen-bundle \
  -version b1 \
  -har same_origin.har \
  -primaryURL https://same-origin.test/web_bundle/resource.json \
  -o same_origin.wbn

# Generate a WBN which includes urn:uuid resources.
gen-bundle \
  -version b1 \
  -har urn-uuid.har \
  -primaryURL urn:uuid:429fcc4e-0696-4bad-b099-ee9175f023ae \
  -o urn-uuid.wbn

# Update Content-Length header in urn-uuid.wbn.mock-http-headers.
(cat web_bundle_browsertest.wbn.mock-http-headers; \
 echo Content-Length: `wc -c <urn-uuid.wbn`) \
    > urn-uuid.wbn.mock-http-headers
