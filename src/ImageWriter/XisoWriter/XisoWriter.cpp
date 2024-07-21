#include <algorithm>

#include "Common/Utils.h"
#include "ImageWriter/XisoWriter/XisoWriter.h"
#include "AvlTree/AvlTree.h"

XisoWriter::XisoWriter(std::shared_ptr<ImageReader> image_reader, ScrubType scrub_type, const bool split) 
    :   image_reader_(image_reader), 
        scrub_type_(scrub_type), 
        split_(split) 
{
    if (scrub_type_ == ScrubType::FULL) {
        avl_tree_ = std::make_unique<AvlTree>(image_reader_->name(), image_reader_->directory_entries());
    }
}

XisoWriter::XisoWriter(const std::filesystem::path& in_dir_path, const bool split) 
    :   split_(split) 
{
    avl_tree_ = std::make_unique<AvlTree>(in_dir_path.filename().string(), in_dir_path);
}

XisoWriter::~XisoWriter() 
{
    if (out_file_.is_open()) {
        out_file_.close();
    }
}

std::vector<std::filesystem::path> XisoWriter::convert(const std::filesystem::path& out_xiso_path) 
{
    create_directory(out_xiso_path.parent_path());
    
    if (avl_tree_) 
    {
        return convert_to_xiso_from_avl(out_xiso_path);
    }
    return convert_to_xiso(out_xiso_path, scrub_type_ == ScrubType::PARTIAL);
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

    out_file_ = split::ofstream(out_xiso_path, split_ ? Xiso::SPLIT_MARGIN : UINT64_MAX);
    if (!out_file_.is_open()) 
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

        out_file_.write(buffer.data(), Xiso::SECTOR_SIZE);
        if (out_file_.fail()) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write sector to output file");
        }

        XGDLog().print_progress(i - sector_offset, end_sector - sector_offset - 1);
    }

    out_file_.close();
    return out_file_.paths();
}

std::vector<std::filesystem::path> XisoWriter::convert_to_xiso_from_avl(const std::filesystem::path& out_xiso_path) 
{
    total_bytes_ = avl_tree_->total_bytes();
    bytes_processed_ = 0;

    out_file_ = split::ofstream(out_xiso_path, split_ ? Xiso::SPLIT_MARGIN : UINT64_MAX);
    if (!out_file_.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), out_xiso_path.string());
    }

    XGDLog() << "Writing XISO" << XGDLog::Endl;

    write_header_from_avl();

    out_file_.seekp(avl_tree_->root()->start_sector * Xiso::SECTOR_SIZE, std::ios::beg);

    if (image_reader_) 
    {
        AvlTree::traverse(avl_tree_->root(), AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
            write_tree(node, static_cast<ImageReader*>(context), depth);
        }, image_reader_.get(), 0);  
        
    } 
    else 
    {
        AvlTree::traverse(avl_tree_->root(), AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
            write_tree(node, nullptr, depth);
        }, nullptr, 0);
    }

    out_file_.seekp(0, std::ios::end);
    IOUtils::pad_to_modulus(out_file_, Xiso::FILE_MODULUS, 0);

    out_file_.close();
    return out_file_.paths();
}

void XisoWriter::write_tree(AvlTree::Node* node, ImageReader* image_reader, int depth) 
{
    if (!node->subdirectory) 
    {
        return;
    }

    if (node->subdirectory != EMPTY_SUBDIRECTORY) 
    {
        if (image_reader) 
        {
            AvlTree::traverse(node->subdirectory, AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
                write_file(node, static_cast<ImageReader*>(context), depth);
            }, image_reader, 0);

        } 
        else 
        {
            AvlTree::traverse(node->subdirectory, AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
                write_file_dir(node, nullptr, depth);
            }, nullptr, 0);
        }
        
        AvlTree::traverse(node->subdirectory, AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
            write_tree(node, static_cast<ImageReader*>(context), depth);
        }, image_reader, 0);

        out_file_.seekp(node->start_sector * Xiso::SECTOR_SIZE, std::ios::beg);

        AvlTree::traverse(node->subdirectory, AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
            write_entry(node, context, depth);
        }, nullptr, 0);

        IOUtils::pad_to_modulus(out_file_, Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);
    } 
    else 
    {
        std::vector<char> pad_sector(Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);
        out_file_.seekp(node->start_sector * Xiso::SECTOR_SIZE, std::ios::beg);
        out_file_.write(pad_sector.data(), Xiso::SECTOR_SIZE);

        if (out_file_.fail()) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write padding sector");
        }
    }
}

