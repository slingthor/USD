//
// Copyright 2023 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#ifndef PXR_USD_SDF_STAGE_VARIABLE_EXPRESSION
#define PXR_USD_SDF_STAGE_VARIABLE_EXPRESSION

/// \file sdf/stageVariableExpression.h

#include "pxr/pxr.h"
#include "pxr/usd/sdf/api.h"

#include "pxr/base/vt/dictionary.h"
#include "pxr/base/vt/value.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace Sdf_StageVariableExpressionImpl {
    class Node;
}

/// \class SdfStageVariableExpression
///
/// Class responsible for parsing and evaluating stage variable expressions.
///
/// Stage variable expressions are written in a custom language and 
/// represented in scene description as a string surrounded by backticks (`).
/// These expressions may refer to "stage variables", which are key-value
/// pairs authored as layer metadata. For example, when evaluating an 
/// expression like:
///
/// \code
/// `"a_${NAME}_string"`
/// \endcode
///
/// The "${NAME}" portion of the string with the value of stage variable
/// "NAME".
///
/// Higher levels of the system (e.g., composition) are responsible for
/// examining fields that support stage variable expressions, evaluating them
/// with the appropriate stage variables (via this class) and consuming the
/// results.
///
/// See \ref Sdf_Page_StageVariableExpressions "Stage Variable Expressions"
/// or more information on the expression language and areas of the system
/// where expressions may be used.
class SdfStageVariableExpression
{
public:
    /// Construct using the expression \p expr. If the expression cannot be
    /// parsed, this object represents an invalid expression. Parsing errors
    /// will be accessible via GetErrors.
    SDF_API
    explicit SdfStageVariableExpression(const std::string& expr);

    /// Construct an object representing an invalid expression.
    SDF_API
    SdfStageVariableExpression();

    SDF_API
    ~SdfStageVariableExpression();

    /// Returns true if \p s is a stage variable expression, false otherwise.
    /// A stage variable expression is a string surrounded by backticks (`).
    ///
    /// A return value of true does not guarantee that \p s is a valid
    /// expression. This function is meant to be used as an initial check
    /// to determine if a string should be considered as an expression.
    SDF_API
    static bool IsExpression(const std::string& s);

    /// Returns true if \p value holds a type that is supported by stage
    /// variable expressions, false otherwise. If this function returns
    /// true, \p value may be authored into the "stageVariables" dictionary.
    SDF_API
    static bool IsValidStageVariableType(const VtValue& value);

    /// Returns true if this object represents a valid expression, false
    /// if it represents an invalid expression.
    ///
    /// A return value of true does not mean that evaluation of this
    /// expression is guaranteed to succeed. For example, an expression may
    /// refer to a stage variable whose value is an invalid expression.
    /// Errors like this can only be discovered by calling Evaluate.
    SDF_API
    explicit operator bool() const;

    /// Returns the expression string used to construct this object.
    SDF_API
    const std::string& GetString() const;

    /// Returns a list of errors encountered when parsing this expression.
    ///
    /// If the expression was parsed successfully, this list will be empty.
    /// However, additional errors may be encountered when evaluating the e
    /// expression.
    SDF_API
    const std::vector<std::string>& GetErrors() const;

    /// \class Result
    class Result
    {
    public:
        /// The result of evaluating the expression. This value may be
        /// empty if the expression yielded no value. It may also be empty
        /// if errors occurred during evaluation. In this case, the errors
        /// field will be populated with error messages.
        VtValue value;

        /// Errors encountered while evaluating the expression.
        std::vector<std::string> errors;

        /// Set of stage variables that were used while evaluating
        /// the expression. For example, for an expression like
        /// `"example_${VAR}_expression"`, this set will contain "VAR".
        ///
        /// This set will also contain stage variables from subexpressions.
        /// In the above example, if the value of "VAR" was another
        /// expression like `"sub_${SUBVAR}_expression"`, this set will
        /// contain both "VAR" and "SUBVAR".
        std::unordered_set<std::string> usedStageVariables;
    };

    /// Evaluates this expression using the stage variables in
    /// \p stageVariables and returns a Result object with the final
    /// value. If an error occurs during evaluation, the value field
    /// in the Result object will be an empty VtValue and error messages
    /// will be added to the errors field.
    ///
    /// If this object represents an invalid expression, calling this
    /// function will return a Result object with an empty value and the
    /// errors from GetErrors().
    ///
    /// If any values in \p stageVariables used by this expression
    /// are themselves expressions, they will be parsed and evaluated.
    /// If an error occurs while evaluating any of these subexpressions,
    /// evaluation of this expression fails and the encountered errors
    /// will be added in the Result's list of errors.
    SDF_API
    Result Evaluate(const VtDictionary& stageVariables) const;

    /// Evaluates this expression using the stage variables in
    /// \p stageVariables and returns a Result object with the final
    /// value.
    ///
    /// This is a convenience function that calls Evaluate and ensures that
    /// the value in the Result object is either an empty VtValue or is
    /// holding the specified ResultType. If this is not the case, the
    /// Result value will be set to an empty VtValue an error message
    /// indicating the unexpected type will be added to the Result's error
    /// list. Otherwise, the Result will be returned as-is.
    template <class ResultType>
    Result EvaluateTyped(const VtDictionary& stageVariables) const
    {
        Result r = Evaluate(stageVariables);
        if (!r.value.IsEmpty() && !r.value.IsHolding<ResultType>()) {
            r.errors.push_back(
                _FormatUnexpectedTypeError(r.value, VtValue(ResultType())));
            r.value = VtValue();
        }
        return r;
    }

private:
    SDF_API
    static std::string
    _FormatUnexpectedTypeError(const VtValue& got, const VtValue& expected);

    std::vector<std::string> _errors;
    std::shared_ptr<Sdf_StageVariableExpressionImpl::Node> _expression;
    std::string _expressionStr;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
