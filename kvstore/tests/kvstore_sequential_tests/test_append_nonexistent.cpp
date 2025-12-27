#include "test_utils/test_utils.hpp"

int main(int argc, char* argv[]) {
  auto store = make_kvstore(argc, argv);

  std::string key = "nonexistent";
  std::string val = "world";

  // Try to Append to non-existent key
  auto append_req = AppendRequest{.key = key, .value = val};
  auto append_res = AppendResponse{};
  ASSERT(store->Append(&append_req, &append_res));

  // Ensure that created an entry
  auto get_req = GetRequest{.key = key};
  auto get_res = GetResponse{};
  ASSERT(store->Get(&get_req, &get_res));
  ASSERT_EQ(get_res.value, val);
}