void XisoWriter::write_entry(AvlTree::Node* node, void* context, int depth) 
{
    Xiso::DirectoryEntry::Header header;
    header.left_offset  = node->left_child ? static_cast<uint16_t>(node->left_child->offset / sizeof(uint32_t)) : 0;
    header.right_offset = node->right_child ? static_cast<uint16_t>(node->right_child->offset / sizeof(uint32_t)) : 0;
    header.start_sector = static_cast<uint32_t>(node->start_sector);
    header.file_size    = static_cast<uint32_t>(node->file_size + (node->subdirectory ? ((Xiso::SECTOR_SIZE - (node->file_size % Xiso::SECTOR_SIZE)) % Xiso::SECTOR_SIZE) : 0));
    header.attributes   = node->subdirectory ? Xiso::ATTRIBUTE_DIRECTORY : Xiso::ATTRIBUTE_FILE;
    header.name_length  = static_cast<uint8_t>(std::min(node->filename.size(), static_cast<size_t>(UINT8_MAX)));
    
    EndianUtils::little_16(header.left_offset);
    EndianUtils::little_16(header.right_offset);
    EndianUtils::little_32(header.start_sector);
    EndianUtils::little_32(header.file_size);

    uint32_t padding_length = static_cast<uint32_t>(node->offset + node->directory_start - out_file_.tellp());
    XGDLog() << "Writing at offset: " << out_file_.tellp() + padding_length << " dir start: " << node->directory_start << " offset: " << node->offset << " combined: " << node->directory_start + node->offset << XGDLog::Endl;
    std::vector<char> padding(padding_length, Xiso::PAD_BYTE);

    if (sizeof(header) != sizeof(Xiso::DirectoryEntry::Header)) 
    {
        throw XGDException(ErrCode::MISC, HERE(), "Header size mismatch");
    }

    out_file_.write(padding.data(), padding_length);
    out_file_.write(reinterpret_cast<char*>(&header), sizeof(Xiso::DirectoryEntry::Header));
    out_file_.write(node->filename.c_str(), header.name_length);

    if (out_file_.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write directory entry for: " + node->filename);
    }
}

void XisoWriter::write_file(AvlTree::Node* node, ImageReader* image_reader, int depth) 
{
    if (node->subdirectory) 
    {
        return;
    }

    out_file_.seekp(node->start_sector * Xiso::SECTOR_SIZE, std::ios::beg);
    if (out_file_.fail() || out_file_.tellp() != (node->start_sector * Xiso::SECTOR_SIZE)) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to seek to file sector: " + node->filename);
    }

    uint32_t bytes_remaining = static_cast<uint32_t>(node->file_size);
    uint64_t read_position = image_reader->image_offset() + (node->old_start_sector * static_cast<uint64_t>(Xiso::SECTOR_SIZE));
    std::vector<char> buffer(XGD::BUFFER_SIZE, 0);

    auto start_pos = out_file_.tellp();

    while (bytes_remaining > 0) 
    {
        uint32_t bytes_to_read = std::min(bytes_remaining, XGD::BUFFER_SIZE);

        image_reader->read_bytes(read_position, bytes_to_read, buffer.data());

        out_file_.write(buffer.data(), bytes_to_read);
        if (out_file_.fail() || out_file_.tellp() != start_pos + bytes_to_read) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write file data: " + node->filename);
        }

        start_pos += bytes_to_read;
        bytes_remaining -= bytes_to_read;
        read_position += bytes_to_read;

        XGDLog().print_progress(bytes_processed_ += bytes_to_read, total_bytes_);
    }

    if ((node->file_size + (node->start_sector * Xiso::SECTOR_SIZE)) != out_file_.tellp()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "File write size mismatch: " + node->filename);
    }

    if (out_file_.tellp() % Xiso::SECTOR_SIZE) 
    {
        IOUtils::pad_to_modulus(out_file_, Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);
    }
}

void XisoWriter::write_file_dir(AvlTree::Node* node, void* context, int depth) 
{
    if (node->subdirectory) 
    {
        return;
    }

    out_file_.seekp(node->start_sector * Xiso::SECTOR_SIZE, std::ios::beg);
    if (out_file_.fail() || out_file_.tellp() != (node->start_sector * Xiso::SECTOR_SIZE)) 
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
            throw XGDException(ErrCode::FILE_READ, HERE(), "Failed to read file data: " + node->filename);
        }

        out_file_.write(buffer.data(), bytes_to_write);
        if (out_file_.fail()) 
        {
            throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write file data: " + node->filename);
        }

        bytes_remaining -= bytes_to_write;

        XGDLog().print_progress(bytes_processed_ += bytes_to_write, total_bytes_);
    }

    in_file.close();

    if ((node->file_size + (node->start_sector * Xiso::SECTOR_SIZE)) != out_file_.tellp()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "File write size mismatch, possible overflow issue: " + node->filename);
    }

    if (out_file_.tellp() % Xiso::SECTOR_SIZE) 
    {
        uint32_t padding_length = Xiso::SECTOR_SIZE - (out_file_.tellp() % Xiso::SECTOR_SIZE);
        std::vector<char> pad_sector(padding_length, Xiso::PAD_BYTE);
        out_file_.write(pad_sector.data(), padding_length);
    }
}

