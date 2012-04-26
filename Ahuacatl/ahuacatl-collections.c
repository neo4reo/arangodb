////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, collection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Ahuacatl/ahuacatl-collections.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator function to sort collections by name
////////////////////////////////////////////////////////////////////////////////

static int CollectionNameComparator (const void* l, const void* r) {
  TRI_aql_collection_t* left = *((TRI_aql_collection_t**) l);
  TRI_aql_collection_t* right = *((TRI_aql_collection_t**) r);

  return strcmp(left->_name, right->_name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection container
////////////////////////////////////////////////////////////////////////////////
    
static TRI_aql_collection_t* CreateCollectionContainer (const char* const name) {
  TRI_aql_collection_t* collection;

  assert(name);

  collection = (TRI_aql_collection_t*) TRI_Allocate(sizeof(TRI_aql_collection_t));
  if (!collection) {
    return NULL;
  }

  collection->_name = (char*) name;
  collection->_collection = NULL;

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up the collection names vector and order it
////////////////////////////////////////////////////////////////////////////////

bool SetupCollections (TRI_aql_parse_context_t* const context) {
  size_t i;
  size_t n;
  bool result = true;

  // each collection used is contained once in the assoc. array
  // so we do not have to care about duplicate names here
  n = context->_collectionNames._nrAlloc;
  for (i = 0; i < n; ++i) {
    char* name = context->_collectionNames._table[i];
    TRI_aql_collection_t* collection;

    if (!name) {
      continue;
    }

    collection = CreateCollectionContainer(name);
    if (!collection) {
      result = false;
      TRI_SetErrorAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      break;
    }

    TRI_PushBackVectorPointer(&context->_collections, (void*) collection);
  }

  if (result && n > 0) {
    qsort(context->_collections._buffer, 
          context->_collections._length, 
          sizeof(void*), 
          &CollectionNameComparator);
  }

  // now collections contains the sorted list of collections

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief open all collections used
////////////////////////////////////////////////////////////////////////////////

bool OpenCollections (TRI_aql_parse_context_t* const context) {
  size_t i;
  size_t n;

  n = context->_collections._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_collection_t* collection = context->_collections._buffer[i];
    char* name;

    assert(collection);
    assert(collection->_name);
    assert(!collection->_collection);

    LOG_TRACE("locking collection %s", collection->_name);

    name = collection->_name;
    collection->_collection = TRI_UseCollectionByNameVocBase(context->_vocbase, name);
    if (!collection->_collection) {
      TRI_SetErrorAql(context, TRI_ERROR_QUERY_COLLECTION_NOT_FOUND, name);
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock all collections used
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCollectionsAql (TRI_aql_parse_context_t* const context) {
  size_t i;
  
  // unlock in reverse order
  i = context->_collections._length;
  while (i--) {
    TRI_aql_collection_t* collection = context->_collections._buffer[i];

    assert(collection);
    assert(collection->_name);

    if (!collection->_collection) {
      // collection not yet opened
      continue;
    }
  
    LOG_TRACE("unlocking collection %s", collection->_name);
    
    TRI_ReleaseCollectionVocBase(context->_vocbase, collection->_collection);

    collection->_collection = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lock all collections used
////////////////////////////////////////////////////////////////////////////////

bool TRI_LockCollectionsAql (TRI_aql_parse_context_t* const context) {
  if (!SetupCollections(context)) {
    return false;
  }

  if (!OpenCollections(context)) {
    TRI_UnlockCollectionsAql(context);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
