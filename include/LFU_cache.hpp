#ifndef _LFU_CACHE_HPP
#define _LFU_CACHE_HPP

#include <unordered_map>
#include <list>
#include <mutex>

using namespace std;

template <typename Key, typename T>
struct NodeData {
    typedef Key                     key_type;
    typedef T                       value_type;
    typedef size_t                  freq_type;

    key_type        key;
    value_type      val;
    freq_type       freq;

    NodeData(key_type k_, value_type v_, freq_type f_ = 1) : key(k_), val(v_), freq(f_) {}
};

template <typename Key, typename T>
class LFUCache {
public:
    typedef NodeData<Key, T>                        node_data;
    typedef list<node_data>                         LRU_list;
    typedef list<node_data>*                        LRU_list_pointer;
    typedef typename list<node_data>::iterator      node_iterator;
    typedef typename node_data::key_type            key_type;
    typedef typename node_data::value_type          value_type;
    typedef typename node_data::freq_type           freq_type;

private:
    unordered_map<key_type, node_iterator>          keyMap_;
    unordered_map<freq_type, LRU_list_pointer>      freqMap_;
    size_t maxSize_;
    size_t minFreq_;
    mutex mtx_;

private:
    // freq -> freq + 1
    void updateFrequency_(node_iterator nodeIt) {
        key_type key = nodeIt->key;
        value_type val = nodeIt->val;
        freq_type freq = nodeIt->freq;

        freqMap_[freq]->erase(nodeIt);
        if (freq == minFreq_ && freqMap_[freq]->empty())
            minFreq_ += 1;
        if (freqMap_[freq]->empty())
            freqMap_.erase(freq);

        if (freqMap_.find(freq + 1) == freqMap_.end())
            freqMap_[freq + 1] = new LRU_list();
        
        freqMap_[freq + 1]->emplace_front(key, val, freq + 1);
        keyMap_[key] = freqMap_[freq + 1]->begin();
    }

public:
    LFUCache(size_t maxSize) : maxSize_((maxSize == 0) ? 1 : maxSize), minFreq_(0) {}
    ~LFUCache() = default;
    LFUCache(const LFUCache& cache) = delete;
    LFUCache& operator= (const LFUCache& cache) = delete;

    void set(const key_type& key, const value_type& val) {
        lock_guard<mutex> locker(mtx_);

        // 若key存在, 则更新节点
        if (keyMap_.find(key) != keyMap_.end()) {
            node_iterator nodeIt = keyMap_[key];
            nodeIt->val = val;
            updateFrequency_(nodeIt);
            return;
        }

        // 若是新节点, 则先判断节点数量是否已经超过最大容量, 若超过, 则删除频率最低的LRU_list中的节点
        if (keyMap_.size() == maxSize_) {
            LRU_list_pointer plist = freqMap_[minFreq_];
            key_type key = plist->back().key;
            plist->pop_back();
            if (plist->empty()) {
                freqMap_.erase(minFreq_);
            }
            keyMap_.erase(key);
        }

        if (freqMap_.find(1) == freqMap_.end())
            freqMap_[1] = new LRU_list();
        
        freqMap_[1]->emplace_front(key, val, 1);
        keyMap_[key] = freqMap_[1]->begin();
        minFreq_ = 1;
    }

    bool get(const key_type& key, value_type& val) {
        lock_guard<mutex> locker(mtx_);
        
        if (maxSize_ == 0 || keyMap_.find(key) == keyMap_.end())
            return false;
        
        node_iterator nodeIt = keyMap_[key];
        val = nodeIt->val;
        updateFrequency_(nodeIt);

        return true;
    }
};

#endif