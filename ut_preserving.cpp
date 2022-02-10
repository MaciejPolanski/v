#include <iostream>

#include "memory_maps.h"
#include "v_allocator.h"

using std::cout;
using v_allocator::page_size;
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


struct blob {
    char b[v_allocator::libcTreshold];
};

void testSmoke()
{
    v_allocator::mmapPreserve<struct blob> a;

    printMaps.multiLine();
    cout << "\nAllocation of " << sizeof(blob) / page_size << " page(s) object\n";
    struct blob* pBlob = a.allocate(1);
    printMaps.multiLine();
    printChunks();
    cout << "\nReleasing that object\n";
    a.deallocate(pBlob,1);
    printMaps.multiLine();
    printChunks();

}

int main()
{
    printMaps.init();
    cout << "Page size is: " << v_allocator::page_size << "\n";
    testSmoke();
}
