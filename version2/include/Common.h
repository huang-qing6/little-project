#pragma once
#include <cstddef>
#include <atomic>
#include <array>

namespace mario_memoryPool{
    constexpr size_t ALIGNMENT = 8;
    constexpr size_t MAX_BYTES = 256*1024;
    constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;// ALIGNMENT等于指针void*的大小

    struct BlockHeader{
        size_t size;
        bool isUse;
        BlockHeader* next;
    };

    class SizeClass{
        public:
            static size_t roundUp(size_t bytes){
                return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT);
            }

            static size_t getIndex(size_t bytes){
                // 确保bytes 至少为 ALIGNMENT
                bytes = std::max(bytes, ALIGNMENT);
                // 向上取整后 -1
                return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
            }
    };

} // namespace memorypool