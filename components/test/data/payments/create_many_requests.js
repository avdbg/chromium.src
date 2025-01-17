/*
 * Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const supportedInstruments = [{
  supportedMethods: 'secure-payment-confirmation',
  data: {
    'credentialIds': [new ArrayBuffer(4)],
    'fallbackUrl': 'localhost:8000',
    'networkData': new ArrayBuffer(4),
  },
}];

const details = {
  total: {label: 'Total', amount: {currency: 'USD', value: '55.00'}},
};

for (let i = 0; i < 0x400; i++) {
  new PaymentRequest(supportedInstruments, details);
}
