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

#include "AttributeScorer.h"

#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"

// ----------------------------------------------------------------------------
// --SECTION--                                        OrderFactory dependencies
// ----------------------------------------------------------------------------

NS_LOCAL

bool fromFCall(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    irs::string_ref const& name,
    arangodb::aql::AstNode const& args,
    bool reverse,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_ARRAY == args.type);
  irs::sort::ptr scorer;

  switch (args.numMembers()) {
    case 0:
      scorer = irs::scorers::get(name, irs::string_ref::nil);

      if (!scorer) {
        scorer = irs::scorers::get(name, "{}"); // pass arg as json
      }

      break;
    case 1: {
      auto* arg = args.getMemberUnchecked(0);

      if (arg
          && arangodb::aql::NODE_TYPE_VALUE == arg->type
          && arangodb::aql::VALUE_TYPE_STRING == arg->value.type) {
        irs::string_ref value(arg->getStringValue(), arg->getStringLength());

        scorer = irs::scorers::get(name, value);

        if (scorer) {
          break;
        }
      }
    }
    // fall through
    default:
      for (size_t i = 0, count = args.numMembers(); i < count; ++i) {
        auto* arg = args.getMemberUnchecked(i);

        if (!arg) {
          return false; // invalid arg
        }

        arangodb::velocypack::Builder builder;

        builder.openObject();
        arg->toVelocyPackValue(builder);
        builder.close();
        scorer = irs::scorers::get(name, builder.toJson()); // pass arg as json
      }
  }

  if (!scorer) {
    return false; // failed find scorer
  }

  if (ctx) {
    ctx->order.add(scorer);
    scorer->reverse(reverse);
  }

  return true;
}

bool fromFCall(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    arangodb::aql::AstNode const& node,
    bool reverse,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);
  auto* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn || 1 != node.numMembers()) {
    return false; // no function
  }

  auto* args = node.getMemberUnchecked(0);

  if (!args || arangodb::aql::NODE_TYPE_ARRAY != args->type) {
    return false; // invalid args
  }

  auto& name = fn->externalName;
  std::string scorerName(name);

  // convert name to lower case
  std::transform(scorerName.begin(), scorerName.end(), scorerName.begin(), ::tolower);

  return fromFCall(ctx, scorerName, *args, reverse, meta);
}

bool fromFCallUser(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    arangodb::aql::AstNode const& node,
    bool reverse,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL_USER == node.type);

  if (arangodb::aql::VALUE_TYPE_STRING != node.value.type
      || 1 != node.numMembers()) {
    return false; // no function name
  }

  irs::string_ref name(node.getStringValue(), node.getStringLength());
  auto* args = node.getMemberUnchecked(0);

  if (!args || arangodb::aql::NODE_TYPE_ARRAY != args->type) {
    return false; // invalid args
  }

  return fromFCall(ctx, name, *args, reverse, meta);
}

bool fromValue(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    arangodb::aql::AstNode const& node,
    bool reverse,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS == node.type
    || arangodb::aql::NODE_TYPE_VALUE == node.type
  );

  if (node.value.type != arangodb::aql::VALUE_TYPE_STRING) {
    return false; // unsupported value
  }

  if (ctx) {
    irs::string_ref name(node.getStringValue(), node.getStringLength());
    auto& scorer = ctx->order.add<arangodb::iresearch::AttributeScorer>(ctx->trx, name);
    scorer.reverse(reverse);

    // ArangoDB default type sort order:
    // null < bool < number < string < array/list < object/document
    scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::NIL);
    scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::BOOLEAN);
    scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::NUMBER);
    scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::STRING);
    scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::ARRAY);
    scorer.orderNext(arangodb::iresearch::AttributeScorer::ValueType::OBJECT);
  }

  return true;
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

// ----------------------------------------------------------------------------
// --SECTION--                                      OrderFactory implementation
// ----------------------------------------------------------------------------

/*static*/ bool OrderFactory::order(
  arangodb::iresearch::OrderFactory::OrderContext* ctx,
  arangodb::aql::SortCondition const& node,
  IResearchViewMeta const& meta
) {
  for (size_t i = 0, count = node.numAttributes(); i < count; ++i) {
    auto field = node.field(i);
    auto* variable = std::get<0>(field);
    auto* expression = std::get<1>(field);
    auto ascending = std::get<2>(field);
    UNUSED(variable);

    if (!expression) {
      return false;
    }

    bool result;

    switch (expression->type) {
      case arangodb::aql::NODE_TYPE_FCALL: // function call
        result = fromFCall(ctx, *expression, !ascending, meta);
        break;
      case arangodb::aql::NODE_TYPE_FCALL_USER: // user function call
        result = fromFCallUser(ctx, *expression, !ascending, meta);
        break;
      case arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS:
      case arangodb::aql::NODE_TYPE_VALUE:
        result = fromValue(ctx, *expression, !ascending, meta);
        break;
      default:
        result = false;
    }

    if (!result) {
      return false;
    }
  }

  return true;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------