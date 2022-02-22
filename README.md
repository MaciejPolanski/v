# v
Deliberations on efficiency of std::vector on x64 Linux

## v_allocator :: mmapPopulate
An allocator that can speed up operations by pre-populating memory pages with physical frames
- Policies *mmapPopulate_base::popFull | popHalf | popNone*

## v_allocator :: mmapPreserve
An allocator that reuses deallocated memory. Prevents memory fragmentation through the remapping operations.
