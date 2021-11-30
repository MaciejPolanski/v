#include <iostream>
#include <array>
#include <vector>
#include <chrono>
#include <iomanip>

#include "perf_event/perf_event.h"

using std::cout;
using std::setw;
using std::to_string;

const int elemN{100000};      // How many element push into vector
const std::size_t vecN{ 10};  // How many pararell vectors in each step 
static_assert(vecN > 0);
struct blob {                   // Block-Of-Bytes to be used 
    unsigned char blob[1024];
};

struct oneLog{
    std::size_t size;
    std::size_t capacity;
    void* ptr;

    //oneLog(): cntResize(0), cntMoves(0){}
};
std::array<oneLog, elemN> logs;

template<typename T_vector, std::size_t M>
int test(int N,                                // N is # of push_backs
         std::array<T_vector, M>& testVectors, // Vectors can be preinitialized 
         std::array<oneLog, elemN>& logs)    // Per-loop measurments
{
    for(int i =0; i < N; ++i){
        for(auto& v: testVectors){
            v.emplace_back(blob{});
        }
        logs[i].size = testVectors[0].size();
        logs[i].capacity = testVectors[0].capacity();
        logs[i].ptr = &*testVectors[0].begin();
    }
    return 1;
}

std::string mem2str(long m)
{
    if ( abs(m / 1024 / 1024 / 1024) > 0 )
        return to_string(m/1024/1024/1024) + " GB";
    if ( abs(m / 1024 / 1024) > 0 )
        return to_string(m/1024/1024) + " MB";
    if ( abs(m / 1024) > 0 )
        return to_string(m/1024) + " KB";
    return to_string(m) + "  B";
}

void showLog(int testN, std::array<oneLog, elemN>& logs)
{
    cout << "1st vector (size/capa/buffer_addr/addr_change):\n";
    void *oldPtr{0};
    for(int i = 0; i < testN; ++i){
        auto& log = logs[i];
        if (oldPtr == log.ptr)
            continue;
        long ptrdiff = (char*)log.ptr - (char*)oldPtr;
        // cout << setw(2) << i << ", ";
        cout << setw(8) << log.size << ", " << setw(10) << mem2str(sizeof(blob) * log.capacity) << "  ";
        cout << log.ptr << " diff " << setw(10) << mem2str(ptrdiff) <<"\n";
        oldPtr = log.ptr;
    }
}

struct cMeasurments{
    int fdPF;
    std::chrono::steady_clock clk;
    std::chrono::steady_clock::time_point clkS, clkE;

    cMeasurments(){ fdPF = page_faults_init(); }

    void start(){
        page_faults_start(fdPF);
        clkS = clk.now();
    }

    void stop(){
        clkE = clk.now();
        long f = page_faults_stop(fdPF);
        std::chrono::milliseconds mili;
        mili  = std::chrono::duration_cast<std::chrono::milliseconds>(clkE - clkS);
        cout << "elapsed: " << mili.count() << "ms, page faults: " << f << "\n";
    }
};

int main(int argc, char** argv)
{
    cMeasurments m;
    std::array<std::vector<blob>, vecN> vectors1;

    cout << "Data size: " << mem2str(sizeof(blob) * elemN * vecN) << ", ";
    cout << elemN << " elements of size " << sizeof(blob) << " in " << vecN << " vectors\n";

    cout << "\nEmpty std::vector, ";
    int testN{elemN};
    m.start();
    test(testN, vectors1, logs);
    m.stop();
    showLog(testN, logs);

    // Reused vectors
    cout << "\nReused std::vector, ";
    for(auto& v: vectors1) v.clear();
    m.start();
    test(testN, vectors1, logs);
    m.stop();
    showLog(testN, logs);

    // Reserved vectors
    cout << "\nReserved std::vector, ";
    std::array<std::vector<blob>, vecN> v;
    vectors1.swap(v);
    for(auto& v: vectors1) v.reserve(testN);
    m.start();
    test(testN, vectors1, logs);
    m.stop();
    showLog(testN, logs);
}

