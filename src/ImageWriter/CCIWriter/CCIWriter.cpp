#include <functional>
#include <cstring>

#include <lz4hc.h>

#include "ImageWriter/CCIWriter/CCIWriter.h"
#include "AvlTree/AvlIterator.h"

CCIWriter::CCIWriter(std::shared_ptr<ImageReader> image_reader, const ScrubType scrub_type)
    :   image_reader_(image_reader), 
        scrub_type_(scrub_type)
{
    init_cci_writer();
}

CCIWriter::CCIWriter(const std::filesystem::path& in_dir_path)
    : in_dir_path_(in_dir_path)
{
    init_cci_writer();
}

CCIWriter::~CCIWriter()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_flag_ = true;
    }
    cv_.notify_all();
    for (std::thread& thread : thread_pool_)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
}

void CCIWriter::init_cci_writer()
{
    for (uint32_t i = 0; i < std::min(std::thread::hardware_concurrency(), static_cast<uint32_t>(32)); ++i)
    {
        thread_pool_.emplace_back(&CCIWriter::thread_worker, this);
    }
}

std::vector<std::filesystem::path> CCIWriter::convert(const std::filesystem::path& out_cci_path) 
{
    out_filepath_base_ = out_cci_path;

    create_directory(out_cci_path.parent_path());

    out_filepath_1_ = out_cci_path;
    out_filepath_2_ = out_cci_path;
    out_filepath_1_.replace_extension(".1.cci");
    out_filepath_2_.replace_extension(".2.cci");

    if (image_reader_ && scrub_type_ == ScrubType::FULL) 
    {
        AvlTree avl_tree(image_reader_->name(), image_reader_->directory_entries());
        convert_to_cci_from_avl(avl_tree);
    }
    else if (!in_dir_path_.empty())
    {
        AvlTree avl_tree(in_dir_path_.filename().string(), in_dir_path_);
        convert_to_cci_from_avl(avl_tree);
    }
    else if (!image_reader_) 
    {
        throw XGDException(ErrCode::ISO_INVALID, HERE(), "No input data");
    }
    else
    {
        convert_to_cci(scrub_type_ == ScrubType::PARTIAL);
    }

    return out_paths();
}

void CCIWriter::convert_to_cci(const bool scrub)
{
    ImageReader& image_reader = *image_reader_;
    uint32_t end_sector = image_reader.total_sectors();
    uint32_t sector_offset = static_cast<uint32_t>(image_reader.image_offset() / Xiso::SECTOR_SIZE);
    const std::unordered_set<uint32_t>* data_sectors;

    if (scrub) 
    {
        data_sectors = &image_reader.data_sectors();
        end_sector = std::min(end_sector, image_reader.max_data_sector() + 1);
    }

    prog_total_ = end_sector - sector_offset - 1;
    prog_processed_ = 0;

    std::ofstream out_file(out_filepath_1_, std::ios::binary);
    if (!out_file.is_open()) 
    {
        throw std::runtime_error("Failed to open output file: " + out_filepath_1_.string());
    }

    XGDLog() << "Writing CCI file" << XGDLog::Endl;

    uint32_t current_sector = sector_offset;
    uint32_t num_threads = static_cast<uint32_t>(thread_pool_.size());

    std::vector<char> read_buffer(Xiso::SECTOR_SIZE * num_threads);
    
    std::vector<CCI::IndexInfo> index_infos;
    index_infos.reserve((end_sector - sector_offset) + 1);

    while (current_sector < end_sector) 
    {
        uint32_t read_sectors = std::min(end_sector - current_sector, num_threads);

        for (uint32_t i = 0; i < read_sectors; ++i)
        {
            bool write_sector = true;

            if (scrub && image_reader.platform() == Platform::OGX) 
            {
                write_sector = data_sectors->find(current_sector) != data_sectors->end();
            }

            if (write_sector) 
            {
                image_reader.read_sector(current_sector, read_buffer.data() + (i * Xiso::SECTOR_SIZE));
            } 
            else 
            {
                std::memset(read_buffer.data() + (i * Xiso::SECTOR_SIZE), 0x00, Xiso::SECTOR_SIZE);
            }

            current_sector++;
        }

        compress_and_write_sectors_managed(out_file, index_infos, read_sectors, read_buffer.data());

        XGDLog().print_progress(prog_processed_ += read_sectors, prog_total_);

        check_status_flags();
    }

    finalize_out_file(out_file, index_infos);
    out_file.close();
}

