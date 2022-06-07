// Copyright Maciej Polanski
#include <iostream>
#include <array>
#include <vector>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <functional>
#include <set>

#include <sys/resource.h>
#include <malloc.h>

#include "stackoverflow/proc_statm.h"
#include "memory_maps.h"
#include "mm_alloc.h"

using std::cout;
using std::setw;
using std::to_string;


const int         N_pushes{5000};// # of pushes into vector
const std::size_t M_vectors{100};  // # of pararell vectors (to slow down process) 
static_assert(M_vectors > 0);
struct blob {                      // Block-Of-Bytes to be pushed into vectors
    unsigned char blob[10 * 1024];
};

constexpr bool color = false;

constexpr const char* NC = color ? "\033[0m" : "";
constexpr const char* MT = color ? "\033[1;33m" : "";
constexpr const char* ST = color ? "\033[0;33m" : "";

cPrintMemoryMaps printMaps{};
// PFN (Page Frame Number) to string
pfn2s_t p2s{mm::page_size};
// Logging internal state of vector after push_back
struct stateOfVector{
    std::size_t size;
    std::size_t capacity;
    void* ptr;
};
using tStatesOfVector =  std::array<stateOfVector, N_pushes>;

void printStatesOfVector(tStatesOfVector vectorStates)
{
    cout << "Vector:Size     Capa  Buffer addr.       Addr. change\n";
    void *oldPtr{0};
    for(int i = 0; i < N_pushes; ++i){
        auto& log = vectorStates[i];
        if (oldPtr == log.ptr && i < (N_pushes- 1))
            continue;
        long ptrdiff = (char*)log.ptr - (char*)oldPtr;
        // cout << setw(2) << i << ", ";
        cout << setw(8) << log.size << ", " << setw(10) << mem2str(sizeof(blob) * log.capacity) << "  ";
        cout << addr{log.ptr} << "   +/- " << setw(9) << mem2str(ptrdiff) <<"\n";
        oldPtr = log.ptr;
    }
}

// Counters before/after tests
struct cStats{
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
        std::chrono::milliseconds mili;
        mili  = std::chrono::duration_cast<std::chrono::milliseconds>(clkE - clkS);
        cout << " t: " << mili.count() << " ms";
        cout << "\n";

        rusage us;
        getrusage(RUSAGE_SELF, &us);
        ProcStatm ps;
        getProcStatm(&ps);
        //cout <<  "max rss: "  << mem2str(us.ru_maxrss * 1024);
        cout << "vms: " << p2s(ps.size) << " rss: " << p2s(ps.resident);
        cout <<  " page_fault: "  << us.ru_minflt - minFlt;
        cout << "\n";
    }
};

template<class T>
void realFree(T& t)
{
    T temp{};
    t.swap(temp);
}

// Testing workhorse
template<typename T_vector>
int test(std::array<T_vector, M_vectors>& testVectors, // Vectors can be given preallocated
         tStatesOfVector& vectorStates)                // Returned per-loop measurments
{
    for(int i =0; i < N_pushes; ++i){
        for(auto& v: testVectors){
            v.emplace_back(blob{});
        }
        vectorStates[i].size = testVectors[0].size();
        vectorStates[i].capacity = testVectors[0].capacity();
        vectorStates[i].ptr = &*testVectors[0].begin();
    }
    return 1;
}

using allocPopulateFull = mm::allocPopulate<blob, mm::allocPopulate_base::popFull>;
using allocPopulateHalf = mm::allocPopulate<blob, mm::allocPopulate_base::popHalf>;
using allocPopulateNone = mm::allocPopulate<blob, mm::allocPopulate_base::popNone>;

void testVectors()
{
    cStats m;
    tStatesOfVector logs;
    std::array<std::vector<blob>, M_vectors> vectors1;

    cout << MT << "\n+------------- Non reserved vectors (growth observation) ---------+" << NC;

    // Absoulutely standard use of vector   
    cout << ST << "\n[Pushing one-by-one into std::vector(s)                ]" << NC;
    m.start();
    test(vectors1, logs);
    m.stop();
    printStatesOfVector(logs);
    printMaps.oneLine();

     // Reused clear() vectors
    cout << ST << "\n[Reusing into std::vector cleared with clear()         ]" << NC;
    for(auto& v: vectors1) v.clear();
    m.start();
    test(vectors1, logs);
    m.stop();
    printStatesOfVector(logs);
    printMaps.oneLine();
    realFree(vectors1);

   // MAP_POPULATE allocator
    cout << ST << "\n[Pushing into vector with allocPopulate popFull        ]" << NC;
    std::array<std::vector<blob, allocPopulateFull>, M_vectors> vmmap;
    m.start();
    test(vmmap, logs);
    m.stop();
    printStatesOfVector(logs);
    printMaps.oneLine();
    cout << "After swap-free:\n";
    realFree(vmmap);
    printMaps.oneLine();
    
    // MAP_POPULATE half allocator
    cout << ST << "\n[Pushing into vector with allocPopulate popHalf        ]" << NC;
    std::array<std::vector<blob, allocPopulateHalf>, M_vectors> vHalfMmap;
    m.start();
    test(vHalfMmap, logs);
    m.stop();
    printStatesOfVector(logs);
    printMaps.oneLine();
    realFree(vHalfMmap);
}

