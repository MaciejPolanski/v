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
#include "perf_event/proc_statm.h"
#include "mmap_popul_alloc.h"

using std::cout;
using std::setw;
using std::to_string;

const int         N_pushes{100000};// # of pushes into vector
const std::size_t M_vectors{  5};  // # of pararell vectors (to slow down process) 
static_assert(M_vectors > 0);
struct blob {                      // Block-Of-Bytes to be pushed into vectors
    unsigned char blob[1024];
};

// String fromating for bytes
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

// PFN (Page Frame Number) to bytes
struct p2s_t {
    const long page_size = 4096;
    std::string operator()(long m) { return mem2str(m * page_size);}
};
p2s_t p2s{};

// Formatting 48bit adresses in two parts
struct addr{
    uintptr_t l;
    addr(uintptr_t _l): l(_l){} 
    addr(void * v): l(reinterpret_cast<uintptr_t>(v)){}

    friend std::ostream& operator<<(std::ostream &o, addr a)
    {
        const uint bits{24};
        const uintptr_t lo_mask = std::numeric_limits<uintptr_t>::max() >> (sizeof(uintptr_t) * 8 - bits);
        o << "0x";
        o << setw(bits/4) << std::hex << (a.l >> bits) << ":";
        o << std::setfill('0') << setw(bits/4) << (a.l & lo_mask) << std::setfill(' ');
        o << std::dec;
        return o;
    }
};

// Logging internal state of vector after push_back
struct stateOfVector{
    std::size_t size;
    std::size_t capacity;
    void* ptr;
};
using tStatesOfVector =  std::array<stateOfVector, N_pushes>;

void printStatesOfVector(tStatesOfVector vectorStates)
{
    cout << "1st vector (size/capa/buffer_addr/addr_change):\n";
    void *oldPtr{0};
    for(int i = 0; i < N_pushes; ++i){
        auto& log = vectorStates[i];
        if (oldPtr == log.ptr && i < (N_pushes- 1))
            continue;
        long ptrdiff = (char*)log.ptr - (char*)oldPtr;
        // cout << setw(2) << i << ", ";
        cout << setw(8) << log.size << ", " << setw(10) << mem2str(sizeof(blob) * log.capacity) << "  ";
        cout << addr{log.ptr} << " diff " << setw(10) << mem2str(ptrdiff) <<"\n";
        oldPtr = log.ptr;
    }
}

// Prints chunks of process memory allocated with mmap
class cMMap {
    // Standard uninteresting process chunks
    std::set<uintptr_t> dontshow;

    // Parser enumarating through chunks
    template <typename F>
    void walker(F f)
    {
        std::ifstream is{"/proc/self/maps"};
        std::string s;
        while (std::getline(is, s)) {
            // 7ffda19f6000-7ffda1a17000 rw-p 00000000 00:00 0 [stack]
            std::istringstream buf{s};
            uintptr_t mmStart, mmEnd;
            char dash;
            buf >> std::hex >> mmStart >> dash >> std::hex >> mmEnd;
            std::array<std::string, 5> ss;
            std::for_each(ss.begin(), ss.end(), [&](std::string& s1){ buf >> s1;});
            // Always skip mapped files
            if (ss[2] != "00:00") 
                continue;   
            // Skip non-dynamic
            if (dontshow.find(mmStart) != dontshow.end())
                continue;
            // Stack grows downwards, so needs special rule for exclusion
            if (ss[4] == "[stack]")
                continue;
            f(mmStart, mmEnd, ss[4]);
        };
    }

    public:
    // Print multi-lines
    void lines() {
        cout << "Anonimous mmaps: ";
        walker([](uintptr_t mmStart, uintptr_t mmEnd, std::string s) { 
            cout << "\n[" << std::hex << addr{mmStart} << " - " << std::hex << addr{mmEnd} << " "; 
            cout << setw(8) << mem2str(mmEnd - mmStart) << s; 
        });
        cout << "\n";
    }
    // Print one-line
    void oneLine() {
        cout << "Anonimous mmaps: ";
        walker([](uintptr_t mmStart, uintptr_t mmEnd, std::string s)
                { cout << mem2str(mmEnd - mmStart) << ", "; });
        cout << "\n";
    }
    // Collects memory adresses to not be listed later
    void init() {
        dontshow.clear();
        //cout << "Skipping mmaps: ";
        walker([&](uintptr_t mmStart, uintptr_t mmEnd, std::string s){
            dontshow.insert(mmStart);
            //cout << addr{mmStart} << " ";
        });
        //cout << "\n";
    }
};
cMMap showMMap{};

// Counters before/after tests
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
        cout <<  " page fault: "  << us.ru_minflt - minFlt;
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

