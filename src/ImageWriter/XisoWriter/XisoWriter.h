#ifndef _XISO_WRITER_H_
#define _XISO_WRITER_H_

#include <cstdint>
#include <filesystem>

#include "SplitFStream/SplitFStream.h"

#include "XGD.h"
#include "AvlTree/AvlTree.h"
#include "ImageReader/ImageReader.h"
#include "ImageWriter/ImageWriter.h"

class XisoWriter : public ImageWriter 
{
public:
    XisoWriter(std::shared_ptr<ImageReader> image_reader, ScrubType scrub_type, const bool split);
    XisoWriter(const std::filesystem::path& in_dir_path, const bool split);
    
    ~XisoWriter() override = default;

    std::vector<std::filesystem::path> convert(const std::filesystem::path& out_xiso_path) override;

private:
    std::shared_ptr<ImageReader> image_reader_{nullptr};
    std::filesystem::path in_dir_path_;

    ScrubType scrub_type_{ScrubType::NONE};
    bool split_{false};

    uint64_t total_bytes_{0};
    uint64_t bytes_processed_{0};

    std::vector<std::filesystem::path> convert_to_xiso(const std::filesystem::path& out_xiso_path, const bool scrub);
    std::vector<std::filesystem::path> convert_to_xiso_from_avl(AvlTree& avl_tree, const std::filesystem::path& out_xiso_path);

    void write_tree(AvlTree::Node* node, split::ofstream* out_file, int depth);
    void write_entry(AvlTree::Node* node, split::ofstream* out_file, int depth);
    void write_file_from_reader(AvlTree::Node* node, split::ofstream* out_file, int depth);
    void write_file_from_directory(AvlTree::Node* node, split::ofstream* out_file, int depth);
    void write_header(split::ofstream& out_file, AvlTree& avl_tree);

    void pad_to_modulus(split::ofstream& out_file, uint32_t modulus, char pad_byte);
};

#endif // _XISO_WRITER_H_