#include "MemoryPool.h"

namespace mario_memoryPool{

    MemoryPool::MemoryPool(size_t BlockSize)
        : Blocksize_(BlockSize)
        , SlotSize_(0)
        , firstBlock_(nullptr)
        , curSlot_(nullptr)
        , freelist_(nullptr)
        , lastSlot_(nullptr)
    {}

    MemoryPool::~MemoryPool(){
        Slot* cur = firstBlock_;
        while(cur){
            Slot* next = cur->next;
            operator delete(reinterpret_cast<void*>(cur));
            cur = next;
        }
    }

    void MemoryPool::init(size_t size){
        assert(size > 0);
        SlotSize_ = size;
        firstBlock_ = nullptr;
        curSlot_ = nullptr;
        freelist_ = nullptr;
        lastSlot_ = nullptr;
    }

    void* MemoryPool::allocate(){
        Slot* slot = popFreeList();
        if(slot != nullptr)
            return slot;
        
        Slot* temp;
        {
            std::lock_guard<std::mutex> lock(mutexForBlock_);
            if(curSlot_ >= lastSlot_){
                allocateNewBlock();
            }

            temp = curSlot_;
            curSlot_ += SlotSize_ / sizeof(Slot);
        }

        return temp;
    }

    void MemoryPool::deallocate(void* ptr){
        if(!ptr) return;

        Slot* slot = reinterpret_cast<Slot*>(ptr);
        pushFreeList(slot);
    }

    void MemoryPool::allocateNewBlock(){
        // 头插法插入新的内存块
        void* newBlock = operator new(Blocksize_);
        reinterpret_cast<Slot*>(newBlock)->next = firstBlock_;
        firstBlock_ = reinterpret_cast<Slot*>(newBlock);

        char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
        size_t paddingSize = padPointer(body, SlotSize_);
        curSlot_ = reinterpret_cast<Slot*>(body + paddingSize);

        lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(newBlock) + Blocksize_ - SlotSize_ + 1);
        freelist_ = nullptr;
    }

    size_t MemoryPool::padPointer(char* p, size_t align){
        return(align - reinterpret_cast<size_t>(p)) % align;
    }

    // 无锁入队
    bool MemoryPool::pushFreeList(Slot* slot){
        while(true){
            Slot* oldHead = freelist_.load(std::memory_order_relaxed);
        
            slot->next.store(oldHead, std::memory_order_relaxed);

            if(freelist_.compare_exchange_weak(oldHead, slot, 
            std::memory_order_release, std::memory_order_relaxed)){
                return true;
            }
        }
    }

    // 无锁出队
    Slot* MemoryPool::popFreeList(){
        while(true){
            Slot* oldHead = freelist_.load(std::memory_order_acquire);
            if(oldHead == nullptr){
                return nullptr;
            }

            // 在访问newHead之前再次验证oldHead的有效性
            Slot* newHead = nullptr;
            try{
                newHead = oldHead->next.load(std::memory_order_relaxed);
            }
            catch(...){
                // 返回失败则尝试重新申请内存
            }

            if(freelist_.compare_exchange_weak(oldHead, newHead,
            std::memory_order_acquire, std::memory_order_relaxed)){
                return oldHead;
            }
        }
    }

    void HashBucket::initMemoryPool(){
        for(int i = 0; i < MEMORY_POOL_NUM; ++i){
            getMemoryPool(i).init((i+1) * SLOT_BASE_SIZE);
        }
    }

    MemoryPool& HashBucket::getMemoryPool(int index){
        static MemoryPool memoryPool[MEMORY_POOL_NUM];
        return memoryPool[index];
    }
} // namespace memoryPool;