void XisoWriter::write_header_from_avl() 
{
    Xiso::Header header(static_cast<uint32_t>(avl_tree_->root()->start_sector), 
                        static_cast<uint32_t>(avl_tree_->root()->file_size), 
                        static_cast<uint32_t>(avl_tree_->out_iso_size() / Xiso::SECTOR_SIZE));

    out_file_.write(reinterpret_cast<char*>(&header), sizeof(Xiso::Header));
    if (out_file_.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write header to output file");
    }
}

// void XisoWriter::convert_iterative(ImageReader& reader) {
//     AvlTree avl_tree(reader.name(), reader.directory_entries());
//     auto out_iso_size = avl_tree.out_iso_size();

//     total_bytes_ = reader.total_file_bytes();
//     bytes_processed_ = 0;

//     AvlIterator avl_iterator(avl_tree);
//     std::vector<AvlIterator::Entry> entries = avl_iterator.entries();

//     std::ofstream out_file(out_xiso_path_, std::ios::binary);
//     if (!out_file.is_open()) {
//         throw XGDException(ErrCode::FILE_OPEN, HERE(), out_xiso_path_.string());
//     }

//     XGDLog() << "Writing XISO" << XGDLog::Endl;

//     Xiso::Header header(static_cast<uint32_t>(avl_tree.root()->start_sector), static_cast<uint32_t>(avl_tree.root()->file_size), static_cast<uint32_t>(out_iso_size / Xiso::SECTOR_SIZE));
//     auto header_sectors = (sizeof(Xiso::Header) + Xiso::SECTOR_SIZE - 1) / Xiso::SECTOR_SIZE;

//     for (auto i = 0; i < header_sectors; ++i) {
//         out_file.write(reinterpret_cast<char*>(&header) + (i * Xiso::SECTOR_SIZE), Xiso::SECTOR_SIZE);
//         if (out_file.fail()) {
//             throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write header to output file");
//         }
//     }

//     for (auto i = 0; i < (entries.front().offset / Xiso::SECTOR_SIZE) - header_sectors; ++i) {
//         std::vector<char> pad_sector(Xiso::SECTOR_SIZE, 0);
//         out_file.write(pad_sector.data(), Xiso::SECTOR_SIZE);
//         if (out_file.fail()) {
//             throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write padding sector");
//         }
//     }

//     entries.push_back({ 0, false, nullptr });

//     for (auto i = 0; i < entries.size(); ++i) {
//         if (!entries[i].node) {
//             break;
//         }

//         if (out_file.fail()) {
//             throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write directory entry");
//         }

//         if (entries[i].directory_entry) {
            
//             std::vector<char> entry_buffer;

//             std::cerr << "Entry offset: " << entries[i].offset 
//                       << " File offset: " << out_file.tellp() 
//                       << " dir start: " << entries[i].node->directory_start 
//                       << " node offset: " << entries[i].node->offset << std::endl;

//             if (entries[i].offset > out_file.tellp()) {
//                 if (out_file_.tellp() % Xiso::SECTOR_SIZE) {
//                     throw XGDException(ErrCode::MISC, HERE(), "unaligned to sector boundary");
//                 }
//                 auto padding_length = entries[i].offset - out_file.tellp();
//                 std::vector<char> padding(padding_length, Xiso::PAD_BYTE);
//                 std::cerr << "writing entry padding: " << padding_length << std::endl;
//                 out_file.write(padding.data(), padding_length);
//             }

//             for (auto j = i; j < entries.size(); ++j) {
//                 if (entry_buffer.size() != entries[j].node->offset) {
//                     throw XGDException(ErrCode::MISC, HERE(), "Entry buffer size mismatch");
//                 }

//                 Xiso::DirectoryEntry::Header header;
//                 header.left_offset  = entries[j].node->left_child ? static_cast<uint16_t>(entries[j].node->left_child->offset / sizeof(uint32_t)) : 0;
//                 header.right_offset = entries[j].node->right_child ? static_cast<uint16_t>(entries[j].node->right_child->offset / sizeof(uint32_t)) : 0;
//                 header.start_sector = static_cast<uint32_t>(entries[j].node->start_sector);
//                 header.file_size    = static_cast<uint32_t>(entries[j].node->file_size + (entries[j].node->subdirectory ? ((Xiso::SECTOR_SIZE - (entries[j].node->file_size % Xiso::SECTOR_SIZE)) % Xiso::SECTOR_SIZE) : 0));
//                 header.attributes   = entries[j].node->subdirectory ? Xiso::ATTRIBUTE_DIRECTORY : Xiso::ATTRIBUTE_FILE;
//                 header.name_length  = static_cast<uint8_t>(std::min(entries[j].node->filename.size(), static_cast<size_t>(UINT8_MAX)));

