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

const int elemN{100000};      // How many element push into vector
const std::size_t vecN{  5};  // How many pararell vectors in each step 
static_assert(vecN > 0);
struct blob {                 // Block-Of-Bytes to be used 
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
        const uintptr_t mask = std::numeric_limits<uintptr_t>::max() >> (sizeof(uintptr_t) * 8 - bits);
        o << "0x";
        o << setw(bits/4) << std::hex << (a.l >> bits) << ":";
        o << std::setfill('0') << setw(bits/4) << (a.l & mask) << std::setfill(' ');
        o << std::dec;
        return o;
    }
};

// Logging parameters of vector after push
struct oneLog{
    std::size_t size;
    std::size_t capacity;
    void* ptr;
};
std::array<oneLog, elemN> logs;

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
        cout << addr{log.ptr} << " diff " << setw(10) << mem2str(ptrdiff) <<"\n";
        oldPtr = log.ptr;
    }
}

// Showing mmap'ed memory
class cMMap {
    std::set<uintptr_t> dontshow;

    template <typename F>
    void walker(F f)
    {
        std::ifstream is{"/proc/self/maps"};
        std::string s;
        while (std::getline(is, s)) {
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
            f(mmStart, mmEnd, ss[4]);
        };
    }
    public:
    void lines() {
        cout << "Anonimous mmaps: ";
        walker([](uintptr_t mmStart, uintptr_t mmEnd, std::string s) { 
            cout << "\n" << std::hex << addr{mmStart} << " - " << std::hex << addr{mmEnd} << " "; 
            cout << setw(8) << mem2str(mmEnd - mmStart) << s; 
        });
        cout << "\n";
    }

    void oneLine() {
        cout << "Anonimous mmaps: ";
        walker([](uintptr_t mmStart, uintptr_t mmEnd, std::string s)
                { cout << mem2str(mmEnd - mmStart) << ", "; });
        cout << "\n";
    }
    // Gathers memory adresses to not be listed   
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
        rusage us;
        getrusage(RUSAGE_SELF, &us);
        ProcStatm ps;
        getProcStatm(&ps);
        //cout <<  "max rss: "  << mem2str(us.ru_maxrss * 1024);
        cout << "vms: " << p2s(ps.size) << " rss: " << p2s(ps.resident);
        cout <<  " page fault: "  << us.ru_minflt - minFlt;
        std::chrono::milliseconds mili;
        mili  = std::chrono::duration_cast<std::chrono::milliseconds>(clkE - clkS);
        cout << " t: " << mili.count() << " ms";
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
template<typename T_vector, std::size_t M>
int test(int N,                                // N is # of push_backs
         std::array<T_vector, M>& testVectors, // Vectors can be preinitialized 
         std::array<oneLog, elemN>& logs)      // Per-loop measurments
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

void testVectors()
{
    cMeasurments m;
    std::array<std::vector<blob>, vecN> vectors1;


    // Absoulutely standard use of vector   
    cout << "\nPushing one-by-one into std::vector(s)\n";
    int testN{elemN};
    m.start();
    test(testN, vectors1, logs);
    m.stop();
    showLog(testN, logs);
    showMMap.oneLine();

    // Reused clear() vectors
    cout << "\nReusing into std::vector cleared with clear()\n";
    for(auto& v: vectors1) v.clear();
    m.start();
    test(testN, vectors1, logs);
    m.stop();
    //showLog(testN, logs);
    //showMMap.oneLine();

    // MAP_POPULATE allocator
    cout << "\nPushing into vector with mmap MAP_POPULATE\n";
    std::array<std::vector<blob, mmap_alloc<blob>>, vecN> vmmap;
    m.start();
    test(testN, vmmap, logs);
    m.stop();
    showLog(testN, logs);
    showMMap.oneLine();
    cout << "Real free of MAP_POPULATE vector\n";
    realFree(vmmap);
    showMMap.oneLine();
    
    // MAP_POPULATE half allocator
    cout << "\nPushing into vector with mmap 1/2 MAP_POPULATE\n";
    std::array<std::vector<blob, mmap_alloc<blob, allocHalf>>, vecN> vHalfMmap;
    m.start();
    test(testN, vHalfMmap, logs);
    m.stop();
    //showLog(testN, logs);
    realFree(vHalfMmap);
}

void testReservedVectors()
{
    cMeasurments m;
    std::array<std::vector<blob>, vecN> vectors1;
    int testN{elemN};

    cout << "\n+----------------- reserved vectors -----------------+\n";

    // Reserved vectors
    cout << "\nNew, reserved std::vector\n";
    std::array<std::vector<blob>, vecN> vectors2;
    for(auto& v: vectors2) v.reserve(testN);
    m.start();
    test(testN, vectors2, logs);
    m.stop();
    showLog(testN, logs);
    showMMap.lines();

    // Reserved after mem release
    cout << "\nReal free (swap) of std::vector(s)\n";
    m.start();
    realFree(vectors1);
    realFree(vectors2);
    m.stop();
    showMMap.oneLine();

    cout << "\nAgain reserved std::vector (not faster -> no mem reuse)\n";
    m.start();
    for(auto& v: vectors1) v.reserve(testN);
    test(testN, vectors1, logs);
    m.stop();
    //showLog(testN, logs);
    showMMap.oneLine();

    // MAP_POPULATE allocator 
    cout << "\nMAP_POPULATE, mmap allocator\n";
    std::array<std::vector<blob, mmap_alloc<blob>>, vecN> vmmap_r;
    m.start();
    for(auto& v:vmmap_r) v.reserve(testN);
    test(testN, vmmap_r, logs);
    m.stop();
    //showLog(testN, logs);
    realFree(vmmap_r);

    // MAP_POPULATE allocator, count only inserting
    cout << "\nMAP_POPULATE, excluding reserve() time\n";
    for(auto& v:vmmap_r) v.reserve(testN);
    m.start();
    test(testN, vmmap_r, logs);
    m.stop();
    //showLog(testN, logs);
    
    // MAP_POPULATE half-allocator 
    cout << "\n1/2 MAP_POPULATE, mmap allocator\n";
    std::array<std::vector<blob, mmap_alloc<blob, allocHalf>>, vecN> vHmmap_r;
    m.start();
    for(auto& v:vHmmap_r) v.reserve(testN);
    test(testN, vHmmap_r, logs);
    m.stop();
    //showLog(testN, logs);
    realFree(vHmmap_r);

    // MAP_POPULATE half-allocator 
    cout << "\n1/2 MAP_POPULATE, excluding reserve() time\n";
    for(auto& v:vHmmap_r) v.reserve(testN);
    m.start();
    test(testN, vHmmap_r, logs);
    m.stop();
    //showLog(testN, logs);
    realFree(vHmmap_r);
}

int main(int argc, char** argv)
{
    cout << "+--Data size: " << mem2str(sizeof(blob) * elemN * vecN) << ", ";
    cout << elemN << " elements of size " << sizeof(blob) << " in " << vecN << " vectors--+\n";
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

