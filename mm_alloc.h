#ifndef V_MM_ALLOC_H
#define V_MM_ALLOC_H

// Copyright (c) 2022 Maciej Polanski

#include <sys/mman.h>
#include <errno.h>
#include <atomic>
#include <type_traits>
#include <cassert>
#include <cstring>
#include <limits>
#include <atomic>
#include <mutex>


#ifdef MM_DBG
#   include "memory_maps.h"
#   define ER(x) std::cerr x;
#else
#   define ER(x)
#endif

namespace mm {

constexpr bool logall = true;

// $ getconf PAGESIZE
constexpr uintptr_t page_size = 4096;

// Size after which stdlib (mostlikely) uses mmap
const uintptr_t libcThreshold = 128 * 1024; 

// Allocator that forces instant physical mapping (thus zeroing) of memory
struct allocPopulate_base {
    static constexpr int popNone = 0x00;
    static constexpr int popFull = 0x01;
    static constexpr int popHalf = 0x02;
};

template <typename T, int alloc>
struct allocPopulate : public allocPopulate_base
{
    using value_type = T;
  
    allocPopulate() = default;
    constexpr allocPopulate(const allocPopulate<T, alloc>&) noexcept {}

    // Why I need this to compile for God sake???
    template<typename T1>
    struct rebind
    {
        using other = allocPopulate<T1, alloc>;
    };
 
    // *** Workhorse ***
    [[nodiscard]] T* allocate(std::size_t n) 
    {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
          throw std::bad_array_new_length();
       
        // Delegating small alloc to stdlib
        if (n*sizeof(T) < libcThreshold) {
            return static_cast<T*>(malloc(n*sizeof(T)));
        }
        void *pv;
        if (alloc == popNone) {
            // No physical frames mapping 
        	pv = mmap(NULL, n*sizeof(T), PROT_EXEC | PROT_READ | PROT_WRITE, 
                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        }
        else if (alloc == popFull) {
            // MAP_POPULATE (physical mapping) for whole memory
        	pv = mmap(NULL, n*sizeof(T), PROT_EXEC | PROT_READ | PROT_WRITE, 
                MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
        }
        else {  // alloc == popHalf
            // MAP_POPULATE (physical mapping) for 1/2 area
            pv = mmap(NULL, n*sizeof(T), PROT_EXEC | PROT_READ | PROT_WRITE, 
                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            if (pv != MAP_FAILED) {
                pv = mmap(pv, n*sizeof(T)/2, PROT_EXEC | PROT_READ | PROT_WRITE, 
                    MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, -1, 0);
            }
        }
        if (pv != MAP_FAILED) {
          auto p = static_cast<T*>(pv);
          return p;
        }
        //std::cout << "MMAP error: 0x" << std::hex << errno << "\n";
        // 0x16 - invalid argument
        perror("mmapPopulate");
        throw std::bad_alloc();
    }
 
    void deallocate(T* p, std::size_t n) noexcept {
        if (n*sizeof(T) < libcThreshold) {
            return free(p);
        }
        munmap(p, n*sizeof(T));
    }
};

} // namespace mm
#endif
