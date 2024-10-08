#ifndef _ZAR_EXTRACTOR_H_
#define _ZAR_EXTRACTOR_H_

#include <cstdint>
#include <filesystem>
#include <atomic>

#include "zarchive/zarchivereader.h"

class ZARExtractor
{
public:
    ZARExtractor(const std::filesystem::path& in_zar_path);
    ~ZARExtractor() = default;

    void extract(const std::filesystem::path& out_dir_path);
    void list_files();
    
    void cancel_processing() { write_cancel_flag_ = true; }
    void pause_processing() { write_pause_flag_ = true; }
    void resume_processing() { write_pause_flag_ = false; }

private:
    std::atomic<bool> write_cancel_flag_{false};
    std::atomic<bool> write_pause_flag_{false};

    uint64_t prog_total_{0};
    uint64_t prog_processed_{0};

    std::filesystem::path in_zar_path_;

    bool extract_recursive(ZArchiveReader* reader, std::string src_path, std::filesystem::path out_dir_path);
    bool extract_file(ZArchiveReader* reader, std::string_view src_path, const std::filesystem::path& path);
    void calculate_total_recursive(ZArchiveReader* reader, std::string src_path, bool list_files = true);
    void check_status_flags();
};

#endif // _ZAR_EXTRACTOR_H_