//                 EndianUtils::little_16(header.left_offset);
//                 EndianUtils::little_16(header.right_offset);
//                 EndianUtils::little_32(header.start_sector);
//                 EndianUtils::little_32(header.file_size);

//                 auto entry_len = sizeof(Xiso::DirectoryEntry::Header) + header.name_length;
//                 auto buffer_position = entry_buffer.size();

//                 entry_buffer.resize(entry_buffer.size() + entry_len, Xiso::PAD_BYTE);

//                 std::memcpy(entry_buffer.data() + buffer_position, &header, sizeof(Xiso::DirectoryEntry::Header));
//                 std::memcpy(entry_buffer.data() + buffer_position + sizeof(Xiso::DirectoryEntry::Header), entries[j].node->filename.c_str(), header.name_length);

//                 if (!entries[j + 1].node || 
//                     !entries[j + 1].directory_entry || 
//                     (entries[j + 1].node->directory_start != entries[j].node->directory_start)) {
//                     break;
//                 }

//                 i++;

//                 auto padding = entries[j + 1].node->offset - entry_buffer.size();
//                 entry_buffer.resize(entry_buffer.size() + padding, Xiso::PAD_BYTE);
//             }

//             if (entry_buffer.size() % Xiso::SECTOR_SIZE) {
//                 entry_buffer.resize(entry_buffer.size() + (Xiso::SECTOR_SIZE - (entry_buffer.size() % Xiso::SECTOR_SIZE)), Xiso::PAD_BYTE);
//             }

//             out_file.write(entry_buffer.data(), entry_buffer.size());

//             // for (auto j = 0; j < entry_buffer.size(); j += Xiso::SECTOR_SIZE) {
//             //     out_file.write(entry_buffer.data() + j, Xiso::SECTOR_SIZE);
//             //     if (out_file.fail()) {
//             //         throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write directory entry");
//             //     }
//             // }
//         } else {
//             // // std::cerr << "current offset: " << out_file.tellp() << " expected offset: " << entries[i].offset << std::endl;

//             // if (entries[i].node->subdirectory) {
//             //     continue;
//             // }

//             if (entries[i].offset > out_file.tellp()) {
//                 // std::cerr << "Padding, write offset : " << entries[i].offset << " current offset: " << out_file.tellp() << std::endl; 
//                 auto padding_length = entries[i].offset - out_file.tellp();
//                 std::vector<char> padding(padding_length, Xiso::PAD_BYTE);
//                 out_file.write(padding.data(), padding_length);
//             } else if (entries[i].offset < out_file.tellp()) {
//                 throw XGDException(ErrCode::MISC, HERE(), "offset less than current file position");
//             }

//             if (entries[i].node->subdirectory == EMPTY_SUBDIRECTORY) {
//                 std::vector<char> pad_sector(Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);
//                 out_file.write(pad_sector.data(), Xiso::SECTOR_SIZE);
//                 continue;
//             }

//             std::cerr << "writing file at " << out_file.tellp() << " start sector: " << entries[i].node->start_sector << " file size: " << entries[i].node->file_size << std::endl;

//             uint32_t bytes_remaining = static_cast<uint32_t>(entries[i].node->file_size);
//             uint64_t read_position = reader.image_offset() + (entries[i].node->old_start_sector * static_cast<uint64_t>(Xiso::SECTOR_SIZE));

//             while (bytes_remaining > 0) {
//                 uint32_t bytes_to_read = std::min(bytes_remaining, Xiso::SECTOR_SIZE);
//                 std::vector<char> buffer(Xiso::SECTOR_SIZE, Xiso::PAD_BYTE);

//                 reader.read_bytes(read_position, bytes_to_read, buffer.data());

//                 out_file.write(buffer.data(), Xiso::SECTOR_SIZE);
//                 if (out_file.fail()) {
//                     throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write file data: " + entries[i].node->filename);
//                 }

//                 bytes_remaining -= bytes_to_read;
//                 read_position += bytes_to_read;

//                 XGDLog().print_progress(bytes_processed_ += bytes_to_read, total_bytes_);
//             }
//         }
//     }

//     if (out_file.tellp() % Xiso::FILE_MODULUS) {
//         std::vector<char> pad_sector(Xiso::FILE_MODULUS - (out_file.tellp() % Xiso::FILE_MODULUS), 0);
//         out_file.write(pad_sector.data(), pad_sector.size());
//     }

//     out_file.close();
// }
