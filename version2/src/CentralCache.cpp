#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <cassert>
#include <thread>

namespace mario_memoryPool{

    // 每次从PageCache获取span大小，单位：页
    static const size_t SPAN_PAGES = 8;

    void* CentralCache::fetchRange(size_t index){
        // 索引检查，大于FREE_LIST_SIZE时，说明申请过大应该直接向系统申请
        if(index >= FREE_LIST_SIZE)
            return nullptr;

        // 自旋保护
        while(locks_[index].test_and_set(std::memory_order_acquire)){
            std::this_thread::yield();
        }

        void* result = nullptr;
        try{
            // 尝试从中心缓存获取内存
            result = centralFreeList_[index].load(std::memory_order_relaxed);

            if(!result){
                // 如果中心缓存为空，从页患侧获取新内存块
                size_t size = (index + 1) * ALIGNMENT;
                result = fetchFromPageCache(size);

                if(!result){
                    locks_[index].clear(std::memory_order_release);
                    return nullptr;
                }

                // 将获取的内存块切分成小块
                char* start = static_cast<char*>(result);
                size_t blockNum = (SPAN_PAGES * PageCache::PAGE_SIZE) / size;

                if(blockNum > 1){// 确保有两个块才构件链表
                    for(size_t i = 1; i < blockNum; ++i){
                        void* current = start + (i - 1) * size;
                        void* next = start + i * size;
                        *reinterpret_cast<void**>(current) = next;
                    }
                    *reinterpret_cast<void**>(start + (blockNum - 1) * size) = nullptr;

                    // 保存result的下一个节点
                    void* next = *reinterpret_cast<void**>(result);
                    // 将result与下一个链表断开
                    *reinterpret_cast<void**>(result) = nullptr;
                    // 更新中心缓存
                    centralFreeList_[index].store(next, std::memory_order_release);
                }
            }
            else {
                // 保存下一个result的下一个节点
                void* next = *reinterpret_cast<void**>(result);
                // 断开result与链表
                *reinterpret_cast<void**>(result) = nullptr;

                // 更新中心缓存
                centralFreeList_[index].store(next, std::memory_order_release);
            }
        }
        catch(...){
            locks_[index].clear(std::memory_order_release);
            throw;
        }
    
        // 释放锁
        locks_[index].clear(std::memory_order_release);
        return result;
    }

    void CentralCache::returnRange(void* start, size_t size, size_t index){
        // 当索引大于等于FREE_LIST_SIZE，说明内存过大应该直接向系统归还
        if(!start || index >= FREE_LIST_SIZE)
            return;

        while(locks_[index].test_and_set(std::memory_order_acquire)){
            std::this_thread::yield();
        }

        try{
            // 找到要归还的链表的最后一个节点
            void* end = start;
            size_t count = 1;
            while(*reinterpret_cast<void**>(end) != nullptr && count < size){
                end = *reinterpret_cast<void**>(end);
                count++;
            }

            // 将归还的链表连接到中心缓存的链表头部
            void* current = centralFreeList_[index].load(std::memory_order_relaxed);
            *reinterpret_cast<void**>(end) = current;
            centralFreeList_[index].store(start, std::memory_order_release); // 将归还的链表头设为新链表头
        }
        catch(...){
            locks_[index].clear(std::memory_order_release);
            throw;
        }

        locks_[index].clear(std::memory_order_release);
    }

    void* CentralCache::fetchFromPageCache(size_t size){
        // 1 计算实际需要页数
        size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;

        // 2 根据大小决定分配策略
        if(size <= SPAN_PAGES * PageCache::PAGE_SIZE){
            // 小于等于32K，使用固定大小页
            return PageCache::getInstance().allocateSpan(SPAN_PAGES);
        }else{
            // 大于32K，按需分配
            return PageCache::getInstance().allocateSpan(numPages);
        }
    }
} // namespace mario memorypool