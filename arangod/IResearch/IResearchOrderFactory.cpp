////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchOrderFactory.h"

#include "AqlHelper.h"
#include "AttributeScorer.h"
#include "IResearchAttributes.h"
#include "VelocyPackHelper.h"

#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"

// ----------------------------------------------------------------------------
// --SECTION--                                        OrderFactory dependencies
// ----------------------------------------------------------------------------

NS_LOCAL

bool validateFuncArgs(
    arangodb::aql::AstNode const* args,
    arangodb::aql::Variable const& ref
) {
  if (!args || arangodb::aql::NODE_TYPE_ARRAY != args->type) {
    return false;
  }

  size_t const size = args->numMembers();

  if (size < 1) {
    return false; // invalid args
  }

  // 1st argument has to be reference to `ref`
  auto const* arg0 = args->getMemberUnchecked(0);

  if (!arg0
      || arangodb::aql::NODE_TYPE_REFERENCE != arg0->type
      || reinterpret_cast<void const*>(&ref) != arg0->getData()) {
    return false;
  }

  for (size_t i = 1, size = args->numMembers(); i < size; ++i) {
    auto const* arg = args->getMemberUnchecked(i);

    if (!arg || !arg->isDeterministic()) {
      // we don't support non-deterministic arguments for scorers
      return false;
    }
  }

  return true;
}

bool makeScorer(
    irs::sort::ptr& scorer,
    irs::string_ref const& name,
    arangodb::aql::AstNode const& args,
    arangodb::iresearch::QueryContext const& ctx
) {
  TRI_ASSERT(!args.numMembers() || arangodb::iresearch::findReference(*args.getMember(0), *ctx.ref));

  switch (args.numMembers()) {
    case 0:
      break;
    case 1: {

      scorer = irs::scorers::get(name, irs::string_ref::nil);

      if (!scorer) {
        scorer = irs::scorers::get(name, "[]"); // pass arg as json array
      }
    } break;
    default: { // fall through
      arangodb::velocypack::Builder builder;
      arangodb::iresearch::ScopedAqlValue arg;

      builder.openArray();

      for (size_t i = 1, count = args.numMembers(); i < count; ++i) {
        auto const* argNode = args.getMemberUnchecked(i);

        if (!argNode) {
          return false; // invalid arg
        }

        arg.reset(*argNode);

        if (!arg.execute(ctx)) {
          // failed to execute value
          return false;
        }

        arg.toVelocyPack(builder);
      }

      builder.close();
      scorer = irs::scorers::get(name, builder.toJson()); // pass arg as json
    }
  }

  return bool(scorer);
}

bool fromFCall(
    irs::sort::ptr* scorer,
    irs::string_ref const& scorerName,
    arangodb::aql::AstNode const* args,
    arangodb::iresearch::QueryContext const& ctx
) {
  if (!validateFuncArgs(args, *ctx.ref)) {
    // invalid arguments
    return false;
  }

  if (!scorer) {
    // cheap shallow check
    return irs::scorers::exists(scorerName);
  }

  // we don't support non-constant arguments for scorers now, if it
  // will change ensure that proper `ExpressionContext` set in `ctx`

  return makeScorer(*scorer, scorerName, *args, ctx);
}

bool fromFCall(
    irs::sort::ptr* scorer,
    arangodb::aql::AstNode const& node,
    arangodb::iresearch::QueryContext const& ctx
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);
  auto* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn || 1 != node.numMembers()) {
    return false; // no function
  }

  auto& name = fn->name;
  std::string scorerName(name);

  // convert name to lower case
  std::transform(
    scorerName.begin(), scorerName.end(), scorerName.begin(), ::tolower
  );

  return fromFCall(scorer, scorerName, node.getMemberUnchecked(0), ctx);
}

bool fromFCallUser(
    irs::sort::ptr* scorer,
    arangodb::aql::AstNode const& node,
    arangodb::iresearch::QueryContext const& ctx
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL_USER == node.type);

  if (arangodb::aql::VALUE_TYPE_STRING != node.value.type
      || 1 != node.numMembers()) {
    return false; // no function name
  }

  irs::string_ref scorerName;

  if (!arangodb::iresearch::parseValue(scorerName, node)) {
    // failed to extract function name
    return false;
  }

  return fromFCall(scorer, scorerName, node.getMemberUnchecked(0), ctx);
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

// ----------------------------------------------------------------------------
// --SECTION--                                      OrderFactory implementation
// ----------------------------------------------------------------------------

/*static*/ bool OrderFactory::scorer(
    irs::sort::ptr* scorer,
    aql::AstNode const& node,
    QueryContext const& ctx
) {
  switch (node.type) {
    case arangodb::aql::NODE_TYPE_FCALL: // function call
      return fromFCall(scorer, node, ctx);
    case arangodb::aql::NODE_TYPE_FCALL_USER: // user function call
      return fromFCallUser(scorer, node, ctx);
    default:
      // IResearch does not support any
      // expressions except function calls
      return false;
  }
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
