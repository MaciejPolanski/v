// Copyright Maciej Polanski

#include <sys/mman.h>
#include <errno.h>
#include <atomic>
#include <type_traits>

namespace v_allocator {
  
// Size after which stdlib (mostlikely) uses mmap
const std::size_t libcTreshold = 100 * 1024; 

// Allocator that forces instant physical mapping (thus zeroing) of memory
// Should be used e.g. during reserve()

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
        // MAP_POPULATE (physical mapping) for whole memory
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

// Helper class managing memory chunks
struct mmap_chunks {
    static std::atomic<void*> head;
};

// Allocator preserving memory between allocations 
// This should rather be implemented insid glibc alloc()

template <class T>
struct mmap_preserve_alloc
{
  typedef T value_type;
 
  mmap_preserve_alloc() = default;
  template <class U> constexpr mmap_preserve_alloc(const mmap_preserve_alloc<U>&) noexcept {}
 
  [[nodiscard]] T* allocate(std::size_t n) {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::bad_array_new_length();
 
    void* pv = mmap(NULL, n*sizeof(T), PROT_EXEC | PROT_READ | PROT_WRITE, 
        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    void* p2 = mmap(pv, n*sizeof(T)/2, PROT_EXEC | PROT_READ | PROT_WRITE, 
        MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    if (pv != MAP_FAILED && p2 != MAP_FAILED) {
      auto p = static_cast<T*>(pv);
      return p;
    }
    //std::cout << "MMAP error: 0x" << std::hex << errno << "\n";
    // 0x16 - invalid argument 
    perror("MMAP alloc");
    throw std::bad_alloc();
  }
 
  void deallocate(T* p, std::size_t n) noexcept {
    munmap(p, n*sizeof(T));
  }
};

} // namespace v_allocator
