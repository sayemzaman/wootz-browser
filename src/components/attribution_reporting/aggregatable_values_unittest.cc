// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/aggregatable_values.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/test/gmock_expected_support.h"
#include "base/test/values_test_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/test_utils.h"
#include "components/attribution_reporting/trigger_registration_error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {
namespace {

using ::attribution_reporting::mojom::TriggerRegistrationError;
using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Property;

TEST(AggregatableValuesTest, Parse) {
  EXPECT_THAT(AggregatableValues::FromJSON(nullptr), ValueIs(IsEmpty()));

  const struct {
    const char* description;
    const char* json;
    ::testing::Matcher<base::expected<std::vector<AggregatableValues>,
                                      TriggerRegistrationError>>
        matches;
  } kTestCases[] = {
      {
          "empty",
          R"json({})json",
          ValueIs(IsEmpty()),
      },
      {
          "empty_list",
          R"json([])json",
          ValueIs(IsEmpty()),
      },
      {
          "not_dictionary_or_list",
          R"json(0)json",
          ErrorIs(TriggerRegistrationError::kAggregatableValuesWrongType),
      },
      {
          "value_not_int",
          R"json({"a": true})json",
          ErrorIs(TriggerRegistrationError::kAggregatableValuesValueInvalid),
      },
      {
          "value_below_range",
          R"json({"a": 0})json",
          ErrorIs(TriggerRegistrationError::kAggregatableValuesValueInvalid),
      },
      {
          "value_above_range",
          R"json({"a": 65537})json",
          ErrorIs(TriggerRegistrationError::kAggregatableValuesValueInvalid),
      },
      {
          "valid",
          R"json({"a": 1, "b": 65536})json",
          ValueIs(ElementsAre(
              AllOf(Property(&AggregatableValues::values,
                             ElementsAre(Pair("a", 1), Pair("b", 65536))),
                    Property(&AggregatableValues::filters, FilterPair())))),
      },
      {
          "list_element_wrong_type",
          R"json([123])json",
          ErrorIs(TriggerRegistrationError::kAggregatableValuesWrongType),
      },
      {
          "list_values_field_missing",
          R"json([
                {
                  "a": 1,
                  "b": 65536,
                }
          ])json",
          ErrorIs(TriggerRegistrationError::
                      kAggregatableValuesListValuesFieldMissing),
      },
      {
          "list_values_invalid_value",
          R"json([
                {
                  "values": {
                    "a": 65537,
                  }
                }
          ])json",
          ErrorIs(
              TriggerRegistrationError::kAggregatableValuesListValueInvalid),
      },
      {
          "list_filters_field_wrong_type",
          R"json([
                {
                  "values": {"a": 1,"b": 65536},
                  "filters": 123,
                }
          ])json",
          ErrorIs(TriggerRegistrationError::kFiltersWrongType),
      },
      {
          "valid_list",
          R"json([
                {
                  "values": {"a": 1,"b": 65536},
                }
          ])json",
          ValueIs(ElementsAre(
              AllOf(Property(&AggregatableValues::values,
                             ElementsAre(Pair("a", 1), Pair("b", 65536))),
                    Property(&AggregatableValues::filters, FilterPair())))),
      },
      {
          "valid_list_with_filters",
          R"json([
                {
                  "values":{"a": 1, "b": 65536},
                  "filters": [{
                    "c": ["1"]
                  }],
                  "not_filters": [{
                    "d": ["2"]
                  }]
                }
          ])json",
          ValueIs(ElementsAre(AllOf(
              Property(&AggregatableValues::values,
                       ElementsAre(Pair("a", 1), Pair("b", 65536))),
              Property(
                  &AggregatableValues::filters,
                  FilterPair(
                      /*positive=*/{*FilterConfig::Create({{"c", {"1"}}})},
                      /*negative=*/{*FilterConfig::Create({{"d", {"2"}}})}))))),
      }};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.description);
    base::Value value = base::test::ParseJson(test_case.json);
    EXPECT_THAT(AggregatableValues::FromJSON(&value), test_case.matches);
  }
}

TEST(AggregatableValuesTest, Parse_KeyLength) {
  auto parse_dict_with_key_length = [](size_t length) {
    base::Value::Dict dict;
    dict.Set(std::string(length, 'a'), 1);
    base::Value value(std::move(dict));
    return AggregatableValues::FromJSON(&value);
  };

  for (size_t length = 0; length < 26; length++) {
    EXPECT_THAT(parse_dict_with_key_length(length), ValueIs(_));
  }

  EXPECT_THAT(parse_dict_with_key_length(26),
              ErrorIs(TriggerRegistrationError::kAggregatableValuesKeyTooLong));
}

TEST(AggregatableValuesTest, Parse_ListKeyLength) {
  auto parse_dict_with_key_length = [](size_t length) {
    base::Value::Dict values;
    values.Set(std::string(length, 'a'), 1);

    base::Value value(base::Value::List().Append(
        base::Value::Dict().Set(kValues, std::move(values))));
    return AggregatableValues::FromJSON(&value);
  };

  for (size_t length = 0; length < 26; length++) {
    EXPECT_THAT(parse_dict_with_key_length(length), ValueIs(_));
  }

  EXPECT_THAT(
      parse_dict_with_key_length(26),
      ErrorIs(TriggerRegistrationError::kAggregatableValuesListKeyTooLong));
}

TEST(AggregatableValuesTest, ToJson) {
  const struct {
    AggregatableValues input;
    const char* expected_json;
  } kTestCases[] = {
      {
          AggregatableValues(),
          R"json({"values": {}})json",
      },
      {
          *AggregatableValues::Create(/*values=*/{{"a", 1}, {"b", 2}},
                                      FilterPair()),
          R"json({"values":{"a": 1,"b": 2}})json",
      },
      {
          *AggregatableValues::Create(
              /*values=*/{{"a", 1}, {"b", 2}},
              FilterPair(/*positive=*/{*FilterConfig::Create({{"c", {}}})},
                         /*negative=*/{*FilterConfig::Create({{"d", {}}})})),
          R"json({
            "filters": [{"c": []}],
            "not_filters": [{"d": []}],
            "values":{"a": 1,"b": 2}
          })json",
      },
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_THAT(test_case.input.ToJson(),
                base::test::IsJson(test_case.expected_json));
  }
}

}  // namespace
}  // namespace attribution_reporting
