#include <sys/mman.h>
#include <errno.h>
#include <atomic>
#include <type_traits>
  
// Size after which libc uses mmap. Esitimate that should be ok for now 
const std::size_t libcTreshold = 100 * 1024; 

// Allocator with physical mapping and zeroing of whole allocated memory
// Should be used e.g. during reserve()
typedef std::integral_constant<int,0x1> allocFull;
typedef std::integral_constant<int,0x2> allocHalf;

template <class T, typename alloc = allocFull>
struct mmap_alloc
{
  typedef T value_type;
  
  mmap_alloc() = default;
  constexpr mmap_alloc(const mmap_alloc<T, alloc>&) noexcept {}
 
  [[nodiscard]] T* allocate(std::size_t n) {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::bad_array_new_length();
   
    // Copycat of glibc malloc, that also allocates small mem on heap
    if (n*sizeof(T) < libcTreshold) {
        return static_cast<T*>(malloc(n*sizeof(T)));
    }
    void *pv;
    if (std::is_same<alloc, allocFull>::value) {
        // MAP_POPULATE for whole area
    	pv = mmap(NULL, n*sizeof(T), PROT_EXEC | PROT_READ | PROT_WRITE, 
            MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    }
    else {
        // MAP_POPULATE for 1/2 area
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

