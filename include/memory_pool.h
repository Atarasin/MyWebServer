#ifndef _MEMORY_POOL_H
#define _MEMORY_POOL_H

#include <utility>
#include <cstdint>

template <typename T, std::size_t BlockSize = 4096>
class MemoryPool {
public:
    typedef T           value_type;
    typedef T*          pointer;

private:
    union Slot {
        value_type      element;        // Slot = max(value_type, Slot*) >= value_type
        Slot*           next;
    };

    typedef Slot        slot_type;
    typedef Slot*       slot_pointer;
    typedef char*       data_pointer;

    slot_pointer    currentBlock_;
    slot_pointer    currentSlot_;
    slot_pointer    lastSlot_;
    slot_pointer    freeSlots_;

    static_assert(BlockSize >= sizeof(slot_type) * 2, "BlockSize is too small.");

public:
    MemoryPool() : currentBlock_(nullptr), currentSlot_(nullptr), lastSlot_(nullptr), freeSlots_(nullptr) {}
    MemoryPool(MemoryPool&& other) : currentBlock_(other.currentBlock_), currentSlot_(other.currentSlot_), 
        lastSlot_(other.lastSlot_), freeSlots_(other.freeSlots_) 
    {
        other.currentBlock_ = nullptr;
        other.currentSlot_ = nullptr;
        other.lastSlot_ = nullptr;
        other.freeSlots_ = nullptr;
    }

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator= (const MemoryPool&) = delete;

    ~MemoryPool() {
        slot_pointer cur = currentBlock_;
        while (cur) {
            slot_pointer next = cur->next;
            operator delete(static_cast<void*>(cur));
            cur = next;
        }
    }

private:
    void allocateBlock();           // core
    std::size_t padForAlign(data_pointer p, std::size_t align) {
        uintptr_t uint_p = reinterpret_cast<uintptr_t>(p);
        return (align - uint_p) % align;
    }

public:
    pointer allocate(std::size_t n = 1);
    void deallocate(pointer p, std::size_t n = 1);

    template <typename Object, typename... Args>
    void construct(Object* p, Args&&... args);

    template <typename Object>
    void destroy(Object* p);

    template <typename... Args>
    pointer newObject(Args&&... args);

    void deleteObject(pointer p);
};

// 1. allocate a new block; 2. align
template <typename T, std::size_t BlockSize>
void MemoryPool<T, BlockSize>::allocateBlock() {
    data_pointer newBlock = static_cast<data_pointer>(operator new(BlockSize));
    reinterpret_cast<slot_pointer>(newBlock)->next = currentBlock_;
    currentBlock_ = reinterpret_cast<slot_pointer>(newBlock);

    data_pointer body = newBlock + sizeof(slot_pointer);
    std::size_t n_bytes = padForAlign(body, alignof(slot_type));
    currentSlot_ = reinterpret_cast<slot_pointer>(body + n_bytes);
    lastSlot_ = reinterpret_cast<slot_pointer>(newBlock + BlockSize - sizeof(slot_type) + 1);
}

template <typename T, std::size_t BlockSize>
inline 
typename MemoryPool<T, BlockSize>::pointer MemoryPool<T, BlockSize>::allocate(std::size_t n) {
    if (freeSlots_ != nullptr) {
        pointer allo_p = reinterpret_cast<pointer>(freeSlots_);
        freeSlots_ = freeSlots_->next;
        return allo_p;
    }
    else {
        if (currentSlot_ >= lastSlot_) allocateBlock();
        return reinterpret_cast<pointer>(currentSlot_++);
    }
}

template <typename T, std::size_t BlockSize>
inline
void MemoryPool<T, BlockSize>::deallocate(pointer p, std::size_t n) {
    if (p != nullptr) {
        reinterpret_cast<slot_pointer>(p)->next = freeSlots_;
        freeSlots_ = reinterpret_cast<slot_pointer>(p);
    }
}

template <typename T, std::size_t BlockSize>
template <typename Object, typename... Args>
inline 
void MemoryPool<T, BlockSize>::construct(Object* p, Args&&... args) {
    new(p) Object(std::forward<Args>(args)...);
}

template <typename T, std::size_t BlockSize>
template <typename Object>
inline 
void MemoryPool<T, BlockSize>::destroy(Object* p) {
    p->~Object();
}

template <typename T, std::size_t BlockSize>
template <typename... Args>
inline 
typename MemoryPool<T, BlockSize>::pointer MemoryPool<T, BlockSize>::newObject(Args&&... args) {
    pointer allo_p = allocate();
    construct<value_type>(allo_p, std::forward<Args>(args)...);
    return allo_p;
}

template <typename T, std::size_t BlockSize>
inline
void MemoryPool<T, BlockSize>::deleteObject(pointer p) {
    if (p != nullptr) {
        // destroy<value_type>(p);
        destroy(p);
        deallocate(p);
    }
}

#endif