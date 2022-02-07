// Copyright Maciej Polanski

#include <iostream>
#include <iomanip>
#include <limits>
#include <set>
#include <fstream>
#include <algorithm>

using std::cout;
using std::setw;
using std::to_string;

// String fromating for bytes
std::string mem2str(long m)
{
    if ( abs(m / 1024 / 1024 / 1024 / 10) > 0 )
        return to_string(m/1024/1024/1024) + " GB";
    if ( abs(m / 1024 / 1024 / 10) > 0 )
        return to_string(m/1024/1024) + " MB";
    if ( abs(m / 1024 / 10) > 0 )
        return to_string(m/1024) + " KB";
    return to_string(m) + "  B";
}

// Page Frame Number to string
struct pfn2s_t {
    uintptr_t page_size;
    pfn2s_t(uintptr_t _page_size):page_size(_page_size){}    
    std::string operator()(long m) { return mem2str(m * page_size);}
};

// Formatting 48bit adresses in two parts
struct addr{
    uintptr_t l;
    addr(uintptr_t _l): l(_l){} 
    addr(void * v): l(reinterpret_cast<uintptr_t>(v)){}

    friend std::ostream& operator<<(std::ostream &o, addr a)
    {
        const uint bits{24};
        const uintptr_t lo_mask = std::numeric_limits<uintptr_t>::max() >> (sizeof(uintptr_t) * 8 - bits);
        o << "0x";
        o << setw(bits/4) << std::hex << (a.l >> bits) << ":";
        o << std::setfill('0') << setw(bits/4) << (a.l & lo_mask) << std::setfill(' ');
        o << std::dec;
        return o;
    }
};

// Prints chunks of process memory allocated with mmap
class cPrintMemoryMaps {
    // Standard uninteresting process chunks
    std::set<uintptr_t> dontshow;

    // Parser enumarating through chunks
    template <typename F>
    void walker(F f)
    {
        std::ifstream is{"/proc/self/maps"};
        std::string s;
        while (std::getline(is, s)) {
            // 7ffda19f6000-7ffda1a17000 rw-p 00000000 00:00 0 [stack]
            std::istringstream buf{s};
            uintptr_t mmStart, mmEnd;
            char dash;
            buf >> std::hex >> mmStart >> dash >> std::hex >> mmEnd;
            std::array<std::string, 5> ss;
            std::for_each(ss.begin(), ss.end(), [&](std::string& s1){ buf >> s1;});
            // Always skip mapped files
            if (ss[2] != "00:00") 
                continue;   
            // Skip non-dynamic
            if (dontshow.find(mmStart) != dontshow.end())
                continue;
            // Stack grows downwards, so needs special rule for exclusion
            if (ss[4] == "[stack]")
                continue;
            f(mmStart, mmEnd, ss[4]);
        };
    }

    public:
    // Print multi-lines
    void multiLine() {
        cout << "Anonimous mmaps: ";
        walker([](uintptr_t mmStart, uintptr_t mmEnd, std::string s) { 
            cout << "\n[" << std::hex << addr{mmStart} << " - " << std::hex << addr{mmEnd} << " "; 
            cout << setw(8) << mem2str(mmEnd - mmStart) << s; 
        });
        cout << "\n";
    }
    // Print one-line
    void oneLine() {
        cout << "Anonimous mmaps: ";
        walker([](uintptr_t mmStart, uintptr_t mmEnd, std::string s)
                { cout << mem2str(mmEnd - mmStart) << ", "; });
        cout << "\n";
    }
    // Collects memory adresses to not be listed later
    void init() {
        dontshow.clear();
        //cout << "Skipping mmaps: ";
        walker([&](uintptr_t mmStart, uintptr_t mmEnd, std::string s){
            // Keep heap, sometimes vectors fits there
            if (s != "[heap]")
                dontshow.insert(mmStart);
            //cout << addr{mmStart} << " ";
        });
        //cout << "\n";
    }
};