void CCIWriter::convert_to_cci_from_avl(AvlTree& avl_tree) 
{
    prog_total_ = avl_tree.total_bytes();
    prog_processed_ = 0;

    AvlIterator avl_iterator(avl_tree);
    std::vector<AvlIterator::Entry> avl_entries = avl_iterator.entries();

    std::ofstream out_file(out_filepath_1_, std::ios::binary);
    if (!out_file.is_open()) 
    {
        throw std::runtime_error("Failed to open output file: " + out_filepath_1_.string());
    }

    XGDLog() << "Writing CCI file" << XGDLog::Endl;

    uint32_t sectors_to_write = num_sectors(avl_tree.out_iso_size());
    std::vector<CCI::IndexInfo> index_infos;
    index_infos.reserve(sectors_to_write + 1);

    write_iso_header(out_file, index_infos, avl_tree);

    uint32_t pad_sectors = num_sectors(avl_entries.front().offset - sizeof(Xiso::Header));
    write_padding_sectors(out_file, index_infos, pad_sectors, 0x00);

    uint32_t sectors_written = num_sectors(avl_entries.front().offset);

    for (size_t i = 0; i < avl_entries.size(); i++) 
    {
        if (num_sectors(avl_entries[i].offset) > sectors_written)
        {
            uint32_t pad_sectors = num_sectors(avl_entries[i].offset) - sectors_written;

            XGDLog(Debug) << "Padding " << pad_sectors << " sectors\n";

            write_padding_sectors(out_file, index_infos, pad_sectors, Xiso::PAD_BYTE);
            sectors_written += pad_sectors;
        } 

        if (num_sectors(avl_entries[i].offset) != sectors_written || avl_entries[i].offset % Xiso::SECTOR_SIZE) 
        {
            throw XGDException(ErrCode::MISC, HERE(), "CCI file has become misaligned");
        }

        if (avl_entries[i].directory_entry) 
        {
            std::vector<char> dir_buffer;
            size_t entries_processed = write_directory_to_buffer(avl_entries, i, dir_buffer);
            i += entries_processed - 1;

            uint32_t write_sectors = num_sectors(dir_buffer.size());
            compress_and_write_sectors_managed(out_file, index_infos, write_sectors, dir_buffer.data());
            sectors_written += write_sectors;
        } 
        else 
        {
            if (image_reader_) 
            {
                write_file_from_reader(out_file, index_infos, *avl_entries[i].node);
            } 
            else 
            {
                write_file_from_dir(out_file, index_infos, *avl_entries[i].node);
            }

            sectors_written += num_sectors(avl_entries[i].node->file_size);
        }
    }

    pad_sectors = sectors_to_write - sectors_written;

    if (pad_sectors > 0) 
    {
        write_padding_sectors(out_file, index_infos, pad_sectors, 0x00);
    }

    finalize_out_file(out_file, index_infos);
    out_file.close();
}

void CCIWriter::write_iso_header(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, AvlTree& avl_tree)
{
    Xiso::Header xiso_header(   static_cast<uint32_t>(avl_tree.root()->start_sector), 
                                static_cast<uint32_t>(avl_tree.root()->file_size), 
                                static_cast<uint32_t>(avl_tree.out_iso_size() / Xiso::SECTOR_SIZE),
                                image_reader_ ? image_reader_->file_time() : Xiso::FileTime());

    static_assert(!(sizeof(Xiso::Header) % Xiso::SECTOR_SIZE), "Xiso::Header size must be a multiple of Xiso::SECTOR_SIZE");

    compress_and_write_sectors_managed(out_file, index_infos, num_sectors(sizeof(Xiso::Header)), reinterpret_cast<char*>(&xiso_header));
}

void CCIWriter::write_padding_sectors(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, const uint32_t num_sectors, const char pad_byte)
{
    std::vector<char> pad_sector(Xiso::SECTOR_SIZE * num_sectors, pad_byte);
    compress_and_write_sectors_managed(out_file, index_infos, num_sectors, pad_sector.data());
}

