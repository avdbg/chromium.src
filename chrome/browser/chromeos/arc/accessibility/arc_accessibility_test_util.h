// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TEST_UTIL_H_

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "components/arc/mojom/accessibility_helper.mojom.h"

namespace arc {

template <class PropType, class ValueType>
void SetProperty(
    base::Optional<base::flat_map<PropType, ValueType>>& properties,
    PropType prop,
    const ValueType& value) {
  if (!properties.has_value())
    properties = base::flat_map<PropType, ValueType>();

  properties->insert_or_assign(prop, value);
}

#define DEF_SET_PROP(data_type, prop_type, data_member_name, value_type) \
  inline void SetProperty(data_type* data, prop_type prop,               \
                          const value_type& value) {                     \
    SetProperty(data->data_member_name, prop, value);                    \
  }                                                                      \
  inline void SetProperty(data_type& data, prop_type prop,               \
                          const value_type& value) {                     \
    SetProperty(data.data_member_name, prop, value);                     \
  }

DEF_SET_PROP(mojom::AccessibilityEventData,
             mojom::AccessibilityEventIntProperty,
             int_properties,
             int32_t)
DEF_SET_PROP(mojom::AccessibilityEventData,
             mojom::AccessibilityEventIntListProperty,
             int_list_properties,
             std::vector<int32_t>)

DEF_SET_PROP(mojom::AccessibilityNodeInfoData,
             mojom::AccessibilityBooleanProperty,
             boolean_properties,
             bool)
DEF_SET_PROP(mojom::AccessibilityNodeInfoData,
             mojom::AccessibilityIntProperty,
             int_properties,
             int32_t)
DEF_SET_PROP(mojom::AccessibilityNodeInfoData,
             mojom::AccessibilityIntListProperty,
             int_list_properties,
             std::vector<int32_t>)
DEF_SET_PROP(mojom::AccessibilityNodeInfoData,
             mojom::AccessibilityStringProperty,
             string_properties,
             std::string)

DEF_SET_PROP(mojom::AccessibilityWindowInfoData,
             mojom::AccessibilityWindowBooleanProperty,
             boolean_properties,
             bool)
DEF_SET_PROP(mojom::AccessibilityWindowInfoData,
             mojom::AccessibilityWindowIntProperty,
             int_properties,
             int32_t)
DEF_SET_PROP(mojom::AccessibilityWindowInfoData,
             mojom::AccessibilityWindowIntListProperty,
             int_list_properties,
             std::vector<int32_t>)
DEF_SET_PROP(mojom::AccessibilityWindowInfoData,
             mojom::AccessibilityWindowStringProperty,
             string_properties,
             std::string)

#undef DEF_SET_PROP

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TEST_UTIL_H_
