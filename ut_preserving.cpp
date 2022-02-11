#include <iostream>

#include "memory_maps.h"
#include "v_allocator.h"

using std::cout;
using v_allocator::page_size;
using v_allocator::memChunk;
std::atomic<v_allocator::memChunk*> v_allocator::memChunk::head;

cPrintMemoryMaps printMaps{};
pfn2s_t p2s(page_size);

void printChunks()
{
    v_allocator::memChunk *head = v_allocator::memChunk::head.exchange(0);
    std::cout << "Chunks: \n";

    v_allocator::memChunk *ch = head;
    while (ch) {
        std::cout << addr{ch} << " " << p2s(ch->pgSize) << "\n";
        ch = ch->next;
    }
    std::cout << "\n";

    v_allocator::memChunk::put(head);
}

std::size_t tstIndex = 1024;
const char tstMark = 42;

struct blob {
    char b[v_allocator::libcTreshold];
};

void testSmoke()
{
    v_allocator::mmapPreserve<struct blob> a;
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
    v_allocator::memChunk::release();
    printMaps.multiLine();
    printChunks();
}

void testGrowmap()
{
    blob* pBlob;
    v_allocator::mmapPreserve<struct blob> a;

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
    v_allocator::memChunk::release();
    printMaps.multiLine();
    printChunks();
}

int main()
{
    printMaps.init();
    cout << "Unit test, manual, for v_allocator::preserving\n";
    cout << "Page size is: " << v_allocator::page_size;
    cout << " Blob size: "<< mem2str(sizeof(blob)) << "\n";
    testSmoke();
    testGrowmap();
}