void CCIWriter::write_file_from_reader(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, AvlTree::Node& node) 
{
    ImageReader& image_reader = *image_reader_;
    uint64_t bytes_remaining = node.file_size;
    uint64_t read_position = image_reader.image_offset() + (node.old_start_sector * Xiso::SECTOR_SIZE);
    std::vector<char> read_buffer(Xiso::SECTOR_SIZE * thread_pool_.size());

    while (bytes_remaining > 0) 
    {
        uint64_t read_size = std::min(bytes_remaining, read_buffer.size());

        image_reader.read_bytes(read_position, read_size, read_buffer.data());

        if (read_size % Xiso::SECTOR_SIZE) // Pad buffer to sector boundary with 0xFF
        {
            std::memset(read_buffer.data() + read_size, Xiso::PAD_BYTE, (Xiso::SECTOR_SIZE - (read_size % Xiso::SECTOR_SIZE)));
        }

        compress_and_write_sectors_managed(out_file, index_infos, num_sectors(read_size), read_buffer.data());

        bytes_remaining -= read_size;
        read_position += read_size;

        XGDLog().print_progress(prog_processed_ += read_size, prog_total_);

        check_status_flags();
    }
}

void CCIWriter::write_file_from_dir(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, AvlTree::Node& node) 
{
    std::ifstream in_file(node.path, std::ios::binary);
    if (!in_file.is_open()) 
    {
        throw std::runtime_error("Failed to open input file: " + node.path.string());
    }

    uint64_t bytes_remaining = node.file_size;
    std::vector<char> read_buffer(Xiso::SECTOR_SIZE * thread_pool_.size());

    while (bytes_remaining > 0) 
    {
        uint64_t read_size = std::min(bytes_remaining, read_buffer.size());

        in_file.read(read_buffer.data(), read_size);
        if (in_file.fail()) 
        {
            throw std::runtime_error("Failed to read from input file: " + node.path.string());
        }

        if (read_size % Xiso::SECTOR_SIZE) // Pad buffer to sector boundary with 0xFF
        {
            std::memset(read_buffer.data() + read_size, Xiso::PAD_BYTE, (Xiso::SECTOR_SIZE - (read_size % Xiso::SECTOR_SIZE)));
        }

        compress_and_write_sectors_managed(out_file, index_infos, num_sectors(read_size), read_buffer.data());

        bytes_remaining -= read_size;

        XGDLog().print_progress(prog_processed_ += read_size, prog_total_);

        check_status_flags();
    }

    in_file.close();
}

void CCIWriter::thread_worker()
{
    while (true)
    {
        CompressTask task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { return stop_flag_ || !task_queue_.empty(); });

            if (stop_flag_ && task_queue_.empty())
            {
                return;
            }

            task = std::move(task_queue_.front());
            task_queue_.pop();
        }

        int compressed_size = LZ4_compress_HC(task.in_buffer, task.out_buffer, task.in_size, task.in_size, 12);

        CompressedTaskResult result; 
        result.sector_idx       = task.sector_idx; 
        result.compressed_size  = compressed_size;
        result.compressed       = compressed_size > 0 && compressed_size < static_cast<int>(Xiso::SECTOR_SIZE - (4 + ALIGN_MULT));
        result.buffer_to_write  = result.compressed ? task.out_buffer : task.in_buffer; 
        
        task.promise.set_value(result);
    }
}

/*  This will check if the out file needs to be split or if it needs room 
    for a CCI header using check_and_manage_write and finalize_out_file */
