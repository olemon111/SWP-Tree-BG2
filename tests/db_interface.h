#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <future>
#include "ycsb/ycsb-c.h"
#include "combotree/combotree.h"
#include "../src/combotree_config.h"
#include "fast-fair/btree.h"
#include "nvm_alloc.h"
#include "lbtree/lbtree_wrapper.hpp"
#include "random.h"
#include "alex/alex.h"
#include "lipp/lipp.h"

#define USE_MEM

using combotree::ComboTree;
using namespace std;

/*
 *file_exists -- checks if file exists
 */
static inline int file_exists(char const *file) { return access(file, F_OK); }

namespace KV
{
  class Key_t
  {
    typedef std::array<double, 1> model_key_t;

  public:
    static constexpr size_t model_key_size() { return 1; }
    static Key_t max()
    {
      static Key_t max_key(std::numeric_limits<uint64_t>::max());
      return max_key;
    }
    static Key_t min()
    {
      static Key_t min_key(std::numeric_limits<uint64_t>::min());
      return min_key;
    }

    Key_t() : key(0) {}
    Key_t(uint64_t key) : key(key) {}
    Key_t(const Key_t &other) { key = other.key; }
    Key_t &operator=(const Key_t &other)
    {
      key = other.key;
      return *this;
    }

    model_key_t to_model_key() const
    {
      model_key_t model_key;
      model_key[0] = key;
      return model_key;
    }

    friend bool operator<(const Key_t &l, const Key_t &r) { return l.key < r.key; }
    friend bool operator>(const Key_t &l, const Key_t &r) { return l.key > r.key; }
    friend bool operator>=(const Key_t &l, const Key_t &r) { return l.key >= r.key; }
    friend bool operator<=(const Key_t &l, const Key_t &r) { return l.key <= r.key; }
    friend bool operator==(const Key_t &l, const Key_t &r) { return l.key == r.key; }
    friend bool operator!=(const Key_t &l, const Key_t &r) { return l.key != r.key; }

    uint64_t key;
  };
}

namespace dbInter
{

  static inline std::string human_readable(double size)
  {
    static const std::string suffix[] = {
        "B",
        "KB",
        "MB",
        "GB"};
    const int arr_len = 4;

    std::ostringstream out;
    out.precision(2);
    for (int divs = 0; divs < arr_len; ++divs)
    {
      if (size >= 1024.0)
      {
        size /= 1024.0;
      }
      else
      {
        out << std::fixed << size;
        return out.str() + suffix[divs];
      }
    }
    out << std::fixed << size;
    return out.str() + suffix[arr_len - 1];
  }

  class AlexDB : public ycsbc::KvDB
  {
    typedef uint64_t KEY_TYPE;
    typedef uint64_t PAYLOAD_TYPE;
    using Alloc = std::allocator<std::pair<KEY_TYPE, PAYLOAD_TYPE>>;
    typedef alex::Alex<KEY_TYPE, PAYLOAD_TYPE, alex::AlexCompare, Alloc> alex_t;

  public:
    AlexDB() : alex_(nullptr) {}
    AlexDB(alex_t *alex) : alex_(alex) {}
    virtual ~AlexDB()
    {
      delete alex_;
    }

    void Init()
    {
      alex_ = new alex_t();
    }

    void Bulk_load(const std::pair<uint64_t, uint64_t> data[], int size)
    {
      alex_->bulk_load(data, size);
    }

    void Info()
    {
      // alex_->PrintInfo();
    }

    int Put(uint64_t key, uint64_t value)
    {
      alex_->insert(key, value);
      return 1;
    }
    int Get(uint64_t key, uint64_t &value)
    {
      value = *(alex_->get_payload(key));
      return 1;
    }
    int Update(uint64_t key, uint64_t value)
    {
      uint64_t *addrs = (alex_->get_payload(key));
      *addrs = value;
      return 1;
    }
    int Delete(uint64_t key)
    {
      alex_->erase(key);
      return 1;
    }
    int Scan(uint64_t start_key, int len, std::vector<std::pair<uint64_t, uint64_t>> &results)
    {
      auto it = alex_->lower_bound(start_key);
      int num_entries = 0;
      while (num_entries < len && it != alex_->end())
      {
        results.push_back({it.key(), it.payload()});
        num_entries++;
        it++;
      }
      return 1;
    }
    void PrintStatic()
    {
    }

  private:
    alex_t *alex_;
  };

  class LippDB : public ycsbc::KvDB
  {
    typedef LIPP<uint64_t, uint64_t> lipp_t;

  public:
    LippDB() : lipp_(nullptr) {}
    LippDB(lipp_t *lipp) : lipp_(lipp) {}
    ~LippDB()
    {
      delete lipp_;
    }
    void Init()
    {
      lipp_ = new lipp_t();
    }
    void Info()
    {
      // lipp_->print_depth();
    }
    void Bulk_load(const std::pair<uint64_t, uint64_t> data[], int size)
    {
      lipp_->bulk_load(data, size);
      // lipp_->verify();
    }
    int Put(uint64_t key, uint64_t value)
    {
      lipp_->insert(key, value);
      return 1;
    }
    int Get(uint64_t key, uint64_t &value)
    {
      value = lipp_->at(key);
      return 1;
    }
    int Update(uint64_t key, uint64_t value)
    {
      // if (!lipp_->exists(key))
      // {
      lipp_->insert(key, value);
      // }
      return 1;
    }
    int Delete(uint64_t key)
    {
      return 1;
    }
    int Scan(uint64_t start_key, int len, std::vector<std::pair<uint64_t, uint64_t>> &results)
    {
      return 1;
    }
    void PrintStatic()
    {
    }

