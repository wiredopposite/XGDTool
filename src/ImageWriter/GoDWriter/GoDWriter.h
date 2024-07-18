#ifndef _GoD_WRITER_H_
#define _GoD_WRITER_H_

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <memory>

#include <openssl/sha.h>

#include "XGD.h"
#include "ExeTool/ExeTool.h"
#include "Formats/GoD.h"
#include "AvlTree/AvlTree.h"
#include "ImageReader/ImageReader.h"
#include "ImageWriter/ImageWriter.h"

class GoDWriter : public ImageWriter {
public:
    GoDWriter(std::shared_ptr<ImageReader> image_reader, const ScrubType scrub_type, const bool allowed_media_patch);
    GoDWriter(const std::filesystem::path& in_dir_path, const bool allowed_media_patch);

    ~GoDWriter() override;

    std::vector<std::filesystem::path> convert(const std::filesystem::path& out_god_path) override;

private:
    struct SHA1Hash { 
        unsigned char hash[SHA_DIGEST_LENGTH]; 
    };
    static_assert(sizeof(SHA1Hash) == SHA_DIGEST_LENGTH, "SHA1Hash size mismatch");

    struct Remap {
        uint64_t offset;
        uint32_t file_index;
    };

    std::unique_ptr<AvlTree> avl_tree_{nullptr};
    std::shared_ptr<ImageReader> image_reader_{nullptr};
    std::filesystem::path in_dir_path_;

    std::vector<std::unique_ptr<std::ofstream>> out_files_;
    uint32_t current_out_file_{0};
    uint64_t current_dir_start_{0};
    uint64_t current_dir_position_{0};

    ScrubType scrub_type_{ScrubType::NONE};
    bool allowed_media_patch_{false};
    bool offline_mode_{false};

    uint64_t prog_total_{0};
    uint64_t prog_processed_{0};

    std::vector<std::filesystem::path> write_data_files(const std::filesystem::path& out_data_directory, const bool scrub);
    std::vector<std::filesystem::path> write_data_files_from_avl(const std::filesystem::path& out_data_directory);

    void write_header(AvlTree& avl_tree);
    void pad_to_file_modulus(const uint64_t& out_iso_size);

    void write_tree(AvlTree::Node* node, ImageReader* reader, int depth);
    void write_entry(AvlTree::Node* node, void* context, int depth);
    void write_file(AvlTree::Node* node, ImageReader* reader, int depth);
    void write_file_dir(AvlTree::Node* node, void* context, int depth);

    void write_hashtables(const std::vector<std::filesystem::path>& part_paths);
    SHA1Hash finalize_hashtables(const std::vector<std::filesystem::path>& part_paths);
    void write_live_header(const std::filesystem::path& out_header_path, const std::vector<std::filesystem::path>& out_part_paths, std::unique_ptr<ExeTool>& exe_tool, const SHA1Hash& final_mht_hash);

    Remap remap_sector(uint64_t iso_sector);
    Remap remap_offset(uint64_t iso_offset);
    uint64_t to_iso_offset(uint64_t god_offset, uint32_t god_file_index);
    void padded_seek(std::unique_ptr<std::ofstream>& out_file, uint64_t offset);
    SHA1Hash compute_sha1(const char* data, size_t size);
    std::string create_unique_name(const Xex::ExecutionInfo& xex_cert);

    template <typename T>
    void write_little_endian(std::ostream& os, T value);
};

#endif // _GoD_WRITER_H_