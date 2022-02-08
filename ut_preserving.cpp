#include <iostream>

#include "memory_maps.h"
#include "v_allocator.h"

using std::cout;
std::atomic<v_allocator::memChunk*> v_allocator::memChunk::head;

struct blob {
    char b[v_allocator::libcTreshold];
};

cPrintMemoryMaps printMaps{};

void testSmoke()
{
    v_allocator::mmapPreserve<struct blob> a;

    printMaps.multiLine();
    struct blob* pBlob = a.allocate(1);
    printMaps.multiLine();
    a.deallocate(pBlob,1);
    printMaps.multiLine();
}

int main()
{
    printMaps.init();
    cout << "Page size is: " << v_allocator::page_size << "\n";
    testSmoke();
}