  private:
    lipp_t *lipp_;
  };

  class FastFairDb : public ycsbc::KvDB
  {
  public:
    FastFairDb() : btree_(TOID_NULL(btree)) {}
    FastFairDb(TOID(btree) tree) : btree_(tree) {}
    virtual ~FastFairDb()
    {
      pmemobj_close(pop_);
    }
    void Init()
    {
      char *persistent_path = "/mnt/pmem1/lbl/fastfair-pool.obj";
      cout << "before init btree" << endl;
      btree_ = TOID_NULL(btree);
      if (file_exists(persistent_path) != 0)
      {
        static const uint64_t pool_size = 40UL * 1024 * 1024 * 1024; // 8000000000
        pop_ = pmemobj_create(persistent_path, "btree", pool_size, 0666);
        btree_ = POBJ_ROOT(pop_, btree);
        D_RW(btree_)->constructor(pop_);
      }
      else
      {
        pop_ = pmemobj_open(persistent_path, "btree");
        btree_ = POBJ_ROOT(pop_, btree);
      }
      cout << "init fastfair db" << endl;
    }

    void Info()
    {
      // tree_->PrintInfo();
    }

    void Close()
    {
    }
    int Put(uint64_t key, uint64_t value)
    {
      D_RW(btree_)->btree_insert(key, (char *)value);
      return 1;
    }
    int Get(uint64_t key, uint64_t &value)
    {
      value = (uint64_t)D_RW(btree_)->btree_search(key);
      return 1;
    }
    int Update(uint64_t key, uint64_t value)
    {
      D_RW(btree_)->btree_delete(key);
      D_RW(btree_)->btree_insert(key, (char *)value);
      return 1;
    }

    int Delete(uint64_t key)
    {
      D_RW(btree_)->btree_delete(key);
      return 1;
    }

    int Scan(uint64_t start_key, int len, std::vector<std::pair<uint64_t, uint64_t>> &results)
    {
      // tree_->btree_search_range(start_key, UINT64_MAX, results, len);
      return 1;
    }
    void PrintStatic()
    {
      // tree_->PrintInfo();
    }

  private:
    TOID(btree)
    btree_;
    PMEMobjpool *pop_;
  };

  class LBTreeDB : public ycsbc::KvDB
  {
  public:
    LBTreeDB() : tree_(nullptr) {}
    LBTreeDB(lbtree_wrapper *tree) : tree_(tree) {}
    virtual ~LBTreeDB() {}
    void Init()
    {
      constexpr const auto MEMPOOL_ALIGNMENT = 4096LL;
      size_t key_size_ = 0;
      size_t pool_size_ = ((size_t)(40UL * 1024 * 1024 * 1024));
      const char *pool_path_ = "/mnt/pmem1/lbl/lbtree-pool.obj";
      kp = new char[8];
      vp = new char[8];

      initUseful();
      worker_id = 0;
      worker_thread_num = 1;
      the_thread_mempools.init(1, 4096, MEMPOOL_ALIGNMENT);
      // cout << "mempools init" << endl;
      the_thread_nvmpools.init(1, pool_path_, pool_size_);
      // cout << "nvmpools init" << endl;
      char *nvm_addr = (char *)nvmpool_alloc(256);
      nvmLogInit(1);
      tree_ = new lbtree_wrapper(nvm_addr, false);
      cout << "init lbtree wrapper" << endl;
    }

    void Info()
    {
      // tree_->PrintInfo();
    }

    void Close()
    {
    }

    void Bulk_load(const std::pair<uint64_t, uint64_t> data[], int size)
    {
      // tree_->bulkload(size, );
    }
    int Put(uint64_t key, uint64_t value)
    {
      memcpy(kp, &key, sizeof(key));
      memcpy(vp, &value, sizeof(value));
      tree_->insert(kp, sizeof(key), vp, sizeof(value));
      return 1;
    }
    int Get(uint64_t key, uint64_t &value)
    {
      memcpy(kp, &key, sizeof(key));
      tree_->find(kp, sizeof(key), vp);
      uint64_t res;
      memcpy(&res, vp, sizeof(value));
      value = res;
      return 1;
    }
    int Update(uint64_t key, uint64_t value)
    {
      memcpy(kp, &key, sizeof(key));
      memcpy(vp, &value, sizeof(value));
      tree_->update(kp, sizeof(key), vp, sizeof(value));
      return 1;
    }
    int Delete(uint64_t key)
    {
      // tree_->del(key);
      return 1;
    }

    int Scan(uint64_t start_key, int len, std::vector<std::pair<uint64_t, uint64_t>> &results)
    {
      return 1;
    }
    void PrintStatic()
    {
      // tree_->PrintInfo();
    }

  private:
    lbtree_wrapper *tree_;
    char *kp, *vp;
  };

} // namespace dbInter
