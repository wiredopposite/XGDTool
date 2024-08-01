#include <iomanip>
#include <chrono>

#include "XGDLog.h"

LogLevel XGDLog::current_level = Normal;

#ifndef ENABLE_GUI

XGDLog& XGDLog::operator<<(Manip manip) 
{
    if (manip == Manip::Endl && should_log()) 
    {
        oss << std::endl;
        
        std::cerr << oss.str();
        oss.str("");  // Clear the stream after flushing
        oss.clear();
    }
    return *this;
}

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

#else // ENABLE_GUI

#include "GUI/MainFrame.h"

void XGDLog::print_progress(uint64_t processed, uint64_t total) 
{
    static auto last_update_time = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();
    auto duration_since_last_update = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time);

    if (duration_since_last_update.count() < 100 && processed < total) 
    {
        return;
    }

    last_update_time = now;

    MainFrame::update_progress_bar(processed, total);
}

XGDLog& XGDLog::operator<<(Manip manip) 
{
    if (manip == Manip::Endl && should_log()) 
    {
        oss << std::endl;
        MainFrame::update_status_field(oss.str());
        oss.str(""); 
        oss.clear();
    }
    return *this;
}

#endif // ENABLE_GUI