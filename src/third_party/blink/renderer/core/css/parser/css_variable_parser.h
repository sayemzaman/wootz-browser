// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_VARIABLE_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_VARIABLE_PARSER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

class CSSUnparsedDeclarationValue;
class CSSParserContext;
class CSSUnparsedDeclarationValue;
struct CSSTokenizedValue;

class CORE_EXPORT CSSVariableParser {
 public:
  static bool ContainsValidVariableReferences(CSSParserTokenRange,
                                              const ExecutionContext* context);

  static CSSValue* ParseDeclarationIncludingCSSWide(const CSSTokenizedValue&,
                                                    bool is_animation_tainted,
                                                    const CSSParserContext&);
  static CSSUnparsedDeclarationValue* ParseDeclarationValue(
      const CSSTokenizedValue&,
      bool is_animation_tainted,
      const CSSParserContext&);
  // Custom properties registered with universal syntax [1] are parsed with
  // this function.
  //
  // https://drafts.css-houdini.org/css-properties-values-api-1/#universal-syntax-definition
  static CSSUnparsedDeclarationValue* ParseUniversalSyntaxValue(
      CSSTokenizedValue,
      const CSSParserContext&,
      bool is_animation_tainted);

  static bool IsValidVariableName(const CSSParserToken&);
  static bool IsValidVariableName(StringView);

  // NOTE: We have to strip both leading and trailing whitespace (and comments)
  // from values as per spec, but we assume the tokenizer has already done the
  // leading ones for us; see comment on CSSPropertyParser::ParseValue().
  static StringView StripTrailingWhitespaceAndComments(StringView);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_VARIABLE_PARSER_H_
