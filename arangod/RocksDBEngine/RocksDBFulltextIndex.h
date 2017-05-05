////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_FULLTEXT_INDEX_H
#define ARANGOD_ROCKSDB_ENGINE_FULLTEXT_INDEX_H 1

#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
struct DocumentIdentifierToken;

class RocksDBFulltextIndex final : public RocksDBIndex {
 public:
  RocksDBFulltextIndex() = delete;

  RocksDBFulltextIndex(TRI_idx_iid_t, LogicalCollection*,
                       arangodb::velocypack::Slice const&);

  ~RocksDBFulltextIndex();

 public:
  IndexType type() const override { return Index::TRI_IDX_TYPE_FULLTEXT_INDEX; }

  char const* typeName() const override { return "fulltext-rocksdb"; }

  bool allowExpansion() const override { return false; }

  bool canBeDropped() const override { return true; }

  bool isSorted() const override { return true; }

  bool hasSelectivityEstimate() const override { return false; }

  size_t memory() const override;

  void toVelocyPack(VPackBuilder&, bool, bool) const override;
  // Uses default toVelocyPackFigures

  bool matchesDefinition(VPackSlice const&) const override;

  int insert(transaction::Methods*, TRI_voc_rid_t,
             arangodb::velocypack::Slice const&, bool isRollback) override;

  int remove(transaction::Methods*, TRI_voc_rid_t,
             arangodb::velocypack::Slice const&, bool isRollback) override;

  int cleanup() override;

  bool isSame(std::string const& field, int minWordLength) const {
    std::string fieldString;
    TRI_AttributeNamesToString(fields()[0], fieldString);
    return (_minWordLength == minWordLength && fieldString == field);
  }
  
  /// insert index elements into the specified write batch. Should be used
  /// as an optimization for the non transactional fillIndex method
  int insertRaw(rocksdb::WriteBatchWithIndex*, TRI_voc_rid_t,
                        arangodb::velocypack::Slice const&) override;
  
  /// remove index elements and put it in the specified write batch. Should be used
  /// as an optimization for the non transactional fillIndex method
  int removeRaw(rocksdb::WriteBatch*, TRI_voc_rid_t,
                        arangodb::velocypack::Slice const&) override;
  
//  TRI_fts_index_t* internals() { return _fulltextIndex; }

  static TRI_voc_rid_t fromDocumentIdentifierToken(
      DocumentIdentifierToken const& token);
  static DocumentIdentifierToken toDocumentIdentifierToken(
      TRI_voc_rid_t revisionId);

 private:
  std::vector<std::string> wordlist(arangodb::velocypack::Slice const&);

  /// @brief the indexed attribute (path)
  std::vector<std::string> _attr;

  /// @brief minimum word length
  int _minWordLength;
};
}

#endif
