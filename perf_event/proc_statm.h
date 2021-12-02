/*
   /proc/[pid]/statm
          Provides information about memory usage, measured in pages.  
          The columns are:

              size       total program size
                         (same as VmSize in /proc/[pid]/status)
              resident   resident set size
                         (same as VmRSS in /proc/[pid]/status)
              share      shared pages (from shared mappings)
              text       text (code)
              lib        library (unused in Linux 2.6)
              data       data + stack
              dt         dirty pages (unused in Linux 2.6)
*/

typedef struct {
    unsigned long size,resident,share,text,lib,data,dt;
} ProcStatm;

/* https://stackoverflow.com/questions/1558402/memory-usage-of-current-process-in-c/7212248#7212248 */
void getProcStatm(ProcStatm *result);

