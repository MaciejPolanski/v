# v
Deliberations on efficiency of std::vector on x64 Linux.  
New allocator proposed and benchmarked.

## mm :: allocPopulate
An allocator that can speed up operations by pre-populating memory pages with physical frames. Policies:
- *mm::allocPopulate_base::popFull* - physicall frames are assigned to whole allocted memory
- *mm::allocPopulate_base::popHalf* - lower half of memory have frames assigned 
- *mm::allocPopulate_base::popNone* - no frames assigned, policy just for comprsion with others

## ~~mm :: allocPreserve~~ (doesn't work)
~~An allocator that reuses deallocated memory. Prevents memory fragmentation with memory remapping.~~

## Files
- `mm_alloc.h` New allocators
- `main.cpp` Benchmark of allocators

## Allocator use
```
using allocPopulateFull = mm::allocPopulate<T, mm::allocPopulate_base::popFull>;
using allocPopulateHalf = mm::allocPopulate<T, mm::allocPopulate_base::popHalf>;
using allocPopulateNone = mm::allocPopulate<T, mm::allocPopulate_base::popNone>;
```
