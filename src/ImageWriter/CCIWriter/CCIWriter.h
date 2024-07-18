#ifndef _CCI_WRITER_H_
#define _CCI_WRITER_H_

#include <cstdint>

#include "ImageReader/ImageReader.h"
#include "ImageWriter/ImageWriter.h"    
#include "Common/Utils.h"
#include "Formats/CCI.h"
#include "Formats/Xiso.h"
#include "AvlTree/AvlTree.h"
#include "XGD.h"

class CCIWriter : public ImageWriter {
public:
    CCIWriter(std::shared_ptr<ImageReader> image_reader, const ScrubType scrub_type, const bool allowed_media_patch);
    CCIWriter(const std::filesystem::path& in_dir_path, const bool allowed_media_patch);
    
    ~CCIWriter() override;

    std::vector<std::filesystem::path> convert(const std::filesystem::path& out_cci_path) override;

private:
    std::unique_ptr<AvlTree> avl_tree_{nullptr};
    std::shared_ptr<ImageReader> image_reader_{nullptr};

    ScrubType scrub_type_;
    bool allowed_media_patch_{true};

    std::filesystem::path out_filepath_base_;
    std::filesystem::path out_filepath_1_;
    std::filesystem::path out_filepath_2_;

    uint64_t prog_total_{0};
    uint64_t prog_processed_{0};

    void convert_to_cci(const bool scrub);
    void convert_to_cci_from_avl();

    void write_file_from_reader(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, AvlTree::Node& node);
    void write_file_from_dir(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, AvlTree::Node& node);

    void compress_and_write_sector(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, const char* in_buffer);
    void compress_and_write_sector_managed(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, const char* in_buffer);

    void finalize_out_file(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos);

    std::vector<std::filesystem::path> out_paths();
};

#endif // _CCI_WRITER_H_