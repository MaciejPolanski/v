#include <iostream>
#include <array>
#include <vector>
#include <chrono>
#include <iomanip>

using std::cout;
using std::setw;

const int elemNum = 1000;      // How many element push into vector
const std::size_t vecNum = 50;  // How many vectors interaced
static_assert(vecNum > 0);
struct blob {                   // Block-Of-Bytes to be used 
    unsigned char blob[512];
};

const int logN = 41;           // How many inspections during test
static_assert(logN > 1);
struct oneLog{
    std::size_t id;
    std::size_t capacity;
    void* ptr;
    int cntResize;
    int cntMoves;

    oneLog(): cntResize(0), cntMoves(0){}
};
std::array<oneLog, logN> logs;

template<typename T_vector, std::size_t M>
int test(std::array<T_vector, M>& testVectors)
{
    int logCount = 0;
    const int logStep = elemNum / (logN -1);
    logs.fill({});
    void *oldPtr = &*testVectors[0].begin();

    for(int i =0; i < elemNum; ++i){
        for(auto& v: testVectors){
            v.emplace_back(blob{});
        }
        if (oldPtr != &*testVectors[0].begin()) {   
            ++logs[logCount].cntResize;
            logs[logCount].cntMoves += testVectors[0].size();
            oldPtr = &*testVectors[0].begin();
        }
        if (i+1 >= logStep * logCount) {
            logs[logCount].id = testVectors[0].size();
            logs[logCount].capacity = testVectors[0].capacity();
            logs[logCount].ptr = &*testVectors[0].begin();
            ++logCount;
        }
    }
    return 1;
}

int main(int argc, char** argv)
{
    std::array<std::vector<blob>, vecNum> vectors1;

    std::chrono::steady_clock clk;
    auto clkS = clk.now();
    test(vectors1);
    auto clkE = clk.now();

    std::chrono::milliseconds mili;
    mili  = std::chrono::duration_cast<std::chrono::milliseconds>(clkE - clkS);

    int totResizes{0};
    int totMoves{0};
    for(int i = 0; i < logN; ++i){
        totResizes += logs[i].cntResize;
        totMoves += logs[i].cntMoves;
        // cout << setw(2) << i << ", ";
        cout << setw(8) << logs[i].id << ", " << setw(8) << sizeof(blob) * logs[i].capacity / 1024 << " KB ";
        cout << logs[i].ptr << " " << setw(2) << totResizes <<  setw(8) << totMoves << "\n";
    }
    cout << elemNum << " elements of size " << sizeof(blob) << " in " << vecNum << " vectors\n";
    cout << "Data size: " << sizeof(blob) * elemNum * vecNum / 1024 / 1024 << " MB";
    cout << ", elapsed: " << mili.count() << "ms\n";
    //cout << decltype(dur)::period::num << ":" << decltype(dur)::period::den << "\n" ;;
}

