#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <cstddef>

namespace mario_memoryPool{
    #define MEMORY_POOL_NUM 64
    #define SLOT_BASE_SIZE 8
    #define MAX_SLOT_SIZE 512

    struct Slot
    {
        std::atomic<Slot*> next; // 原子指针
    };
    
    class MemoryPool{
        public:
            MemoryPool(size_t Blocksize = 4096);
            ~MemoryPool();

            void init(size_t);

            void* allocate();
            void deallocate(void*);
        private:
            void allocateNewBlock();
            size_t padPointer(char* p, size_t align);

            // CAS无锁出入队
            bool pushFreeList(Slot* slot);
            Slot* popFreeList();
        private:
            int Blocksize_; // 内存大小
            int SlotSize_; // 槽大小
            Slot* firstBlock_; // 首个内存块地址
            Slot* curSlot_; // 当前未使用的槽
            std::atomic<Slot*> freelist_; // 指向空闲的槽，被使用后有被释放的
            Slot* lastSlot_; // 当前内存块能够存放元素的位置标识
            std::mutex mutexForBlock_; // 保证多线程下避免不必要的重复开辟内存
    };

    class HashBucket{
        public:
            static void initMemoryPool();
            static MemoryPool& getMemoryPool(int index);
        
            static void* useMemory(size_t size){
                if(size <= 0)
                    return nullptr;
                if(size > MAX_SLOT_SIZE)
                    return operator new(size);

                return getMemoryPool(((size+7)/SLOT_BASE_SIZE)-1).allocate(); // size除以8向上取整
            }

            static void freeMemory(void* ptr, size_t size){
                if(!ptr)
                    return;
                
                if(size > MAX_SLOT_SIZE){
                    operator delete(ptr);
                    return;
                }

                getMemoryPool(((size+7)/SLOT_BASE_SIZE)-1).allocate();
            }
    };

    template<typename T, typename... Args>
    T* newElement(Args&&... args){
        T* p = nullptr;
        if((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr)
            new(p) T(std::forward<Args>(args)...);

        return p;
    }

    template<typename T>
    void deleteElement(T* p){
        if(p){
            p->~T();
            HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
        }
    }
} // namespace MemoryPool