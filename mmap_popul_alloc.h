#include <sys/mman.h>
#include <errno.h>
  
// Size after which libc uses mmap. Esitimate that should be ok for now 
const std::size_t libcTreshold = 100 * 1024; 

template <class T>
struct mmap_alloc
{
  typedef T value_type;
  
  mmap_alloc() = default;
  template <class U> constexpr mmap_alloc(const mmap_alloc<U>&) noexcept {}
 
  [[nodiscard]] T* allocate(std::size_t n) {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::bad_array_new_length();
   
    if (n*sizeof(T) < libcTreshold) {
        return static_cast<T*>(malloc(n*sizeof(T)));
    }
    void* pv = mmap(NULL, n*sizeof(T), PROT_EXEC | PROT_READ | PROT_WRITE, 
        MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
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
        free(p);
    }
    munmap(p, n*sizeof(T));
  }
};

template <class T>
struct mmap_half_alloc
{
  typedef T value_type;
 
  mmap_half_alloc() = default;
  template <class U> constexpr mmap_half_alloc(const mmap_alloc<U>&) noexcept {}
 
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

