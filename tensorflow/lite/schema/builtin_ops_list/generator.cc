/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/lite/schema/builtin_ops_list/generator.h"

#include <cctype>
#include <iostream>
#include <string>

#include "tensorflow/lite/schema/schema_generated.h"

namespace tflite {
namespace builtin_ops_list {

const char kFileHeader[] = R"(
/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// DO NOT EDIT MANUALLY: This file is automatically generated by
// `tensorflow/lite/schema/builtin_ops_list/generator.cc`.

)";

bool IsValidInputEnumName(const std::string& name) {
  const char* begin = name.c_str();
  const char* ch = begin;
  while (*ch != '\0') {
    // If it's not the first character, expect an underscore.
    if (ch != begin) {
      if (*ch != '_') {
        return false;
      }
      ++ch;
    }

    // Expecting a word with upper case letters or digits, like "CONV",
    // "CONV2D", "2D"...etc.
    bool empty = true;
    while (isupper(*ch) || isdigit(*ch)) {
      // It's not empty if at least one character is consumed.
      empty = false;
      ++ch;
    }
    if (empty) {
      return false;
    }
  }
  return true;
}

bool GenerateHeader(std::ostream& os) {
  auto enum_names = tflite::EnumNamesBuiltinOperator();

  os << kFileHeader;

  // Check if all the input enum names are valid.
  for (auto enum_value : EnumValuesBuiltinOperator()) {
    std::string enum_name = enum_names[enum_value];
    if (!IsValidInputEnumName(enum_name)) {
      std::cerr << "Invalid input enum name: " << enum_name << std::endl;
      return false;
    }
  }

  for (auto enum_value : EnumValuesBuiltinOperator()) {
    std::string enum_name = enum_names[enum_value];
    // Skip pseudo-opcodes that aren't real ops.
    if (enum_name == "CUSTOM" ||
        enum_name == "PLACEHOLDER_FOR_GREATER_OP_CODES" ||
        enum_name == "DELEGATE") {
      continue;
    }
    // Skip ops that aren't declared in builtin_op_kernels.h.
    if (enum_name == "CALL" || enum_name == "CONCAT_EMBEDDINGS") {
      continue;
    }
    os << "TFLITE_OP(Register_" << enum_name << ")\n";
  }
  return true;
}

}  // namespace builtin_ops_list
}  // namespace tflite
