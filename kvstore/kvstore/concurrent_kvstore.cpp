#include "concurrent_kvstore.hpp"

#include <mutex>
#include <optional>

bool ConcurrentKvStore::Get(const GetRequest* req, GetResponse* res) {

  size_t bkt = this->store.bucket(req->key);

  auto& mtx = this->store.shared_mtxs[bkt];
  std::shared_lock guard(mtx);

  std::optional<DbItem> dbItem = this->store.getIfExists(bkt, req->key);
  if (!dbItem) {
    return false;
  }
  res->value = dbItem->value;
  return true;
}

bool ConcurrentKvStore::Put(const PutRequest* req, PutResponse*) {

  size_t bkt = this->store.bucket(req->key);

  auto& mtx = this->store.shared_mtxs[bkt];
  std::unique_lock guard(mtx);

  this->store.insertItem(bkt, req->key, req->value);
  return true;
}

bool ConcurrentKvStore::Append(const AppendRequest* req, AppendResponse*) {

  size_t bkt = this->store.bucket(req->key);

  auto& mtx = this->store.shared_mtxs[bkt];
  std::unique_lock guard(mtx);

  std::optional<DbItem> dbItem = this->store.getIfExists(bkt, req->key);

  if (!dbItem) {
    this->store.insertItem(bkt, req->key, req->value);
  }
  else {
    this->store.insertItem(bkt, req->key, dbItem->value + req->value);
  }
  return true;
}

bool ConcurrentKvStore::Delete(const DeleteRequest* req, DeleteResponse* res) {

  size_t bkt = this->store.bucket(req->key);

  auto& mtx = this->store.shared_mtxs[bkt];
  std::unique_lock guard(mtx);

  std::optional<DbItem> dbItem = this->store.getIfExists(bkt, req->key);

  if (!dbItem) {
    return false;
  }
  res->value = dbItem->value;
  
  this->store.removeItem(this->store.bucket(req->key), req->key);
  return true;
}

bool ConcurrentKvStore::MultiGet(const MultiGetRequest* req,
                                 MultiGetResponse* res) {

  std::array<bool, DbMap::BUCKET_COUNT> needed{};
  for (const auto& k : req->keys) {
    needed[this->store.bucket(k)] = true;
  }

  std::vector<std::shared_lock<std::shared_mutex>> guards;
  guards.reserve(DbMap::BUCKET_COUNT);

  for (size_t b = 0; b < DbMap::BUCKET_COUNT; b++) {
    if (needed[b]) {
      guards.emplace_back(this->store.shared_mtxs[b]);
    }
  }

  res->values.clear();
  res->values.reserve(req->keys.size());

  for (const auto& k : req->keys) {
    std::optional<DbItem> item = this->store.getIfExists(this->store.bucket(k), k);
    if (!item) {
      res->values.clear();
      return false;
    }
    res->values.push_back(item->value);
  }

  return true;
}

bool ConcurrentKvStore::MultiPut(const MultiPutRequest* req,
                                 MultiPutResponse*) {

  std::array<bool, DbMap::BUCKET_COUNT> needed{};
  for (const auto& k : req->keys) {
    needed[this->store.bucket(k)] = true;
  }

  std::vector<std::unique_lock<std::shared_mutex>> guards;
  guards.reserve(DbMap::BUCKET_COUNT);

  for (size_t b = 0; b < DbMap::BUCKET_COUNT; b++) {
    if (b) {
      guards.emplace_back(this->store.shared_mtxs[b]);
    }
  }
  
  if (req->keys.size() != req->values.size()) {
    return false;
  }

  for (size_t i = 0; i < req->keys.size(); i++) {
    this->store.insertItem(this->store.bucket(req->keys[i]), req->keys[i], req->values[i]);
  }

  return true;
}

std::vector<std::string> ConcurrentKvStore::AllKeys() {
  std::vector<std::string> keys;
  for (const std::list<DbItem>& bucket : this->store.buckets) {
    for (const DbItem& item : bucket) {
      keys.push_back(item.key);
    }
  }
  return keys;
}