#include <string>

#include "common/config.hpp"
#include "common/shard.hpp"
#include "test_utils/test_utils.hpp"

constexpr size_t N_SERVERS = 5;
static constexpr std::size_t kRandStringLength = 10;
static constexpr std::size_t kNumKeyValPairs = 500;

int main() {
  ShardControllerConfig sm_config = {
      std::map<std::string, std::vector<Shard>>{}};
  std::vector<std::string> server_addresses = make_server_addresses(N_SERVERS);
  std::vector<Shard> shards = split_into(N_SERVERS);

  // Note the N_SERVERS - 1; last shard [T-Z] doesn't get allocated
  for (std::size_t i = 0; i < N_SERVERS - 1; i++) {
    sm_config.server_to_shards[server_addresses[i]] = {shards[i]};
  }
  // server[4] has no shards
  sm_config.server_to_shards[server_addresses[4]] = std::vector<Shard>{};

  /*
  sm_config.server_to_shards =
    {server_addresses[0], [0-7]},
    {server_addresses[1], [8-E]},
    {server_addresses[2], [F-L]},
    {server_addresses[3], [M-S]},
    {server_addresses[4], {}}
  */

  std::optional<std::string> server;
  server = sm_config.get_server("0123");
  ASSERT(server);
  ASSERT_EQ(*server, server_addresses[0]);

  server = sm_config.get_server("89A");
  ASSERT(server);
  ASSERT_EQ(*server, server_addresses[1]);

  server = sm_config.get_server("GDPR");
  ASSERT(server);
  ASSERT_EQ(*server, server_addresses[2]);

  server = sm_config.get_server("servers");
  ASSERT(server);
  ASSERT_EQ(*server, server_addresses[3]);

  // no server is responsible for keys starting with "w"
  server = sm_config.get_server("windmill");
  ASSERT(!server);

  // Generate keys w/o T-Z; all of these will start with 0-S, so get_server
  // should succeed
  std::vector<std::string> keys =
      make_rand_strs(kNumKeyValPairs, kRandStringLength,
                     "0123456789ABCDEFGHIJKLMNOPQRSabcdefghijklmnopqrs");
  for (std::string key : keys) {
    ASSERT(sm_config.get_server(key));
  }

  // Generate keys w T-Z; all of these will start with T-Z, so get_server
  // should fail
  keys = make_rand_strs(kNumKeyValPairs, kRandStringLength, "TUVWXYZtuvwxyz");
  for (std::string key : keys) {
    ASSERT(!sm_config.get_server(key));
  }

  cout_color(GREEN, "Test passed!");
  return 0;
}