void testReservedVectors()
{
    cStats m;
    tStatesOfVector logs;

    cout << MT << "\n+----------------------- Reserved vectors ------------------------+" << NC;

    // Reserved vectors
    cout << ST << "\n[New, reserved std::vector                             ]" << NC;
    std::array<std::vector<blob>, M_vectors> vectors1;
    for(auto& v: vectors1) v.reserve(N_pushes);
    m.start();
    test(vectors1, logs);
    m.stop();
    printStatesOfVector(logs);
    printMaps.multiLine();

    // True memory release by swap
    //cout << "\n[Real free (swap) of std::vector(s)                    ]" << NC;
    //m.start();
    realFree(vectors1);
    //m.stop();
    //printMaps.multiLine();

/*    cout << ST << "\n[Again reserved vector (not faster -> no mem reuse)    ]" << NC;
    m.start();
    for(auto& v: vectors1) v.reserve(N_pushes);
    test(vectors1, logs);
    m.stop();
    printStatesOfVector(logs);
    printMaps.oneLine();
    realFree(vectors1);*/

    cout << ST << "\n[Time excluding reserve() (proof reserve does nothing) ]" << NC;
    for(auto& v: vectors1) v.reserve(N_pushes);
    m.start();
    test(vectors1, logs);
    m.stop();
    //printStatesOfVector(logs);
    //printMaps.oneLine();
    realFree(vectors1);

     // MAP_POPULATE allocator 
    cout << ST << "\n[allocPopulate popFull                                 ]" << NC;
    std::array<std::vector<blob, allocPopulateFull>, M_vectors> vmmap_r;
    m.start();
    for(auto& v:vmmap_r) v.reserve(N_pushes);
    test(vmmap_r, logs);
    m.stop();
    //printStatesOfVector(logs);
    printMaps.oneLine();
    realFree(vmmap_r);

    // MAP_POPULATE allocator, count only inserting
    cout << ST << "\n[allocPopulate popFull, excluding reserve() time       ]" << NC;
    for(auto& v:vmmap_r) v.reserve(N_pushes);
    m.start();
    test(vmmap_r, logs);
    m.stop();
    //printStatesOfVector(logs);
    realFree(vmmap_r);
    
    // MAP_POPULATE half-allocator 
    cout << ST << "\n[allocPopulate popHalf                                 ]" << NC;
    std::array<std::vector<blob, allocPopulateHalf>, M_vectors> vHmmap_r;
    m.start();
    for(auto& v:vHmmap_r) v.reserve(N_pushes);
    test(vHmmap_r, logs);
    m.stop();
    //printStatesOfVector(logs);
    printMaps.oneLine();
    realFree(vHmmap_r);

    // MAP_POPULATE half-allocator 
    cout << ST << "\n[allocPopulate popHalf, excluding reserve() time       ]" << NC;
    for(auto& v:vHmmap_r) v.reserve(N_pushes);
    m.start();
    test(vHmmap_r, logs);
    m.stop();
    //printStatesOfVector(logs);
    realFree(vHmmap_r);

    // MAP_POPULATE none-allocator 
    cout << ST << "\n[allocPopulate popNone (for comparsion)                ]" << NC;
    std::array<std::vector<blob, allocPopulateNone>, M_vectors> vecNone;
    m.start();
    for(auto& v:vecNone) v.reserve(N_pushes);
    test(vecNone, logs);
    m.stop();
    //printStatesOfVector(logs);
    printMaps.oneLine();
    realFree(vecNone);

    // MAP_POPULATE half-allocator 
    cout << "\n[allocPopulate popNone, excluding reserve()            ]" << NC;
    for(auto& v:vecNone) v.reserve(N_pushes);
    m.start();
    test(vecNone, logs);
    m.stop();
    //printStatesOfVector(logs);
    realFree(vecNone);
}

int main(int argc, char** argv)
{
    cout << ST << "+--Data size: " << mem2str(sizeof(blob) * N_pushes * M_vectors) << ", ";
    cout << N_pushes << " elements of size " << sizeof(blob) << " in " << M_vectors << " vectors--+\n" << NC;
    printMaps.init();
    // To equalize mmap threshold of malloc() and allocators
    mallopt(M_MMAP_THRESHOLD, mm::libcThreshold);
    // To absorb mystical initial slowdown obscuring results  
    std::array<std::vector<blob>, M_vectors> vectors1;
    tStatesOfVector logs;
    test(vectors1, logs);
    realFree(vectors1);

    testReservedVectors();
    testVectors();

    cout << ST << "\nFinished, left " << NC;
    printMaps.multiLine();
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

