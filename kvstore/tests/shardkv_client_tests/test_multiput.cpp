#include <string>

#include "client/shardkv_client.hpp"
#include "common/config.hpp"
#include "common/shard.hpp"
#include "server/server.hpp"
#include "test_utils/test_utils.hpp"

constexpr size_t N_SERVERS = 5;
static constexpr std::size_t kRandStringLength = 5;
static constexpr std::size_t kNumKeyValPairs = 20;
static constexpr std::size_t kNumBatches = 5;

int main() {
  std::string sm_addr = get_host_address("8080");
  std::shared_ptr<Shardcontroller> sm = start_shardcontroller(sm_addr);

  std::shared_ptr<ShardKvClient> client = make_shared<ShardKvClient>(sm_addr);

  std::vector<std::string> server_addresses = make_server_addresses(N_SERVERS);

  std::vector<Shard> shards = split_into(N_SERVERS);
  std::vector<std::shared_ptr<KvServer>> servers;

  // Start N_SERVERS - 1 & move shards to them
  for (std::size_t i = 1; i < N_SERVERS; i++) {
    std::shared_ptr<KvServer> ptr =
        start_server<KvServer, const std::string&, const std::string&,
                     uint64_t>(server_addresses[i], sm_addr, 5);
    servers.push_back(ptr);
    JoinRequest rq{server_addresses[i]};
    sm->Join(&rq, {});
    ASSERT(test_move(sm, server_addresses[i], std::vector<Shard>{shards[i]}));
  }

  // At this point, shards should look like this: [8, E] [F, L] [M, S] [T-Z]

  std::vector<std::vector<std::string>> keys;
  std::vector<std::vector<std::string>> vals;

  for (std::size_t i = 0; i < kNumBatches; i++) {
    // don't put 0-7 in keys so that no key starts with those letters -- no
    // server has shard 0-7
    keys.push_back(make_rand_strs(
        kNumKeyValPairs, kRandStringLength,
        "89ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"));
    vals.push_back(make_rand_strs(kNumKeyValPairs, kRandStringLength));
  }

  // Sleep to allow the config to update before issuing requests
  std::this_thread::sleep_for(500ms);

  const auto multiput_start{std::chrono::steady_clock::now()};
  for (std::size_t i = 0; i < kNumBatches; i++) {
    bool success = client->MultiPut(keys[i], vals[i]);
    ASSERT(success);
  }
  const auto multiput_end{std::chrono::steady_clock::now()};

  // Get key-value pairs from the servers
  // (Avoid using MultiGet so that we only test MultiPut functionality here)
  for (std::size_t i = 0; i < kNumBatches; i++) {
    for (std::size_t j = 0; j < kNumKeyValPairs; j++) {
      std::optional<std::string> return_val = client->Get(keys[i][j]);
      ASSERT(return_val);
      ASSERT_EQ(*return_val, vals[i][j]);
    }
  }

  const auto put_start{std::chrono::steady_clock::now()};
  for (std::size_t i = 0; i < kNumBatches; i++) {
    for (std::size_t j = 0; j < kNumKeyValPairs; j++) {
      bool success = client->Put(keys[i][j], vals[i][j]);
      ASSERT(success);
    }
  }
  const auto put_end{std::chrono::steady_clock::now()};

  const std::chrono::duration<double> multiput_elapsed{multiput_end -
                                                       multiput_start};
  const std::chrono::duration<double> put_elapsed{put_end - put_start};
  // MultiPut performance should be better than just naively issuing Puts for
  // every key
  ASSERT(multiput_elapsed < put_elapsed);

  // Since no server has shard 0-7, this MultiPut should fail
  std::vector<std::string> invalid_keys =
      make_rand_strs(kNumKeyValPairs, kRandStringLength, "01234567");
  bool success = client->MultiPut(invalid_keys, invalid_keys);
  ASSERT(!success);

  for (std::shared_ptr<KvServer> server : servers) {
    server->stop();
  }

  sm->stop();

  cout_color(GREEN, "Test passed!");
  return 0;
}
