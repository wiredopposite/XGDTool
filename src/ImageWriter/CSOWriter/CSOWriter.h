#ifndef _CSO_WRITER_H_
#define _CSO_WRITER_H_

#include <cstdint>
#include <vector>
#include <fstream>
#include <filesystem>

#include <lz4frame.h>

#include "ImageReader/ImageReader.h"
#include "ImageWriter/ImageWriter.h"
#include "Formats/CSO.h"
#include "Formats/Xiso.h"
#include "AvlTree/AvlTree.h"
#include "XGD.h"

class CSOWriter : public ImageWriter {
public:
    CSOWriter(std::shared_ptr<ImageReader> image_reader, const ScrubType scrub_type);
    CSOWriter(const std::filesystem::path& in_dir_path);
    
    ~CSOWriter();

    std::vector<std::filesystem::path> convert(const std::filesystem::path& out_cso_path);

private:
    const int ALIGN_B = 1 << CSO::INDEX_ALIGNMENT;
    const int ALIGN_M = ALIGN_B - 1;

    std::shared_ptr<ImageReader> image_reader_{nullptr};
    std::filesystem::path in_dir_path_; 

    ScrubType scrub_type_{ScrubType::NONE};

    std::filesystem::path out_filepath_base_;
    std::filesystem::path out_filepath_1_;
    std::filesystem::path out_filepath_2_;

    size_t lz4f_max_size_;
    LZ4F_compressionContext_t lz4f_ctx_;
    LZ4F_preferences_t lz4f_prefs_ = { 
        { LZ4F_default, LZ4F_blockIndependent, LZ4F_noContentChecksum, LZ4F_frame, 0ULL, 0U, LZ4F_noBlockChecksum }, 
        12, 
        1, 
        0u, 
        { 0u, 0u, 0u } 
    };

    uint64_t prog_total_{0};
    uint64_t prog_processed_{0};

    void init_lz4f_context();

    void convert_to_cso(const bool scrub);
    void convert_to_cso_from_avl(AvlTree& avl_tree);
    void write_header(std::ofstream& out_file, std::vector<uint32_t>& block_index, AvlTree& avl_tree);
    void write_file_from_reader(std::ofstream& out_file, std::vector<uint32_t>& block_index, AvlTree::Node& node);
    void write_file_from_directory(std::ofstream& out_file, std::vector<uint32_t>& block_index, AvlTree::Node& node);

    void compress_and_write_sector(std::ofstream& out_file, std::vector<uint32_t>& block_index, const char* in_buffer);
    void compress_and_write_sector_managed(std::ofstream& out_file, std::vector<uint32_t>& block_index, const char* in_buffer);

    void finalize_out_files(std::ofstream& out_file, std::vector<uint32_t>& block_index);

    void write_padding_sectors(std::ofstream& out_file, std::vector<uint32_t>& block_index, const uint32_t num_sectors, const char pad_byte);
    void pad_to_modulus(std::ofstream& out_file, uint32_t modulus, char pad_byte);

    std::vector<std::filesystem::path> out_paths();
};

#endif // _CSO_WRITER_H_