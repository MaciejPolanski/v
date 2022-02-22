#include <iostream>
#include <cstring>
#include <cassert>
#include <vector>
#include <thread>

#include "v_allocator.h"

using std::cout;
using v_allocator::memChunk;
using v_allocator::page_size;

std::atomic<memChunk*> memChunk::head;

void printChunks()
{
    v_allocator::memChunk *head = v_allocator::memChunk::head.exchange(0);
    std::cout << "Chunks: ";

    v_allocator::memChunk *ch = head;
    while (ch) {
        std::cout << ch->pgSize << ", ";
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
    blob a,b,c;
    memset(&a, 0x0badf00d, sizeof(a));
    memset(&b, 0x0badf00d, sizeof(b));
    memset(&c, 0x0badf00d, sizeof(c));

    memChunk*const pA = new(&a)memChunk(1);
    memChunk*const pB = new(&b)memChunk(2);
    memChunk*const pC = new(&c)memChunk(3);
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

    printChunks();
    while (memChunk::get())
    ;
    printChunks();
}

memChunk pest(1);

void ABA_pest()
{
    memChunk *pA, *pB;
    // Here main must call cmp_ex(A, A->next==B) and be swapped
    while ((pA = memChunk::get()) && (pB = memChunk::get())) {
        assert(pA != &pest);
        assert(pB != &pest);
        pB->next = &pest;
        memChunk::put(pA);
        // Here main may confirm A and set head to B
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    cout << " pest_quit ";
}

void testABA()
{
    constexpr std::size_t size = 16 * 1024 * 1024;
    cout << "ABA test initializaton...\n";
    std::vector<memChunk> v;
    v.resize(size, memChunk{1});
    for (auto &a : v) {
        memChunk::put(&a);
    }    

    cout << "Start test...";
    std::thread th(ABA_pest); 
    memChunk *pA;
    while ((pA = memChunk::get())) {
        assert(pA != &pest);
    }
    th.join();
    cout << "ABA test end\n"; 
}

int main()
{
    cout << "UnitTest memChunk, page size is: " << page_size << "\n";
    testSmoke();
    //testABA(); uncatchable anyway without intentional changes in code, like sleeps added
}
