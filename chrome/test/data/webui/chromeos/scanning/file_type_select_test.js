// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/file_type_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {changeSelect} from './scanning_app_test_utils.js';

const FileType = {
  JPG: chromeos.scanning.mojom.FileType.kJpg,
  PDF: chromeos.scanning.mojom.FileType.kPdf,
  PNG: chromeos.scanning.mojom.FileType.kPng,
};

export function fileTypeSelectTest() {
  /** @type {?FileTypeSelectElement} */
  let fileTypeSelect = null;

  setup(() => {
    fileTypeSelect = /** @type {!FileTypeSelectElement} */ (
        document.createElement('file-type-select'));
    assertTrue(!!fileTypeSelect);
    document.body.appendChild(fileTypeSelect);
  });

  teardown(() => {
    fileTypeSelect.remove();
    fileTypeSelect = null;
  });

  test('initializeFileTypeSelect', () => {
    // The dropdown should be initialized as enabled with three options. The
    // default option should be PDF.
    const select =
        /** @type {!HTMLSelectElement} */ (fileTypeSelect.$$('select'));
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(3, select.length);
    assertEquals('JPG', select.options[0].textContent.trim());
    assertEquals('PNG', select.options[1].textContent.trim());
    assertEquals('PDF', select.options[2].textContent.trim());
    assertEquals(FileType.PDF.toString(), select.value);

    // Selecting a different option should update the selected value.
    return changeSelect(
               select, FileType.JPG.toString(), /* selectedIndex */ null)
        .then(() => {
          assertEquals(
              FileType.JPG.toString(), fileTypeSelect.selectedFileType);
        });
  });
}
