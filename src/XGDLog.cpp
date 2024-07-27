#include <iomanip>
#include <chrono>

#include "XGDLog.h"

LogLevel XGDLog::current_level = Normal; // Set default level

void XGDLog::print_progress(uint64_t processed, uint64_t total) 
{
    static bool should_print = (current_level != Error);
    static auto last_update_time = std::chrono::steady_clock::now();

    if (!should_print) 
    {
        return;
    }

    const int bar_width = 50;
    float progress = static_cast<float>(processed) / total;

    auto now = std::chrono::steady_clock::now();
    auto duration_since_last_update = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time);

    if (duration_since_last_update.count() < 100 && processed < total) 
    {
        return;
    }

    last_update_time = now;

    std::cout << "\r[";

    int pos = static_cast<int>(bar_width * progress);
    for (int i = 0; i < bar_width; ++i) 
    {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }

    std::cout << "] " << std::setw(6) << std::fixed << std::setprecision(2) << (progress * 100.0) << "%";
    std::cout.flush();

    if (processed >= total) 
    {
        std::cout << std::endl;
    }
}

// void print_hex(const char* buffer, std::size_t size) {
//     for (std::size_t i = 0; i < size; ++i) {
//         std::cout << std::hex << std::setw(2) << std::setfill('0') << (static_cast<unsigned int>(buffer[i]) & 0xFF) << " ";
//     }
//     std::cout << std::dec << std::endl;
// }