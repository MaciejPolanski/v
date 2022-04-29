#ifndef V_MM_ALLOC_H
#define V_MM_ALLOC_H
// Copyright Maciej Polanski

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
#   include "../memory_maps.h"
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

// Helper struct managing chunks of anonymous, mapped memory
// enforcing page_size for pointer-to-page arythmetics
union memChunk {
    struct {
       memChunk* next;
       uintptr_t pgSize;
    };
    // buffer forces pointer incrementation to be in whole pages
    char buffer[page_size];

    memChunk(std::size_t _pgSize) : next(0), pgSize(_pgSize){}

    // Static part
    static std::atomic<memChunk*> head;

    static std::size_t mem2pages(uintptr_t size) { 
        return (size-1) / page_size + 1;
    }

    static void put(memChunk* chunk)
    {
        if (!chunk)
            return;
        ER( << "put  " << mapSpan(chunk, chunk->pgSize * page_size) <<
                ", " << chunk->pgSize  <<" pages\n");
        memChunk **oldHead = &chunk->next;
        while (*oldHead) {
            oldHead = &(*oldHead)->next;
        };
        
        *oldHead = head.load(std::memory_order_relaxed);
        while (!head.compare_exchange_weak(*oldHead, chunk,
                std::memory_order_release,
                std::memory_order_relaxed))
        ;
    }
    
    static memChunk* get()
    {
        //  ABA problem and potential ret->next dereference crash
        static std::mutex m;
        std::lock_guard<std::mutex> lock(m);

        memChunk* ret = head.load(std::memory_order_relaxed);
        while (ret && !head.compare_exchange_weak(ret, ret->next,
                std::memory_order_acquire))
        ;
        if (ret)
            ret->next = nullptr;
        ER( << "get  " << mapSpan(ret, (ret ? (ret->pgSize*page_size) : 0)) <<
                ", " <<  (ret ? ret->pgSize : 0) << " pages\n");
        return ret;
    }

    static void release()
    {
        memChunk* mc;
        while ((mc = get())){
            int ret = munmap(mc, mc->pgSize * page_size);
            if (ret == -1/*MAP_FAILED*/) {
                 perror("mmapPreserve");
                 throw std::bad_alloc();
            }
        }
    }
};
static_assert(sizeof(memChunk) == page_size);

// Allocator preserving memory between allocations 
// This should rather be implemented insid glibc alloc()
template <class T>
struct allocPreserve
{
    typedef T value_type;
   
    allocPreserve() = default;
    template <class U> constexpr allocPreserve(const allocPreserve<U>&) noexcept {}
   
    template<typename T1>
    struct rebind
    {
        using other = allocPreserve<T1>;
    };

    void* mm_alloc(size_t size)
    {
        void* pv = mmap(NULL, size, PROT_EXEC | PROT_READ | PROT_WRITE, 
                MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, /*not a file mapping*/-1, 0);
        ER( << "aloc " << mapSpan(pv, size) << ", " << size / page_size << " pages\n");
        if (pv == MAP_FAILED) {
            perror("mmapPreserve");
            throw std::bad_alloc();
        }
        return pv;
    }

    void mm_populate(void* what, size_t size)
    {
        void* pv = mmap(what, size, PROT_EXEC | PROT_READ | PROT_WRITE, 
                MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_FIXED, /*not a file mapping*/-1, 0);
        ER( << "popl " << addr{pv} << ", " << std::hex << "0x" << size << std::dec << ", " <<
                "(" << size / page_size << " pages)\n");
        if (pv == MAP_FAILED) {
            perror("mmapPreserve");
            throw std::bad_alloc();
        }
    }

    void* mm_grow(void* what, size_t size_a, size_t size_b)
    {
        // Ugly woraround for mremap of multiarea memory issue 
        // memset(what, 0, size_a);
        ER( << "grow " << mapSpan(what, size_a) << ", " << size_a / page_size << " pages\n");
        void* pv = mremap(what, size_a, size_b, MREMAP_MAYMOVE);
        if (pv == MAP_FAILED) {
            perror("mmapPreserve");
            throw std::bad_alloc();
        }
        ER( << "     " << mapSpan(pv, size_b) << ", " << size_b / page_size << " pages\n");
        return pv;
    }

    void* mm_move(void* what, size_t size, void* where)
    {
        ER( << "move " << mapSpan(what, size) << ", " << size / page_size << " pages\n");
        void* pv = mremap(what, size, size, MREMAP_MAYMOVE | MREMAP_FIXED, where);
        if (pv == MAP_FAILED) {
            perror("mmapPreserve");
            throw std::bad_alloc();
        }
        ER( << "     " << mapSpan(pv, size) << "\n");
        return pv;
    }
  
    [[nodiscard]] T* allocate(std::size_t n) 
    {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length();
    
        uintptr_t pgNeeded = n ? memChunk::mem2pages(n * sizeof(T)) : 1;
        ER( << "ALOC " << n * sizeof(T) << ", " << pgNeeded <<" pages\n");
    
        memChunk* retVal = memChunk::get();
        // If we need new memory from system
        if (retVal == nullptr) {
            auto p = static_cast<T*>(mm_alloc(pgNeeded * page_size));
            return p;
        }
        // Chunk too big, cut tail and return to the queue
        assert((uintptr_t)retVal % page_size == 0);
        if (pgNeeded < retVal->pgSize) {
            memChunk*  mc = new (retVal + pgNeeded) memChunk (retVal->pgSize - pgNeeded);
            retVal->pgSize = pgNeeded;
            memChunk::put(mc);
        }
        // Chunk ideal (maybe after trimming)
        if (pgNeeded == retVal->pgSize) {
            auto p = (T*)retVal;
            return p;
        }
        // We need to grow. Add address space after our chunk (may move mapping)
        void* pv = mm_grow(retVal, retVal->pgSize * page_size, pgNeeded * page_size);
        retVal = static_cast<memChunk*>(pv);

        // Let's start collecting chunks
        assert(pgNeeded > retVal->pgSize);
        pgNeeded -= retVal->pgSize;
        memChunk* mc_next = retVal + retVal->pgSize;
        while (pgNeeded) {
            memChunk* mc1 = memChunk::get();
            // No more chunks
            if (!mc1) {
                //mm_populate(mc_next, pgNeeded * page_size);
                auto p = (T*)retVal;
                return p;
            }
            // Chunk too big, return tail to queue
            if (pgNeeded < mc1->pgSize) {
                auto mc = new (mc1 + pgNeeded) memChunk(mc1->pgSize - pgNeeded);
                mc1->pgSize = pgNeeded;
                memChunk::put(mc);
            }
            // Smaller or equal(maybe after trimming), just attach at the end
            void* pv = mm_move(mc1, mc1->pgSize * page_size, mc_next);
            mc1 = static_cast<memChunk*>(pv);
            assert(mc1 == mc_next);
            assert(pgNeeded >= mc1->pgSize);
            pgNeeded -= mc1->pgSize; 
            mc_next = mc1 + mc1->pgSize;
        }
        auto p = (T*)retVal;
        return p;
    }
 
    void deallocate(T* p, std::size_t n) noexcept 
    {
        std::size_t pgNeeded = memChunk::mem2pages(n * sizeof(T));
        ER( << "DEAL " << n * sizeof(T) << ", " << pgNeeded <<" pages\n");
        memChunk* mc = new (p) memChunk(n ? pgNeeded : 1);
        memChunk::put(mc);
        //munmap(p, pages * page_size);
    }
};

} // namespace mm
#endif
