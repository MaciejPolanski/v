#include <iostream>
#include <array>
#include <vector>
#include <chrono>
#include <iomanip>

using std::cout;
using std::setw;
using std::to_string;

const int elemNum{ 10000};      // How many element push into vector
const std::size_t vecNum{100};  // How many pararell vectors in each step 
static_assert(vecNum > 0);
struct blob {                   // Block-Of-Bytes to be used 
    unsigned char blob[1024];
};

struct oneLog{
    std::size_t size;
    std::size_t capacity;
    void* ptr;

    //oneLog(): cntResize(0), cntMoves(0){}
};
std::array<oneLog, elemNum> logs;

template<typename T_vector, std::size_t M>
int test(int N,                                // N is # of push_backs
         std::array<T_vector, M>& testVectors, // Vectors can be preinitialized 
         std::array<oneLog, elemNum>& logs)    // Per-loop measurments
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

void showLog(int testN, std::array<oneLog, elemNum>& logs)
{
    cout << setw(8) << "size()" << "  " << setw(8) << "capacity()  &*.begin()     diff_to_previous\n";
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
    cout << "Data size: " << mem2str(sizeof(blob) * testN * vecNum) << ", ";
    cout << elemNum << " elements of size " << sizeof(blob) << " in " << vecNum << " vectors\n";
}

int main(int argc, char** argv)
{
    std::array<std::vector<blob>, vecNum> vectors1;

    cout << "Empty std::vector, one row per reallocation\n";
    int testN{elemNum};
    std::chrono::steady_clock clk;
    auto clkS = clk.now();
    test(testN, vectors1, logs);
    auto clkE = clk.now();

    std::chrono::milliseconds mili;
    mili  = std::chrono::duration_cast<std::chrono::milliseconds>(clkE - clkS);
    showLog(testN, logs);
    cout << "Elapsed: " << mili.count() << "ms\n";

    // Reused vectors
    cout << "Reused std::vector, no reallocations - one row\n";
    for(auto& v: vectors1) v.clear();
    clkS = clk.now();
    test(testN, vectors1, logs);
    clkE = clk.now();

    mili  = std::chrono::duration_cast<std::chrono::milliseconds>(clkE - clkS);
    showLog(testN, logs);
    cout << "Elapsed: " << mili.count() << "ms\n";

    // Reserved vectors
    cout << "Reserved std::vector\n";
    std::array<std::vector<blob>, vecNum> v;
    vectors1.swap(v);
    for(auto& v: vectors1) v.reserve(testN);
    clkS = clk.now();
    test(testN, vectors1, logs);
    clkE = clk.now();

    mili  = std::chrono::duration_cast<std::chrono::milliseconds>(clkE - clkS);
    showLog(testN, logs);
    cout << "Elapsed: " << mili.count() << "ms\n";
}

