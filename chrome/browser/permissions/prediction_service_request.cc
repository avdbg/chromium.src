// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service_request.h"

#include <utility>

#include "base/bind.h"
#include "components/permissions/prediction_service/prediction_service.h"

PredictionServiceRequest::PredictionServiceRequest(
    permissions::PredictionService* service,
    const permissions::PredictionRequestFeatures& entity,
    permissions::PredictionServiceBase::LookupResponseCallback callback)
    : callback_(std::move(callback)) {
  service->StartLookup(
      entity, base::NullCallback(),
      base::BindOnce(&PredictionServiceRequest::LookupReponseReceived,
                     weak_factory_.GetWeakPtr()));
}

PredictionServiceRequest::~PredictionServiceRequest() = default;

void PredictionServiceRequest::LookupReponseReceived(
    bool lookup_succesful,
    bool response_from_cache,
    std::unique_ptr<permissions::GeneratePredictionsResponse> response) {
  std::move(callback_).Run(lookup_succesful, response_from_cache,
                           std::move(response));
}
