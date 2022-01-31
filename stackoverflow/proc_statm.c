#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "proc_statm.h"

/* https://stackoverflow.com/questions/1558402/memory-usage-of-current-process-in-c/7212248#7212248 */
void getProcStatm(ProcStatm *result) {
    const char* statm_path = "/proc/self/statm";
    FILE *f = fopen(statm_path, "r");
    if(!f) {
        perror(statm_path);
        abort();
    }
    if(7 != fscanf(
        f,
        "%lu %lu %lu %lu %lu %lu %lu",
        &(result->size),
        &(result->resident),
        &(result->share),
        &(result->text),
        &(result->lib),
        &(result->data),
        &(result->dt)
    )) {
        perror(statm_path);
        abort();
    }
    fclose(f);
}

