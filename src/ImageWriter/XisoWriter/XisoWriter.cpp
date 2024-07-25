#include <algorithm>

#include "ImageWriter/XisoWriter/XisoWriter.h"
#include "AvlTree/AvlTree.h"

XisoWriter::XisoWriter(std::shared_ptr<ImageReader> image_reader, ScrubType scrub_type, const bool split) 
    : image_reader_(image_reader), scrub_type_(scrub_type), split_(split) {}

XisoWriter::XisoWriter(const std::filesystem::path& in_dir_path, const bool split) 
    : split_(split), in_dir_path_(in_dir_path) {}

std::vector<std::filesystem::path> XisoWriter::convert(const std::filesystem::path& out_xiso_path) 
{
    create_directory(out_xiso_path.parent_path());

    if (image_reader_ && scrub_type_ == ScrubType::FULL) //Full scrub ISO
    {
        AvlTree avl_tree(image_reader_->name(), image_reader_->directory_entries());
        return convert_to_xiso_from_avl(avl_tree, out_xiso_path);
    }
    else if (!in_dir_path_.empty()) //Create ISO from directory
    {
        AvlTree avl_tree(in_dir_path_.string(), in_dir_path_);
        return convert_to_xiso_from_avl(avl_tree, out_xiso_path);
    }

    if (!image_reader_) 
    {
        throw XGDException(ErrCode::MISC, HERE(), "No image reader or directory path specified");
    }

    return convert_to_xiso(out_xiso_path, scrub_type_ == ScrubType::PARTIAL); //Partial/no scrub ISO
}

std::vector<std::filesystem::path> XisoWriter::convert_to_xiso(const std::filesystem::path& out_xiso_path, const bool scrub) 
{
    ImageReader& image_reader = *image_reader_;
    uint32_t sector_offset = static_cast<uint32_t>(image_reader.image_offset() / Xiso::SECTOR_SIZE);
    uint32_t end_sector = image_reader.total_sectors();
    const std::unordered_set<uint32_t>* data_sectors;

    if (scrub) 
    {
        data_sectors = &image_reader.data_sectors();
        end_sector = std::min(end_sector, image_reader.max_data_sector() + 1);
    }

    split::ofstream out_file(out_xiso_path, split_ ? Xiso::SPLIT_MARGIN : UINT64_MAX);
    if (!out_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), out_xiso_path.string());
    }

    std::vector<char> buffer(Xiso::SECTOR_SIZE);

    XGDLog() << "Writing XISO" << XGDLog::Endl;

    for (uint32_t i = sector_offset; i < end_sector; i++) 
    {
        bool write_sector = true;

        if (scrub && image_reader.platform() == Platform::OGX) 
        {
            write_sector = data_sectors->find(i) != data_sectors->end();
        }

        if (write_sector) 
        {
            image_reader.read_sector(i, buffer.data());
        } 
        else 
        {
            std::fill(buffer.begin(), buffer.end(), 0);
        }

        out_file.write(buffer.data(), Xiso::SECTOR_SIZE);
        if (out_file.fail()) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write sector to output file");
        }

        XGDLog().print_progress(i - sector_offset, end_sector - sector_offset - 1);
    }

    out_file.close();
    return out_file.paths();
}

std::vector<std::filesystem::path> XisoWriter::convert_to_xiso_from_avl(AvlTree& avl_tree, const std::filesystem::path& out_xiso_path) 
{
    total_bytes_ = avl_tree.total_bytes();
    bytes_processed_ = 0;

    split::ofstream out_file(out_xiso_path, split_ ? Xiso::SPLIT_MARGIN : UINT64_MAX);
    if (!out_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), out_xiso_path.string());
    }

    XGDLog() << "Writing XISO" << XGDLog::Endl;

    write_header(out_file, avl_tree);

    out_file.seekp(avl_tree.root()->start_sector * Xiso::SECTOR_SIZE, std::ios::beg);

    AvlTree::traverse<split::ofstream>(avl_tree.root(), AvlTree::TraversalMethod::PREFIX, 
        [this](AvlTree::Node* node, split::ofstream* out_file, int depth) {
            write_tree(node, out_file, depth);
        }, &out_file, 0);  

    out_file.seekp(0, std::ios::end);
    pad_to_modulus(out_file, Xiso::FILE_MODULUS, 0x00);

    out_file.close();
    return out_file.paths();
}

void XisoWriter::write_tree(AvlTree::Node* node, split::ofstream* out_file, int depth) 
{
    if (!node->subdirectory) 
    {
        return;
    }

    if (node->subdirectory != EMPTY_SUBDIRECTORY) 
    {
        if (image_reader_) 
        {
            AvlTree::traverse<split::ofstream>(node->subdirectory, AvlTree::TraversalMethod::PREFIX, 
                [this](AvlTree::Node* node, split::ofstream* out_file, int depth) {
                    write_file_from_reader(node, out_file, depth);
                }, out_file, 0);
        } 
        else
        {
            AvlTree::traverse<split::ofstream>(node->subdirectory, AvlTree::TraversalMethod::PREFIX, 
                [this](AvlTree::Node* node, split::ofstream* out_file, int depth) {
                    write_file_from_directory(node, out_file, depth);
                }, out_file, 0);
        }
        
        AvlTree::traverse<split::ofstream>(node->subdirectory, AvlTree::TraversalMethod::PREFIX, 
            [this](AvlTree::Node* node, split::ofstream* out_file, int depth) {
                write_tree(node, out_file, depth);
            }, out_file, 0);

        out_file->seekp(node->start_sector * Xiso::SECTOR_SIZE, std::ios::beg);

        AvlTree::traverse<split::ofstream>(node->subdirectory, AvlTree::TraversalMethod::PREFIX, 
            [this](AvlTree::Node* node, split::ofstream* out_file, int depth) {
                write_entry(node, out_file, depth);
            }, out_file, 0);

        pad_to_modulus(*out_file, Xiso::SECTOR_SIZE, Xiso::PAD_BYTE); 
    } 
    else 
    {
        std::vector<char> pad_sector(Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);

        out_file->seekp(node->start_sector * Xiso::SECTOR_SIZE, std::ios::beg);
        out_file->write(pad_sector.data(), Xiso::SECTOR_SIZE);
        if (out_file->fail()) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write padding sector");
        }
    }
}