void CCIWriter::compress_and_write_sectors_managed(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos, const uint32_t num_sectors, const char* in_buffer)
{
    std::vector<std::future<CompressedTaskResult>> futures;
    std::vector<std::vector<char>> compress_buffers(num_sectors, std::vector<char>(LZ4_compressBound(Xiso::SECTOR_SIZE)));

    for (uint32_t i = 0; i < num_sectors; ++i)
    {
        std::promise<CompressedTaskResult> promise;
        futures.emplace_back(promise.get_future());

        CompressTask task;
        task.in_buffer = in_buffer + (i * Xiso::SECTOR_SIZE);
        task.in_size = Xiso::SECTOR_SIZE;
        task.out_buffer = compress_buffers[i].data();
        task.sector_idx = i;
        task.promise = std::move(promise);

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.push(std::move(task));
        }
        cv_.notify_one();
    }

    std::vector<CompressedTaskResult> ordered_results;
    ordered_results.reserve(num_sectors);

    for (auto& future : futures)
    {
        ordered_results.push_back(future.get());
    }

    // Order results by sector index incase they were compressed out of order
    std::sort(ordered_results.begin(), ordered_results.end(), [](const CompressedTaskResult& a, const CompressedTaskResult& b) 
    {
        return a.sector_idx < b.sector_idx;
    });

    for (const auto& result : ordered_results)
    {
        check_and_manage_write(out_file, index_infos);

        if (result.compressed)
        {
            uint8_t padding = static_cast<uint8_t>(((result.compressed_size + 1 + ALIGN_MULT - 1) / ALIGN_MULT * ALIGN_MULT) - (result.compressed_size + 1));
            out_file.write(reinterpret_cast<const char*>(&padding), sizeof(uint8_t));
            out_file.write(result.buffer_to_write, result.compressed_size);

            if (padding != 0)
            {
                std::vector<char> empty_buffer(padding, 0);
                out_file.write(empty_buffer.data(), padding);
            }

            index_infos.push_back({ static_cast<uint32_t>(result.compressed_size + 1 + padding), true });
        }
        else
        {
            out_file.write(result.buffer_to_write, Xiso::SECTOR_SIZE);
            index_infos.push_back({ Xiso::SECTOR_SIZE, false });
        }

        if (out_file.fail())
        {
            throw std::runtime_error("Failed to write to output file");
        }
    }
}

void CCIWriter::check_and_manage_write(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos)
{
    if (static_cast<uint64_t>(out_file.tellp()) > CCI::SPLIT_OFFSET)
    {
        finalize_out_file(out_file, index_infos);
        out_file.close();

        out_file = std::ofstream(out_filepath_2_, std::ios::binary);
        if (!out_file.is_open())
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), "Failed to open output file: " + out_filepath_2_.string());
        }
    }

    if (index_infos.size() == 0 && out_file.tellp() == 0)
    {
        std::vector<char> empty_buffer(sizeof(CCI::Header), 0);
        out_file.write(empty_buffer.data(), sizeof(CCI::Header));
    }
}

//  This writes the index info and finalized header to the current out file
void CCIWriter::finalize_out_file(std::ofstream& out_file, std::vector<CCI::IndexInfo>& index_infos) 
{
    out_file.seekp(0, std::ios::end);

    uint64_t index_offset = out_file.tellp();
    uint64_t uncompressed_size = index_infos.size() * Xiso::SECTOR_SIZE;
    uint32_t position = CCI::HEADER_SIZE;

    for (const auto& index_info : index_infos) 
    {
        uint32_t index = static_cast<uint32_t>(position >> CCI::INDEX_ALIGNMENT) | (index_info.compressed ? 0x80000000 : 0);
        out_file.write(reinterpret_cast<const char*>(&index), sizeof(uint32_t));
        position += index_info.value;
    }

    uint32_t index_end = static_cast<uint32_t>(position >> CCI::INDEX_ALIGNMENT);
    out_file.write(reinterpret_cast<const char*>(&index_end), sizeof(uint32_t));

    CCI::Header cci_header(uncompressed_size, index_offset);

    out_file.seekp(0, std::ios::beg);
    out_file.write(reinterpret_cast<char*>(&cci_header), sizeof(CCI::Header));

    if (out_file.fail()) 
    {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write to output file");  
    }

    index_infos.clear();
}

std::vector<std::filesystem::path> CCIWriter::out_paths() 
{
    if (std::filesystem::exists(out_filepath_2_)) 
    {
        return { out_filepath_1_, out_filepath_2_ };
    }

    try 
    {
        std::filesystem::rename(out_filepath_1_, out_filepath_base_);
        return { out_filepath_base_ };
    } 
    catch (const std::exception& e) 
    {
        XGDLog(Error) << "Warning: Failed to rename output file: " << e.what() << XGDLog::Endl;
    }

    return { out_filepath_1_ };
}