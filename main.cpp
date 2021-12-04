#include <iostream>
#include <array>
#include <vector>
#include <chrono>
#include <iomanip>

#include <sys/resource.h>
#include "perf_event/proc_statm.h"

using std::cout;
using std::setw;
using std::to_string;

const int elemN{100000};      // How many element push into vector
const std::size_t vecN{ 10};  // How many pararell vectors in each step 
static_assert(vecN > 0);
struct blob {                 // Block-Of-Bytes to be used 
    unsigned char blob[1024];
};

struct oneLog{
    std::size_t size;
    std::size_t capacity;
    void* ptr;

    //oneLog(): cntResize(0), cntMoves(0){}
};
std::array<oneLog, elemN> logs;

template<typename T_vector, std::size_t M>
int test(int N,                                // N is # of push_backs
         std::array<T_vector, M>& testVectors, // Vectors can be preinitialized 
         std::array<oneLog, elemN>& logs)    // Per-loop measurments
{
    for(int i =0; i < N; ++i){
        for(auto& v: testVectors){
            v.emplace_back(blob{});
        }
        logs[i].size = testVectors[0].size();
        logs[i].capacity = testVectors[0].capacity();
        logs[i].ptr = &*testVectors[0].begin();
    }
    return 1;
}

std::string mem2str(long m)
{
    if ( abs(m / 1024 / 1024 / 1024 / 10) > 0 )
        return to_string(m/1024/1024/1024) + " GB";
    if ( abs(m / 1024 / 1024 / 10) > 0 )
        return to_string(m/1024/1024) + " MB";
    if ( abs(m / 1024 / 10) > 0 )
        return to_string(m/1024) + " KB";
    return to_string(m) + "  B";
}

struct p2s_t {
    const long page_size = 4096;
    std::string operator()(long m) { return mem2str(m * page_size);}
};
p2s_t p2s{};

void showLog(int testN, std::array<oneLog, elemN>& logs)
{
    cout << "1st vector (size/capa/buffer_addr/addr_change):\n";
    void *oldPtr{0};
    for(int i = 0; i < testN; ++i){
        auto& log = logs[i];
        if (oldPtr == log.ptr && i < (testN - 1))
            continue;
        long ptrdiff = (char*)log.ptr - (char*)oldPtr;
        // cout << setw(2) << i << ", ";
        cout << setw(8) << log.size << ", " << setw(10) << mem2str(sizeof(blob) * log.capacity) << "  ";
        cout << log.ptr << " diff " << setw(10) << mem2str(ptrdiff) <<"\n";
        oldPtr = log.ptr;
    }
}

struct cMeasurments{
    std::chrono::steady_clock clk;
    std::chrono::steady_clock::time_point clkS, clkE;
    long minFlt;

    void start(){
        rusage us;
        getrusage(RUSAGE_SELF, &us);
        minFlt = us.ru_minflt; 
        clkS = clk.now();
    }

    void stop(){
        clkE = clk.now();
        rusage us;
        getrusage(RUSAGE_SELF, &us);
        ProcStatm ps;
        getProcStatm(&ps);
        cout << "\n";
        //cout <<  "max rss: "  << mem2str(us.ru_maxrss * 1024);
        cout << "vms: " << p2s(ps.size) << " rss: " << p2s(ps.resident);
        cout <<  " page fault: "  << us.ru_minflt - minFlt;
        std::chrono::milliseconds mili;
        mili  = std::chrono::duration_cast<std::chrono::milliseconds>(clkE - clkS);
        cout << " t: " << mili.count() << " ms";
        cout << "\n";
    }
};

void testVector()
{
    cMeasurments m;
    std::array<std::vector<blob>, vecN> vectors1;

    cout << "Data size: " << mem2str(sizeof(blob) * elemN * vecN) << ", ";
    cout << elemN << " elements of size " << sizeof(blob) << " in " << vecN << " vectors\n";

    cout << "\nEmpty std::vector,";
    int testN{elemN};
    m.start();
    test(testN, vectors1, logs);
    m.stop();
    showLog(testN, logs);

    // Reused vectors
    cout << "\nReused std::vector,";
    for(auto& v: vectors1) v.clear();
    m.start();
    test(testN, vectors1, logs);
    m.stop();
    showLog(testN, logs);

    // Reserved vectors
    cout << "\nReserved std::vector,";
    std::array<std::vector<blob>, vecN> v;
    vectors1.swap(v);
    for(auto& v: vectors1) v.reserve(testN);
    m.start();
    test(testN, vectors1, logs);
    m.stop();
    showLog(testN, logs);
    std::array<std::vector<blob>, vecN>{}.swap(v);  // Kill v to not hang in memory
    // Reserved after mem release

    cout << "\nSwap free before use of std::vector(s),";
    m.start();
    std::array<std::vector<blob>, vecN>{}.swap(vectors1);
    m.stop();
    cout << "\nReserved std::vector, ";
    m.start();
    for(auto& v: vectors1) v.reserve(testN);
    test(testN, vectors1, logs);
    m.stop();
    showLog(testN, logs);
}

int main(int argc, char** argv)
{
    testVector();
}

//   Kernel memory page allocation calls:
// mm/memory.c do_anonymous_page
// include/linux/highmem.h:alloc_zeroed_user_highpage_movable(struct vm_area_struct *vma,
// arch/ia64/include/asm/page.h:#define __alloc_zeroed_user_highpage(movableflags, vma, vaddr)
// include/linux/gfp.h:#define alloc_page_vma(gfp_mask, vma, addr)
//                             alloc_pages(gfp_t gfp_mask, unsigned int order)
//                             alloc_pages_node(numa_node_id(), gfp_mask, order);
//                             __alloc_pages_node
//                             __alloc_pages
//                             __alloc_pages_nodemask(gfp_mask, order, preferred_nid, NULL);
// mm/page_alloc.c:__alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order, int preferred_nid,
//                             get_page_from_freelist
//                             prep_new_page
//                             post_alloc_hook
// include/linux/mm.h:static inline bool want_init_on_alloc(gfp_t flags)
//                                       ***return flags & __GFP_ZERO;
// page_alloc.c:static void kernel_init_free_pages(struct page *page, int numpages)
// include/linux/highmem.h:static inline void clear_highpage(struct page *page)
// include/asm-generic/page.h:#define clear_page(page)	memset((page), 0, PAGE_SIZE)
//

