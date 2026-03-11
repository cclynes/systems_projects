#ifndef SIMPLE_KVSTORE_HPP
#define SIMPLE_KVSTORE_HPP

#include <map>
#include <mutex>
#include <string>

#include "kvstore.hpp"
#include "net/server_commands.hpp"

class SimpleKvStore : public KvStore {
 public:
  SimpleKvStore() = default;
  ~SimpleKvStore() = default;

  bool Get(const GetRequest* req, GetResponse* res) override;
  bool Put(const PutRequest* req, PutResponse*) override;
  bool Append(const AppendRequest* req, AppendResponse*) override;
  bool Delete(const DeleteRequest* req, DeleteResponse* res) override;
  bool MultiGet(const MultiGetRequest* req, MultiGetResponse* res) override;
  bool MultiPut(const MultiPutRequest* req, MultiPutResponse*) override;

  std::vector<std::string> AllKeys() override;

 private:
  std::mutex mtx;
  std::unordered_map<std::string, std::string> store_;
};

#endif