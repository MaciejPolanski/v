#include <iostream>
#include <cstring>
#include <cassert>

#include "v_allocator.h"

using std::cout;
using v_allocator::memChunk;
using v_allocator::page_size;

std::atomic<memChunk*> memChunk::head;

struct blob {
    char b[v_allocator::libcTreshold];
};


void testSmoke()
{
    blob a,b,c;
    memset(&a, 0x0badf00d, sizeof(a));
    memset(&b, 0x0badf00d, sizeof(b));
    memset(&c, 0x0badf00d, sizeof(c));

    memChunk*const pA = new(&a)memChunk(sizeof(a) / page_size);
    memChunk*const pB = new(&b)memChunk(sizeof(b) / page_size);
    memChunk*const pC = new(&c)memChunk(sizeof(c) / page_size);
    memChunk *pT;

    cout << "Adding one element\n";
    memChunk::put(pC);
    assert(memChunk::head == pC);
    assert(static_cast<memChunk*>(memChunk::head)->next == nullptr);
  
    cout << "Removing this one element\n";
    pT = memChunk::get();
    assert(pT == pC);
    assert(static_cast<memChunk*>(pC)->next == nullptr);
    assert(static_cast<memChunk*>(memChunk::head) == nullptr);

    cout << "Get from  empty\n";
    pT = memChunk::get();
    assert(pT == nullptr);
    assert(static_cast<memChunk*>(memChunk::head) == nullptr);

    cout << "Put two elements\n";
    memChunk::put(pC);
    memChunk::put(pB);
    assert(static_cast<memChunk*>(memChunk::head) == pB);
    assert(pB->next == pC);
    assert(pC->next == nullptr);
    
    cout << "Removing first\n";
    pT = memChunk::get();
    assert(pT == pB);
    assert(pB->next == nullptr);
    assert(static_cast<memChunk*>(memChunk::head) == pC);
    assert(pC->next == nullptr);
   
    cout << "Putting two elements at once\n";
    pA->next = pB;
    memChunk::put(pA);
    assert(static_cast<memChunk*>(memChunk::head) == pA);
    assert(pA->next == pB);
    assert(pB->next == pC);
    assert(pC->next == nullptr);
 
}

void testABA()
{
    
}

int main()
{
    cout << "UnitTest memChunk, page size is: " << page_size << "\n";
    testSmoke();
    testABA();
}