void testVectors()
{
    cMeasurments m;
    tStatesOfVector logs;
    std::array<std::vector<blob>, M_vectors> vectors1;

    // Absoulutely standard use of vector   
    cout << "\n[Pushing one-by-one into std::vector(s)                ]";
    m.start();
    test(vectors1, logs);
    m.stop();
    printStatesOfVector(logs);
    showMMap.oneLine();

    // Reused clear() vectors
    cout << "\n[Reusing into std::vector cleared with clear()         ]";
    for(auto& v: vectors1) v.clear();
    m.start();
    test(vectors1, logs);
    m.stop();
    printStatesOfVector(logs);
    showMMap.oneLine();

    // MAP_POPULATE allocator
    cout << "\n[Pushing into vector with mmap MAP_POPULATE            ]";
    std::array<std::vector<blob, mmap_alloc<blob>>, M_vectors> vmmap;
    m.start();
    test(vmmap, logs);
    m.stop();
    printStatesOfVector(logs);
    showMMap.oneLine();
    cout << "After swap-free of MAP_POPULATE vector:\n";
    realFree(vmmap);
    showMMap.oneLine();
    
    // MAP_POPULATE half allocator
    cout << "\n[Pushing into vector with mmap 1/2 MAP_POPULATE        ]";
    std::array<std::vector<blob, mmap_alloc<blob, allocHalf>>, M_vectors> vHalfMmap;
    m.start();
    test(vHalfMmap, logs);
    m.stop();
    //printStatesOfVector(logs);
    realFree(vHalfMmap);
}

void testReservedVectors()
{
    cMeasurments m;
    tStatesOfVector logs;

    cout << "\n[+----------------- reserved vectors -----------------+]";

    // Reserved vectors
    cout << "\n[New, reserved std::vector                             ]";
    std::array<std::vector<blob>, M_vectors> vectors1;
    for(auto& v: vectors1) v.reserve(N_pushes);
    m.start();
    test(vectors1, logs);
    m.stop();
    printStatesOfVector(logs);
    showMMap.lines();

    // True memory release by swap
    cout << "\n[Real free (swap) of std::vector(s)                    ]";
    m.start();
    realFree(vectors1);
    m.stop();
    showMMap.oneLine();

    cout << "\n[Again reserved vector (not faster -> no mem reuse)    ]";
    m.start();
    for(auto& v: vectors1) v.reserve(N_pushes);
    test(vectors1, logs);
    m.stop();
    //printStatesOfVector(logs);
    showMMap.oneLine();
    realFree(vectors1);

    cout << "\n[Time excluding reserve() (proof reserve does nothing) ]";
    for(auto& v: vectors1) v.reserve(N_pushes);
    m.start();
    test(vectors1, logs);
    m.stop();
    //printStatesOfVector(logs);
    showMMap.oneLine();

     // MAP_POPULATE allocator 
    cout << "\n[MAP_POPULATE, mmap allocator                          ]";
    std::array<std::vector<blob, mmap_alloc<blob>>, M_vectors> vmmap_r;
    m.start();
    for(auto& v:vmmap_r) v.reserve(N_pushes);
    test(vmmap_r, logs);
    m.stop();
    //printStatesOfVector(logs);
    realFree(vmmap_r);

    // MAP_POPULATE allocator, count only inserting
    cout << "\n[MAP_POPULATE, excluding reserve() time                ]";
    for(auto& v:vmmap_r) v.reserve(N_pushes);
    m.start();
    test(vmmap_r, logs);
    m.stop();
    //printStatesOfVector(logs);
    
    // MAP_POPULATE half-allocator 
    cout << "\n[1/2 MAP_POPULATE, mmap allocator                      ]";
    std::array<std::vector<blob, mmap_alloc<blob, allocHalf>>, M_vectors> vHmmap_r;
    m.start();
    for(auto& v:vHmmap_r) v.reserve(N_pushes);
    test(vHmmap_r, logs);
    m.stop();
    //printStatesOfVector(logs);
    realFree(vHmmap_r);

    // MAP_POPULATE half-allocator 
    cout << "\n[1/2 MAP_POPULATE, excluding reserve() time            ]";
    for(auto& v:vHmmap_r) v.reserve(N_pushes);
    m.start();
    test(vHmmap_r, logs);
    m.stop();
    //printStatesOfVector(logs);
    realFree(vHmmap_r);
}

int main(int argc, char** argv)
{
    cout << "+--Data size: " << mem2str(sizeof(blob) * N_pushes * M_vectors) << ", ";
    cout << N_pushes << " elements of size " << sizeof(blob) << " in " << M_vectors << " vectors--+\n";
    showMMap.init();

    testVectors();
    testReservedVectors();

    cout << "\nBefore exit ";
    showMMap.oneLine();
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

