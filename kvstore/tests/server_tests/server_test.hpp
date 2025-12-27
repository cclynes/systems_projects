#include "common/config.hpp"
#include "common/shard.hpp"
#include "server/server.hpp"
#include "shardcontroller/shardcontroller.hpp"

class ServerTest {
 public:
  int run_test();
  void test_process_config(std::shared_ptr<Shardcontroller> sm,
                           std::shared_ptr<KvServer> source,
                           std::shared_ptr<KvServer> dest,
                           std::string dest_addr, Shard shard,
                           std::vector<std::string> keys,
                           std::vector<std::string> values);
};
