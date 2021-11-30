#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <linux/perf_event.h>
//#include <linux/hw_breakpoint.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>

#include "perf_event.h"

// Based on https://stackoverflow.com/questions/23302763/measure-page-faults-from-a-c-program
static long perf_event_open(struct perf_event_attr *hw_event,
                pid_t pid,     // pid{0} cpu{-1} measures the calling process/thread on any CPU.  
                int cpu,
                int group_fd,
                unsigned long flags) {
  int ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
            group_fd, flags);
  return ret;
}

int page_faults_init()
{ 
  struct perf_event_attr pe_attr_page_faults;

  memset(&pe_attr_page_faults, 0, sizeof(pe_attr_page_faults));
  pe_attr_page_faults.size = sizeof(pe_attr_page_faults);
  pe_attr_page_faults.type =   PERF_TYPE_SOFTWARE;
  pe_attr_page_faults.config = PERF_COUNT_SW_PAGE_FAULTS;
  pe_attr_page_faults.disabled = 1;
  pe_attr_page_faults.exclude_kernel = 1;

  int page_faults_fd = perf_event_open(&pe_attr_page_faults, 0, -1, -1, 0);
  if (page_faults_fd == -1) {
    printf("perf_event_open failed for page faults: %s\n", strerror(errno));
    return -1;
  }
  return page_faults_fd;
}

void page_faults_start(int page_faults_fd)
{
  // Start counting
  ioctl(page_faults_fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(page_faults_fd, PERF_EVENT_IOC_ENABLE, 0);
}

uint64_t page_faults_stop(int page_faults_fd)
{
  // Stop counting and read value
  ioctl(page_faults_fd, PERF_EVENT_IOC_DISABLE, 0);
  uint64_t page_faults_count;
  read(page_faults_fd, &page_faults_count, sizeof(page_faults_count));
  return page_faults_count;
}

#if 0 
// !sudo ./a.out
#include <stdlib.h>
int main(int argc, char**argch)
{ 
  int fd = page_faults_init();

  page_faults_start(fd);
  char* pch = malloc(1024*1024);
  uint64_t faults =  page_faults_stop(fd);
  
  page_faults_start(fd);
  for(int i = 0; i<1024*1024;++i) pch[i]=1;
  uint64_t faults2 =  page_faults_stop(fd);

  printf("Faults: %li %li\n", faults, faults2); // Faults: 4 256
}
#endif

