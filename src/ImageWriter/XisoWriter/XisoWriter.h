#ifndef _XISO_WRITER_H_
#define _XISO_WRITER_H_

#include <cstdint>
#include <filesystem>

#include "split_fstream.h"

#include "XGD.h"
#include "AvlTree/AvlTree.h"
#include "ImageReader/ImageReader.h"
#include "ImageWriter/ImageWriter.h"

class XisoWriter : public ImageWriter {
public:
    XisoWriter(std::shared_ptr<ImageReader> image_reader, ScrubType scrub_type, const bool split, const bool allowed_media_patch);
    XisoWriter(const std::filesystem::path& in_dir_path, const bool split, const bool allowed_media_patch);
    
    ~XisoWriter();

    std::vector<std::filesystem::path> convert(const std::filesystem::path& out_xiso_path);

private:
    std::unique_ptr<AvlTree> avl_tree_{nullptr};
    std::shared_ptr<ImageReader> image_reader_{nullptr};

    ScrubType scrub_type_{ScrubType::NONE};
    bool split_{false};
    bool allowed_media_patch_{false}; // Only used for full scrub rn

    split::ofstream out_file_;

    uint64_t total_bytes_{0};
    uint64_t bytes_processed_{0};

    std::vector<std::filesystem::path> convert_to_xiso(const std::filesystem::path& out_xiso_path, const bool scrub);
    std::vector<std::filesystem::path> convert_to_xiso_from_avl(const std::filesystem::path& out_xiso_path);

    void write_tree(AvlTree::Node* node, ImageReader* image_reader, int depth);
    void write_entry(AvlTree::Node* node, void* context, int depth);
    void write_file(AvlTree::Node* node, ImageReader* image_reader, int depth);
    void write_file_dir(AvlTree::Node* node, void* context, int depth);

    void write_header_from_avl();
};

#endif // _XISO_WRITER_H_