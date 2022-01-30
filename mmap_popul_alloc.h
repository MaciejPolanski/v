#include <sys/mman.h>
#include <errno.h>
#include <atomic>
#include <type_traits>
  
// Size after which stdlib (mostlikely) uses mmap
const std::size_t libcTreshold = 100 * 1024; 

// Allocator that forces physical mapping (and zeroing) of memory
// Should be used e.g. during reserve()
const int allocFull = 0x01;
const int allocHalf = 0x02;

template <class T, int alloc = allocFull>
struct mmap_alloc
{
  typedef T value_type;
  
  mmap_alloc() = default;
  constexpr mmap_alloc(const mmap_alloc<T, alloc>&) noexcept {}

  // Why I need this to compile for God sake???
  template<typename _T1>
  struct rebind
  {
      typedef mmap_alloc<_T1> other;
  };
 
  // Workhorse
  [[nodiscard]] T* allocate(std::size_t n) 
  {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::bad_array_new_length();
   
    // Delegating small alloc to stdlib
    if (n*sizeof(T) < libcTreshold) {
        return static_cast<T*>(malloc(n*sizeof(T)));
    }
    void *pv;
    if (allocFull == alloc) {
        // MAP_POPULATE (physical mapping) for whole memory
    	pv = mmap(NULL, n*sizeof(T), PROT_EXEC | PROT_READ | PROT_WRITE, 
            MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    }
    else {
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
    perror("MMAP alloc");
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
/*
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
*/

