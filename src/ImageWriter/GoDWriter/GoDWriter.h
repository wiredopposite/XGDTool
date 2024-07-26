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
#include "Formats/GoD.h"
#include "AvlTree/AvlTree.h"
#include "AvlTree/AvlIterator.h"
#include "ImageReader/ImageReader.h"
#include "ImageWriter/ImageWriter.h"
#include "TitleHelper/TitleHelper.h"

class GoDWriter : public ImageWriter {
public:
    GoDWriter(std::shared_ptr<ImageReader> image_reader, TitleHelper& title_helper, const ScrubType scrub_type);
    GoDWriter(const std::filesystem::path& in_dir_path, TitleHelper& title_helper);

    ~GoDWriter() override = default;

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

    std::shared_ptr<ImageReader> image_reader_{nullptr};
    std::filesystem::path in_dir_path_;
    TitleHelper& title_helper_;
    ScrubType scrub_type_{ScrubType::NONE};

    uint64_t prog_total_{0};
    uint64_t prog_processed_{0};

    //Either no or partial scrubbing
    std::vector<std::filesystem::path> write_data_files(const std::filesystem::path& out_data_directory, const bool scrub);

    //Full scrub/write from directory
    std::vector<std::filesystem::path> write_data_files_from_avl(AvlTree& avl_tree, const std::filesystem::path& out_data_directory);
    void write_iso_header(std::vector<std::unique_ptr<std::ofstream>>& out_files, AvlTree& avl_tree);
    void write_file_from_reader(std::vector<std::unique_ptr<std::ofstream>>& out_files, const AvlTree::Node& node);
    void write_file_from_directory(std::vector<std::unique_ptr<std::ofstream>>& out_files, const AvlTree::Node& node);
    
    //Finalize out files
    void write_hashtables(const std::vector<std::filesystem::path>& part_paths);
    SHA1Hash finalize_hashtables(const std::vector<std::filesystem::path>& part_paths);
    void write_live_header(const std::filesystem::path& out_header_path, const std::vector<std::filesystem::path>& out_part_paths, const SHA1Hash& final_mht_hash);

    //Helpers
    std::vector<std::filesystem::path> get_part_paths(const std::filesystem::path& out_directory, const uint32_t num_files);
    void write_padding_sectors(std::vector<std::unique_ptr<std::ofstream>>& out_files, const uint32_t start_sector, const uint32_t num_sectors, const char pad_byte); 
    Remap remap_sector(const uint64_t iso_sector);
    Remap remap_offset(const uint64_t iso_offset);
    uint64_t to_iso_offset(const uint64_t god_offset, const uint32_t god_file_index);
    uint32_t num_blocks(const uint64_t num_bytes);
    uint32_t num_parts(const uint32_t num_data_blocks);
    SHA1Hash compute_sha1(const char* data, const uint64_t size);
};

#endif // _GoD_WRITER_H_