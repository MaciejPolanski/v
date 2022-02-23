#include <vector>
#include <iostream>
#include <array>
#include <thread>
#include <cstring>
#include <condition_variable>

#include "memory_maps.h"
#include "mm_alloc.h"

using std::cout;
using mm::page_size;
using mm::memChunk;
std::atomic<mm::memChunk*> mm::memChunk::head;

cPrintMemoryMaps printMaps{};
pfn2s_t p2s(page_size);

std::size_t tstIndex = 1024;
const char tstMark = 42;

struct blob {
    char buf[mm::libcTreshold];
};

void printChunks()
{
    int nTotal = 0;
    int pgTotal = 0;
    mm::memChunk *head = mm::memChunk::head.exchange(0);
    std::cout << "Chunks: \n";

    mm::memChunk *ch = head;
    while (ch) {
        std::cout << addr{ch} << " " << p2s(ch->pgSize) << "\n";
        ++nTotal;
        pgTotal += ch->pgSize;
        ch = ch->next;
    }
    std::cout << "Total " << nTotal << " chunks, " << p2s(pgTotal) << "\n";
    std::cout << "\n";
    mm::memChunk::put(head);
}

void testSmoke()
{
    mm::allocPreserve<struct blob> a;
    cout << "\n+--- Basic allocations tests ---+\n";
    printMaps.multiLine();
    cout << "\n";

    cout << "*** Allocation of " << mem2str(sizeof(blob)) << " ***\n";
    cout << "Such mmap should appear\n";
    blob* pBlob = a.allocate(1);
    reinterpret_cast<memChunk*>(pBlob)->buffer[tstIndex] = tstMark;   // minor fault test
    printMaps.multiLine();
    printChunks();

    cout << "*** Releasing that object ***\n";
    cout << "mmap should be kept, but added to chunks list\n";
    a.deallocate(pBlob,1);
    printMaps.multiLine();
    printChunks();

    cout << "*** Allocation of " << mem2str(sizeof(blob)) << " ***\n";
    cout << "Memory should be taken from chunks, no new mmap, old kept\n";
    pBlob = a.allocate(1);
    assert(reinterpret_cast<memChunk*>(pBlob)->buffer[tstIndex] == tstMark);
    printMaps.multiLine();
    printChunks();
 
    cout << "\n*** Allocation of " << mem2str(sizeof(blob)) << " ***\n";
    cout << "New mmap or old increased\n";
    blob* pBlob2 = a.allocate(1);
    printMaps.multiLine();
    printChunks();

    cout << "*** Releasing both objects ***\n";
    cout << "Two chunks expected\n";
    a.deallocate(pBlob,1);
    a.deallocate(pBlob2,1);
    printMaps.multiLine();
    printChunks();
    
    cout << "*** Releasing chunks ***\n";
    cout << "Chunks and mmaps should dissapear\n";
    mm::memChunk::release();
    printMaps.multiLine();
    printChunks();
}

void testGrowmap()
{
    blob* pBlob;
    mm::allocPreserve<struct blob> a;

    cout << "\n+--- Reallocation tests ---+\n";
    printMaps.multiLine();
    cout << "\n";

    cout << "*** Allocation of " << mem2str(sizeof(blob)) << " ***\n";
    cout << "Such mmap should appear\n";
    pBlob = a.allocate(1);
    reinterpret_cast<memChunk*>(pBlob)->buffer[tstIndex] = tstMark;   // minor fault test
    printMaps.multiLine();
    printChunks();

    cout << "*** Releasing that object ***\n";
    cout << "mmap should be kept, but added to chunks list\n";
    a.deallocate(pBlob,1);
    printMaps.multiLine();
    printChunks();

    cout << "*** Allocation of " << mem2str(2 * sizeof(blob)) << " ***\n";
    cout << "Memory should be taken from chunk and mmap grow/relocate to 2x size\n";
    pBlob = a.allocate(2);
    assert(reinterpret_cast<memChunk*>(pBlob)->buffer[tstIndex] == tstMark);
    reinterpret_cast<memChunk*>(pBlob)->buffer[sizeof(blob) + tstIndex] = tstMark;
    printMaps.multiLine();
    printChunks();
 
    cout << "*** Releasing that object ***\n";
    cout << "2x mmap should be kept, but added to chunks list\n";
    a.deallocate(pBlob,2);
    printMaps.multiLine();
    printChunks();

    cout << "*** Allocation of " << mem2str(sizeof(blob)) << " ***\n";
    cout << "1x of memory should stay in chunks\n";
    pBlob = a.allocate(1);
    assert(reinterpret_cast<memChunk*>(pBlob)->buffer[tstIndex] == tstMark);
    printMaps.multiLine();
    printChunks();

    cout << "*** Releasing that object ***\n";
    cout << "mmap should be kept, but added to chunks list\n";
    a.deallocate(pBlob,1);
    printMaps.multiLine();
    printChunks();

    cout << "*** Allocation of " << mem2str(2 * sizeof(blob)) << " ***\n";
    cout << "Memory should be taken from both chunks, 2x size mmap may relocate\n";
    pBlob = a.allocate(2);
    assert(reinterpret_cast<memChunk*>(pBlob)->buffer[tstIndex] == tstMark);
    assert(reinterpret_cast<memChunk*>(pBlob)->buffer[sizeof(blob) + tstIndex] == tstMark);
    printMaps.multiLine();
    printChunks();
 
    cout << "*** Releasing that object ***\n";
    cout << "mmap should be kept, but added to chunks list\n";
    a.deallocate(pBlob,2);
    printMaps.multiLine();
    printChunks();

    cout << "*** Releasing chunks ***\n";
    cout << "Chunks and mmaps should dissapear\n";
    mm::memChunk::release();
    printMaps.multiLine();
    printChunks();
}

