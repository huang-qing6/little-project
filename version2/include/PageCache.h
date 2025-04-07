#pragma once
#include "Common.h"
#include <map>
#include <mutex>

namespace mario_memoryPool
{
    class PageCache{
        public:
            static const size_t PAGE_SIZE = 4096; // 4K页

            static PageCache& getInstance(){
                static PageCache instace;
                return instace;
            }

            // 分配指定页数
            void* allocateSpan(size_t numPages);

            // 释放span
            void deallocateSpan(void* ptr, size_t numPages);
        
        private:
            PageCache() = default;

            void* systemAlloc(size_t numpages);


        private:
            struct Span{
                void* pageAddr;  // 页启始地址
                size_t numPages; // 页数
                Span* next;      // 链表指针
            };

            // 按页数管理空闲空间span，不同页数对应不同Span链表
            std::map<size_t, Span*> freeSpans_;
            // 页号到Span的映射，方便回收
            std::map<void*, Span*> spanMap_;
            std::mutex mutex_;       
    };
} // namespace mario_memoryPool
