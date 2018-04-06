// Agency output of .[0].arango.Plan.Collections
std::shared_ptr<VPackBuffer<uint8_t>> planCollections = R"=(
{
  "someDb": {
    "11111111": {
      "name": "prototype",
      "replicationFactor": 2,
      "shards": {
        "s11": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ],
        "s1": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ],
        "s20": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ],
        "s346": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ],
        "s2": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ],
        "s35": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ]
      }
    },
    "22222222": {
      "name": "follower",
      "replicationFactor": 2,
      "distributeShardsLike": "11111111",
      "shards": {
        "s6": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ],
        "s3": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ],
        "s2": [
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ],
        "s5": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"
        ],
        "s4": [
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"
        ],
        "s1": [
          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"
        ]
      }
    }
  }
}
)="_vpack;

// Agency output of .[0].arango.Supervision.Health
// Coordinators are unused in the test, but must be ignored
std::shared_ptr<VPackBuffer<uint8_t>> supervisionHealth3Healthy0Bad = R"=(
{
  "CRDN-976e3d6a-9148-4ece-99e9-326dc69834b2": {
  },
  "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA": {
    "Status": "GOOD"
  },
  "CRDN-94ea8912-ff22-43d0-a005-bfc87f22709b": {
  },
  "CRDN-34b46cab-6f06-40a8-ac24-5eec1cf78f67": {
  },
  "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB": {
    "Status": "GOOD"
  },
  "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC": {
    "Status": "GOOD"
  }
}
)="_vpack;

// The correct proto<-shard mapping (and shard order) is:
// proto   shard
// "s1",   "s1"
// "s2",   "s2"
// "s11",  "s3"
// "s20",  "s4"
// "s35",  "s5"
// "s346", "s6"

std::map<CollectionID, ResultT<std::vector<RepairOperation>>>
    expectedResultsWithMultipleShards{
        {"22222222",
         {{
             BeginRepairsOperation{
                 "someDb", "22222222", "follower", "11111111", "prototype", 2,
                 2, true,
             },
             // "s1",   "s1"
             MoveShardOperation{
                 "someDb", "22222222", "follower", "s1",
                 "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA", true,
             },
             // "s2",  "s2"
             MoveShardOperation{
                 "someDb", "22222222", "follower", "s2",
                 "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                 "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC", false,
             },
             // "s11",   "s3"
             MoveShardOperation{
                 "someDb", "22222222", "follower", "s3",
                 "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                 "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB", true,
             },
             // "s20",  "s4"
             MoveShardOperation{
                 "someDb", "22222222", "follower", "s4",
                 "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                 "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA", false,
             },
             // "s35",  "s5"
             MoveShardOperation{
                 "someDb", "22222222", "follower", "s5",
                 "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                 "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC", true,
             },
             // "s346", "s6"
             MoveShardOperation{
                 "someDb", "22222222", "follower", "s6",
                 "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                 "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB", false,
             },

             FinishRepairsOperation{
                 "someDb",
                 "22222222",
                 "follower",
                 "11111111",
                 "prototype",
                 {
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s1", "s1",
                         {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"}),
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s2", "s2",
                         {"PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA",
                          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"}),
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s3", "s11",
                         {"PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                          "PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC"}),
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s4", "s20",
                         {"PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB",
                          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"}),
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s5", "s35",
                         {"PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                          "PRMR-AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA"}),
                     std::make_tuple<ShardID, ShardID, DBServers>(
                         "s6", "s346",
                         {"PRMR-CCCCCCCC-CCCC-CCCC-CCCC-CCCCCCCCCCCC",
                          "PRMR-BBBBBBBB-BBBB-BBBB-BBBB-BBBBBBBBBBBB"}),
                 },
                 2,
             },
         }}},
    };
