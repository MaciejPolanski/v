// Copyright Maciej Polanski

#include <sys/mman.h>
#include <errno.h>
#include <atomic>
#include <type_traits>
#include <cassert>
#include <limits>
#include <atomic>
#include <mutex>

namespace v_allocator {

// $ getconf PAGESIZE
constexpr uintptr_t page_size = 4096;

// Size after which stdlib (mostlikely) uses mmap
const std::size_t libcTreshold = 100 * 1024; 

// Allocator that forces instant physical mapping (thus zeroing) of memory
struct mmapPopulate_base {
    static constexpr int popNone = 0x00;
    static constexpr int popFull = 0x01;
    static constexpr int popHalf = 0x02;
};

template <typename T, int alloc>
struct mmapPopulate : public mmapPopulate_base
{
    using value_type = T;
  
    mmapPopulate() = default;
    constexpr mmapPopulate(const mmapPopulate<T, alloc>&) noexcept {}

    // Why I need this to compile for God sake???
    template<typename T1>
    struct rebind
    {
        using other = mmapPopulate<T1, alloc>;
    };
 
    // *** Workhorse ***
    [[nodiscard]] T* allocate(std::size_t n) 
    {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
          throw std::bad_array_new_length();
       
        // Delegating small alloc to stdlib
        if (n*sizeof(T) < libcTreshold) {
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
        if (n*sizeof(T) < libcTreshold) {
            return free(p);
        }
        munmap(p, n*sizeof(T));
    }
};

// Helper struct managing chunks of anonymous, mapped memory
struct memChunk {
    memChunk* next;
    std::size_t pgSize;
    memChunk(std::size_t _pgSize) : next(0), pgSize(_pgSize){}
    static std::atomic<memChunk*> head;

    static std::size_t mem2pages(std::size_t size) { 
        return (size-1) / page_size + 1;
    }

    static void put(memChunk* chunk)
    {
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
        while(ret && !head.compare_exchange_weak(ret, ret->next,
                std::memory_order_acquire))
        ;
        if (ret) {
            ret->next = nullptr;
        }
        return ret;
    }
};

// Allocator preserving memory between allocations 
// This should rather be implemented insid glibc alloc()

template <class T>
struct mmapPreserve
{
    typedef T value_type;
   
    mmapPreserve() = default;
    template <class U> constexpr mmapPreserve(const mmapPreserve<U>&) noexcept {}
   
    template<typename T1>
    struct rebind
    {
        using other = mmapPreserve<T1>;
    };
  
    [[nodiscard]] T* allocate(std::size_t n) 
    {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length();
    
        std::size_t pgNeeded = n ? memChunk::mem2pages(n * sizeof(T)) : 1;
    
        memChunk* retVal = memChunk::get();
        // If we need new memory from system
        if (retVal == nullptr) {
            mmapPopulate<T, mmapPopulate_base::popNone> alloc;
            return alloc.allocate(n);
        } 
        // Chunk too big, need to return tail to the queue
        assert((std::size_t)retVal % page_size == 0);
        if (pgNeeded < retVal->pgSize) {
            memChunk* mc = retVal + pgNeeded * page_size;
            mc->pgSize = retVal->pgSize - pgNeeded;
            retVal->pgSize = pgNeeded;
            memChunk::put(mc);
        }
        // Chunk ideal (maybe after trimming)
        if (pgNeeded == retVal->pgSize) {
            auto p = (T*)retVal;
            return p;
        }
        // We need to grow. Add address space after our chunk (may move mapping)
        void* pv = mremap(retVal, retVal->pgSize * page_size, pgNeeded * page_size, 
                MREMAP_MAYMOVE);
        retVal = static_cast<memChunk*>(pv);
        if (pv == MAP_FAILED) {
            perror("mmapPreserve");
            throw std::bad_alloc();
        }
        // Let's start collecting chunks
        assert(pgNeeded > retVal->pgSize);
        pgNeeded -= retVal->pgSize;
        memChunk* mc = retVal + retVal->pgSize * page_size;
        while (pgNeeded) {
            memChunk* mc1 = memChunk::get();
            // Chunk too big, return tail to queue
            if (pgNeeded < mc1->pgSize) {
                memChunk* mc2 = mc1 + pgNeeded * page_size;
                mc2->pgSize = mc1->pgSize - pgNeeded;
                mc1->pgSize = pgNeeded;
                memChunk::put(mc2);
            }
            // Smaller or equal, just attach at the end
            void* pv = mremap(mc1, mc1->pgSize * page_size, mc1->pgSize * page_size, 
                MREMAP_MAYMOVE | MREMAP_FIXED, mc);
            mc1 = static_cast<memChunk*>(pv);
            if (pv == MAP_FAILED) {
                perror("mmapPreserve");
                throw std::bad_alloc();
            }
            assert(pgNeeded >= mc1->pgSize);
            pgNeeded -= mc1->pgSize; 
            mc = mc1 + mc1->pgSize * page_size;
        }
        auto p =(T*)retVal;
        return p;
    }
 
    void deallocate(T* p, std::size_t n) noexcept 
    {
        memChunk* mc = (memChunk*)p;
        mc->pgSize = n ? memChunk::mem2pages(n * sizeof(T)) : 1;
        memChunk::put(mc);
        //munmap(p, pages * page_size);
    }
};

} // namespace v_allocator
