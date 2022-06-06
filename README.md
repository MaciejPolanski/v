# v
Deliberations on efficiency of std::vector on x64 Linux.  
New allocator proposed and benchmarked.

## mm :: allocPopulate
An allocator that can speed up operations by pre-populating memory pages with physical frames.
- Policies *mm::allocPopulate_base::popFull | popHalf | popNone*

## ~~mm :: allocPreserve~~ (doesn't work)
~~An allocator that reuses deallocated memory. Prevents memory fragmentation with memory remapping.~~

## Files
- `mm_alloc.h` New allocators
- `main.cpp` Benchmark of allocators
