// Wrapper for Linux syscall measuring page faults
// Use with 'sudo'

int page_faults_init();
void page_faults_start(int page_faults_fd);
uint64_t page_faults_stop(int page_faults_fd);

