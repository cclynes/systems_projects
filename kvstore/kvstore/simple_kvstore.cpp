#include <unordered_map>

#include "simple_kvstore.hpp"

bool SimpleKvStore::Get(const GetRequest* req, GetResponse* res) {

  this->mtx.lock();

  auto it = this->store_.find(req->key);
  if (it == this->store_.end()) {
    return false;
  }
  res->value = it->second;

  this->mtx.unlock();
  return true;
}

bool SimpleKvStore::Put(const PutRequest* req, PutResponse*) {

  this->mtx.lock();

  this->store_[req->key] = req->value;
  
  this->mtx.unlock();
  return true;
}

bool SimpleKvStore::Append(const AppendRequest* req, AppendResponse*) {

  this->mtx.lock();

  if (this->store_.contains(req->key)) {
    this->store_[req->key] += req->value;
  }
  else {
    this->store_[req->key] = req->value;
  }

  this->mtx.unlock();
  return true;
}

bool SimpleKvStore::Delete(const DeleteRequest* req, DeleteResponse* res) {

  this->mtx.lock();

  auto it = this->store_.find(req->key);
  if (it == this->store_.end()) {
    return false;
  }
  res->value = it->second;
  this->store_.erase(req->key);

  this->mtx.unlock();
  return true;
}

bool SimpleKvStore::MultiGet(const MultiGetRequest* req,
                             MultiGetResponse* res) {

  this->mtx.lock();
  
  res->values.clear();
  res->values.reserve(req->keys.size());

  for (const auto& k : req->keys) {
    auto it = this->store_.find(k);
    if (it == this->store_.end()) {
      res->values.clear();
      return false;
    }
    res->values.push_back(it->second);
  }

  this->mtx.unlock();
  return true;
}

bool SimpleKvStore::MultiPut(const MultiPutRequest* req, MultiPutResponse*) {

  this->mtx.lock();

  if (req->keys.size() != req->values.size()) {
    return false;
  }

  for (size_t i = 0; i < req->keys.size(); i++) {
    this->store_[req->keys[i]] = req->values[i];
  }

  this->mtx.unlock();
  return true;
}

std::vector<std::string> SimpleKvStore::AllKeys() {

  this->mtx.lock();

  std::vector<std::string> keys;
  keys.reserve(this->store_.size());

  for (const auto& pair : this->store_) {
    keys.push_back(pair.first);
  }

  this->mtx.unlock();
  return keys;
}