void XisoWriter::write_entry(AvlTree::Node* node, split::ofstream* out_file, int depth) 
{
    Xiso::DirectoryEntry::Header header = get_directory_entry_header(*node);    

    uint32_t padding_length = static_cast<uint32_t>(node->offset + node->directory_start - out_file->tellp());
    std::vector<char> padding(padding_length, Xiso::PAD_BYTE);

    if (sizeof(header) != sizeof(Xiso::DirectoryEntry::Header)) 
    {
        throw XGDException(ErrCode::MISC, HERE(), "Header size mismatch");
    }

    out_file->write(padding.data(), padding_length);
    out_file->write(reinterpret_cast<char*>(&header), sizeof(Xiso::DirectoryEntry::Header));
    out_file->write(node->filename.c_str(), header.name_length);

    if (out_file->fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write directory entry for: " + node->filename);
    }
}

void XisoWriter::write_file_from_reader(AvlTree::Node* node, split::ofstream* out_file, int depth) 
{
    if (node->subdirectory) 
    {
        return;
    }

    out_file->seekp(node->start_sector * Xiso::SECTOR_SIZE, std::ios::beg);
    if (out_file->fail() || out_file->tellp() != (node->start_sector * Xiso::SECTOR_SIZE)) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to seek to file sector: " + node->filename);
    }

    uint32_t bytes_remaining = static_cast<uint32_t>(node->file_size);
    uint64_t read_position = image_reader_->image_offset() + (node->old_start_sector * static_cast<uint64_t>(Xiso::SECTOR_SIZE));
    std::vector<char> buffer(XGD::BUFFER_SIZE, 0);

    auto start_pos = out_file->tellp();

    while (bytes_remaining > 0) 
    {
        uint32_t bytes_to_read = std::min(bytes_remaining, XGD::BUFFER_SIZE);

        image_reader_->read_bytes(read_position, bytes_to_read, buffer.data());

        out_file->write(buffer.data(), bytes_to_read);
        if (out_file->fail() || out_file->tellp() != start_pos + bytes_to_read) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write file data: " + node->filename);
        }

        start_pos += bytes_to_read;
        bytes_remaining -= bytes_to_read;
        read_position += bytes_to_read;

        XGDLog().print_progress(bytes_processed_ += bytes_to_read, total_bytes_);
    }

    if ((node->file_size + (node->start_sector * Xiso::SECTOR_SIZE)) != out_file->tellp()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "File write size mismatch: " + node->filename);
    }

    pad_to_modulus(*out_file, Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);
}

void XisoWriter::write_file_from_directory(AvlTree::Node* node, split::ofstream* out_file, int depth)
{
    if (node->subdirectory) 
    {
        return;
    }

    out_file->seekp(node->start_sector * Xiso::SECTOR_SIZE, std::ios::beg);
    if (out_file->fail() || out_file->tellp() != (node->start_sector * Xiso::SECTOR_SIZE)) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to seek to file sector: " + node->filename);
    }

    std::ifstream in_file(node->path, std::ios::binary);
    if (!in_file.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), node->path.string());
    }

    uint32_t bytes_remaining = static_cast<uint32_t>(node->file_size);
    std::vector<char> buffer(XGD::BUFFER_SIZE, 0);

    while (bytes_remaining > 0) 
    {
        uint32_t bytes_to_write = std::min(bytes_remaining, XGD::BUFFER_SIZE);

        in_file.read(buffer.data(), bytes_to_write);
        if (in_file.fail()) 
        {
            throw XGDException(ErrCode::FILE_READ, HERE(), "Failed to read file data: " + node->path.string());
        }

        out_file->write(buffer.data(), bytes_to_write);
        if (out_file->fail()) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write file data: " + node->filename);
        }

        bytes_remaining -= bytes_to_write;

        XGDLog().print_progress(bytes_processed_ += bytes_to_write, total_bytes_);
    }

    in_file.close();

    if ((node->file_size + (node->start_sector * Xiso::SECTOR_SIZE)) != out_file->tellp()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "File write size mismatch, possible overflow issue: " + node->filename);
    }

    pad_to_modulus(*out_file, Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);
}

void XisoWriter::write_header(split::ofstream& out_file, AvlTree& avl_tree) 
{
    Xiso::Header header(static_cast<uint32_t>(avl_tree.root()->start_sector), 
                        static_cast<uint32_t>(avl_tree.root()->file_size), 
                        static_cast<uint32_t>(avl_tree.out_iso_size() / Xiso::SECTOR_SIZE),
                        image_reader_ ? image_reader_->file_time() : Xiso::FileTime());

    out_file.write(reinterpret_cast<char*>(&header), sizeof(Xiso::Header));
    if (out_file.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write header to output file");
    }
}

void XisoWriter::pad_to_modulus(split::ofstream& out_file, uint32_t modulus, char pad_byte) 
{
    if ((out_file.tellp() % modulus) == 0) 
    {
        return;
    }

    size_t padding_len = modulus - (out_file.tellp() % modulus);
    std::vector<char> buffer(padding_len, pad_byte);

    out_file.write(buffer.data(), padding_len); 
    if (out_file.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write padding bytes");
    }
}