void testCheckerboard()
{
    mm::allocPreserve<blob> a;
    std::vector<blob*> va,vb;

    cout << "\n+--- Chessboard tests ---+\n";
    printMaps.multiLine();
    cout << "\n";

    cout << "*** Allocation of 20x" << mem2str(sizeof(blob)) << ", releasing every second ***\n";
    cout << "Expected mmaps in big isles, 10 free chunks \n";
    for (int i = 0; i < 20; ++i) {
        if (i%2 == 0) {
            va.emplace_back(a.allocate(1));
            (*va.rbegin())->buf[tstIndex] = tstMark;
        }
        else {
            vb.emplace_back(a.allocate(1));
            (*vb.rbegin())->buf[tstIndex] = tstMark + 1;
        }
    }
    for (auto& x : va) {
        a.deallocate(x, 1);
    }
    va.clear();
    printMaps.multiLine();
    printChunks();
 
    cout << "*** Allocation of 5x" << mem2str(2 * sizeof(blob)) << " ***\n";
    cout << "Expected old mmaps full of "<< mem2str(sizeof(blob)) << " holes, new in isle(s), same total amount\n";
    for (int i = 0; i < 5; ++i) {
        va.emplace_back(a.allocate(2));
        //assert((*va.rbegin())->buf[tstIndex] == tstMark);
        //assert((*va.rbegin())->buf[tstIndex] + sizeof(blob) == tstMark);
    }
    printMaps.multiLine();
    printChunks();

    cout << "*** Releasing chunks ***\n";
    cout << "Chunks and mmaps should dissapear\n";
    for (auto& x : va) {
        a.deallocate(x, 2);
    }
    for (auto& x : vb) {
        a.deallocate(x, 1);
    }
    mm::memChunk::release();
    printMaps.multiLine();
    printChunks();
}

constexpr int nThreads = 5;
constexpr int n = 15;
std::atomic<int> aStart{0};
std::atomic<int> aDealloc{0};
std::atomic<int> aAlloc{0};
std::atomic<int> aEnd{0};
//std::condition_variable cvStart;

void testThread(std::array<blob*, n>& aBlobs, int i)
{
    mm::allocPreserve<blob> a;
    // Allocation
    ++aStart;
    while(aStart.load() != nThreads + 1);
    for (auto& x : aBlobs) {
        x  = a.allocate(1);
        memset(x, i, sizeof(blob));
    }
    // Deallocation every second
    ++aDealloc;
    while(aDealloc.load() != nThreads + 1);
    for (int x = 1; x < n; x += 2) {
        a.deallocate(aBlobs[x], 1);
    }

    // Allocation double in size, but half in number
    ++aAlloc;
    while(aAlloc.load() != nThreads + 1);
    for (int x = 1; x < n / 2; x += 2) {
        aBlobs[x] = a.allocate(2);
        memset(aBlobs[x], 10 * i, 2 * sizeof(blob));
    }

    ++aEnd;
    while(aEnd.load() != nThreads + 1);
}

void testThreads()
{

    mm::allocPreserve<blob> a;
    
    std::array<std::array<blob*,n>, nThreads> allBlobs{};
    std::array<std::thread, nThreads> threads;

    cout << "\n+---      Chessboard threads test      ---+\n";
    printMaps.multiLine();
    cout << "\n";

    cout << "*** Allocation, threads: " << nThreads << " each alloc: " << n;
    cout << " blocks of size: " << mem2str(sizeof(blob)) << " ***\n";
    cout << "Expected mmaps in big isle(s)\n";

    for (int x = 0; x < nThreads; ++x) {
        threads[x] =  std::thread(testThread, std::ref(allBlobs[x]), x);
    }
    while(aStart.load() != nThreads);    // Sync all threads
    printMaps.init();                    // To hide threads stacks
    ++aStart;                            // Start allocations
    while(aDealloc.load() != nThreads);  // Wait for finish
    printMaps.multiLine();
    printChunks();

    cout << "*** Deallocation every second, threaded " << "\n";
    cout << "Expected mmaps as previously, but " << n / 2 * nThreads << " chunks avaliable\n";
    ++aDealloc;                          // Start Deallocations
    while(aAlloc.load() != nThreads);    // Wait for finish
    printMaps.multiLine();
    printChunks();

    cout << "*** Allocation of " << (n / 4 * nThreads) << " double sized blocks, threaded\n";
    cout << "Expected prevoius mmaps with holes, new isle(s), " << (n / 2) % 2 * nThreads << " chunks avaliable\n";
    ++aAlloc;                            // Start double allocations
    while(aEnd.load() != nThreads);      // Wait for finish
    printMaps.multiLine();
    printChunks();
    // Joining pollutes mmaps
    aEnd++;
    for (auto& x : threads)
        x.join(); 

    cout << "*** Releasing chunks ***\n";
    cout << "Chunks and mmaps should dissapear, but threads storages may appear\n";
    for (auto& b : allBlobs) 
        for (int x =0; x < n; ++x)
    {
        if (x % 2 == 0)
            a.deallocate(b[x], 1);
        else if (x < n / 2)
            a.deallocate(b[x], 2);
    }
    mm::memChunk::release();
    printMaps.multiLine();
    printChunks();
}

int main()
{
    printMaps.init();
    cout << "Unit test, manual, for mm::allocPreserving\n";
    cout << "Page size is: " << mm::page_size;
    cout << " Blob size: "<< mem2str(sizeof(blob)) << "\n";
    testSmoke();
    testGrowmap();
    testCheckerboard();
    testThreads();
    cout << "+---        End, memory status      ---+\n";
    printMaps.multiLine();
}
