// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/manifest_parse_util.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace json_schema_compiler {
namespace manifest_parse_util {

namespace {

// Alias for a pointer to a base::Value const function which converts the
// base::Value into type T. This is used by ParseHelper below.
template <typename T>
using ValueTypeConverter = T (base::Value::*)() const;

template <typename T, typename U>
bool ParseHelper(const base::DictionaryValue& dict,
                 base::StringPiece key,
                 base::Value::Type expected_type,
                 ValueTypeConverter<U> type_converter,
                 T* out,
                 base::string16* error,
                 std::vector<base::StringPiece>* error_path_reversed) {
  DCHECK(type_converter);
  DCHECK(out);

  const base::Value* value =
      FindKeyOfType(dict, key, expected_type, error, error_path_reversed);
  if (!value)
    return false;

  *out = (value->*type_converter)();
  return true;
}

}  // namespace

void PopulateInvalidEnumValueError(
    base::StringPiece key,
    const std::string& value,
    base::string16* error,
    std::vector<base::StringPiece>* error_path_reversed) {
  DCHECK(error);
  DCHECK(error->empty());
  DCHECK(error_path_reversed);
  DCHECK(error_path_reversed->empty());

  error_path_reversed->push_back(key);
  *error = base::ASCIIToUTF16(
      base::StringPrintf("Specified value '%s' is invalid.", value.c_str()));
}

void PopulateFinalError(base::string16* error,
                        std::vector<base::StringPiece>* error_path_reversed) {
  DCHECK(error);
  DCHECK(error_path_reversed);
  DCHECK(!error->empty());
  DCHECK(!error_path_reversed->empty());

  // Reverse the path to ensure the constituent keys are in the correct order.
  std::reverse(error_path_reversed->begin(), error_path_reversed->end());
  *error = base::ASCIIToUTF16(
      base::StringPrintf("Error at key '%s'. %s",
                         base::JoinString(*error_path_reversed, ".").c_str(),
                         base::UTF16ToASCII(*error).c_str()));
}

const base::Value* FindKeyOfType(
    const base::DictionaryValue& dict,
    base::StringPiece key,
    base::Value::Type expected_type,
    base::string16* error,
    std::vector<base::StringPiece>* error_path_reversed) {
  DCHECK(error);
  DCHECK(error->empty());
  DCHECK(error_path_reversed);
  DCHECK(error_path_reversed->empty());

  const base::Value* value = dict.FindKey(key);
  if (!value) {
    error_path_reversed->push_back(key);
    *error = base::ASCIIToUTF16("Manifest key is required.");
    return nullptr;
  }

  if (value->type() != expected_type) {
    error_path_reversed->push_back(key);
    *error = base::ASCIIToUTF16(
        base::StringPrintf("Type is invalid. Expected %s, found %s.",
                           base::Value::GetTypeName(expected_type),
                           base::Value::GetTypeName(value->type())));
    return nullptr;
  }

  return value;
}

bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         int* out,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed) {
  return ParseHelper(dict, key, base::Value::Type::INTEGER,
                     &base::Value::GetInt, out, error, error_path_reversed);
}

bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         bool* out,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed) {
  return ParseHelper(dict, key, base::Value::Type::BOOLEAN,
                     &base::Value::GetBool, out, error, error_path_reversed);
}

bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         double* out,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed) {
  return ParseHelper(dict, key, base::Value::Type::DOUBLE,
                     &base::Value::GetDouble, out, error, error_path_reversed);
}

bool ParseFromDictionary(const base::DictionaryValue& dict,
                         base::StringPiece key,
                         std::string* out,
                         base::string16* error,
                         std::vector<base::StringPiece>* error_path_reversed) {
  return ParseHelper(dict, key, base::Value::Type::STRING,
                     &base::Value::GetString, out, error, error_path_reversed);
}

}  // namespace manifest_parse_util
}  // namespace json_schema_compiler
