#include <algorithm>

#include "XGD.h"
#include "AvlTree/AvlTree.h"

void AvlTree::generate_from_filesystem(const std::filesystem::path& in_directory, Node** dir_node) 
{
    for (const auto& entry : std::filesystem::directory_iterator(in_directory)) 
    {
        const auto& entry_path = entry.path();
        const auto& entry_filename = entry_path.filename().string();

        Node* current_node = new Node(entry_filename);
        current_node->path = std::filesystem::absolute(entry_path);

        if (std::filesystem::is_directory(entry_path)) 
        {
            generate_from_filesystem(entry_path, &current_node->subdirectory);

            if (!current_node->subdirectory) 
            {
                current_node->subdirectory = EMPTY_SUBDIRECTORY;
            }
        } 
        else if (std::filesystem::is_regular_file(entry_path)) 
        {
            if (std::filesystem::file_size(entry_path) > UINT32_MAX) 
            {
                XGDLog(Error) << "Warning: File size exceeds maximum allowed in XISO format:.\nSkipping: " << entry_path.string() << "\n";
                delete current_node;
                continue;
            }

            current_node->file_size = static_cast<uint32_t>(std::filesystem::file_size(entry_path));

            total_bytes_ += current_node->file_size;
            ++total_files_;
        } 
        else 
        {
            delete current_node;
            continue;
        }

        if (insert_node(dir_node, current_node) == AvlTree::Result::Error) 
        {
            throw XGDException(ErrCode::AVL_INSERT, HERE(), entry_path.string());
        }
    }
}

void AvlTree::generate_from_directory_entries(std::vector<Xiso::DirectoryEntry>& directory_entries, Node** dir_node) 
{
    for (auto it = directory_entries.begin(); it != directory_entries.end(); ) 
    {
        Node* current_node = new Node(it->filename);
        current_node->old_start_sector = it->header.start_sector;
        current_node->file_size = it->header.file_size;
        current_node->path = it->path;

        if (it->header.attributes & Xiso::ATTRIBUTE_DIRECTORY) 
        {
            std::vector<Xiso::DirectoryEntry> subdirectory_entries;
            std::string current_path_str = it->path.string();

            // Create a new vector of entries that are children of the current directory
            // Delete those entries from the original vector so they're not processed again
            auto end_it = std::remove_if(directory_entries.begin(), directory_entries.end(),
                                        [&](const Xiso::DirectoryEntry& entry) 
                                        {
                                            std::string entry_path_str = entry.path.string();
                                            if (entry_path_str.compare(0, current_path_str.length(), current_path_str) == 0 && entry_path_str != current_path_str) 
                                            {
                                                subdirectory_entries.push_back(entry);
                                                return true;
                                            }
                                            return false;
                                        });

            directory_entries.erase(end_it, directory_entries.end());

            // Call itself again with just the child entries
            if (subdirectory_entries.size() > 0) 
            {
                generate_from_directory_entries(subdirectory_entries, &current_node->subdirectory);
            }

            if (!current_node->subdirectory) 
            {
                current_node->subdirectory = EMPTY_SUBDIRECTORY;
            }
        } 
        else if (it->header.file_size > 0) 
        {
            total_bytes_ += current_node->file_size;
            ++total_files_;
        } 
        else 
        {
            delete current_node;
            ++it;
            continue;
        }

        if (insert_node(dir_node, current_node) == AvlTree::Result::Error) 
        {
            throw XGDException(ErrCode::AVL_INSERT, HERE(), "Entry path: " + it->path.string() + " Filename: " + current_node->filename);
        }

        ++it;
    }
}

// #include <cstdlib>
// #include <iostream>
// #include <filesystem>
// #include <fstream>
// #include <chrono>
// #include <random>
// #ifdef _WIN32
// #include <windows.h>
// #endif

// std::filesystem::path create_temp_directory() {
//     std::filesystem::path temp_directory;
//     std::string unique_suffix;

//     // Generate a unique suffix using a random number and current time
//     auto now = std::chrono::system_clock::now().time_since_epoch().count();
//     std::mt19937_64 rng(now);
//     std::uniform_int_distribution<uint64_t> dist;
//     unique_suffix = std::to_string(dist(rng));

// #ifdef _WIN32
//     char temp_path[MAX_PATH];
//     if (GetTempPathA(MAX_PATH, temp_path)) {
//         temp_directory = std::filesystem::path(temp_path) / ("temp_dir_" + unique_suffix);
//     } else {
//         throw std::runtime_error("Failed to get temporary directory path.");
//     }
// #else
//     const char* temp_path = std::getenv("TMPDIR");
//     if (!temp_path) {
//         temp_path = "/tmp";
//     }
//     temp_directory = std::filesystem::path(temp_path) / ("temp_dir_" + unique_suffix);
// #endif

//     if (!std::filesystem::create_directories(temp_directory)) {
//         throw std::runtime_error("Failed to create temporary directory.");
//     }

//     return temp_directory;
// }

// void AvlTree::generate_from_directory_entries(std::vector<Xiso::DirectoryEntry>& directory_entries, Node** dir_node) {
//     std::filesystem::path temp_directory = create_temp_directory();
//     auto cwd = std::filesystem::current_path();
//     std::filesystem::current_path(temp_directory);

//     for (auto entry : directory_entries) {
//         if (entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY) {
//             std::filesystem::create_directories(entry.path);
//         } else {
//             std::ofstream file(entry.path, std::ios::binary);
//             if (!file) {
//                 throw std::runtime_error("Failed to create file: " + entry.path.string());
//             }
//             file.write("XGD", 3);
//             file.close();
//         }
//     }

//     generate_from_mock_filesystem(temp_directory, directory_entries, dir_node);

//     std::filesystem::current_path(cwd);
//     std::filesystem::remove_all(temp_directory);
// }

// void AvlTree::generate_from_mock_filesystem(const std::filesystem::path& in_directory, std::vector<Xiso::DirectoryEntry>& directory_entries, Node** dir_node) {
//     for (const auto& entry : std::filesystem::directory_iterator(in_directory)) {
//         const auto& entry_path = entry.path();
//         const auto& entry_filename = entry_path.filename().string();

//         Node* current_node = new Node(entry_filename);
//         auto current_relative_path = std::filesystem::relative(entry_path, std::filesystem::current_path());
//         bool found = false;

//         for (auto& entry : directory_entries) {
//             if (entry.path == current_relative_path) {
//                 current_node->old_start_sector = entry.header.start_sector;
//                 current_node->file_size = entry.header.file_size;
//                 found = true;
//                 break;
//             }
//         }

//         if (!found) {
//             delete current_node;
//             std::cerr << "Failed to find directory entry for: " << entry_path.string() << std::endl;    
//             std::cerr << "current_relative_path: " << current_relative_path << std::endl;
//             throw std::runtime_error("Failed to find directory entry for: " + entry_path.string());
//         }

//         if (std::filesystem::is_directory(entry_path)) {
//             generate_from_mock_filesystem(entry_path, directory_entries, &current_node->subdirectory);

//             if (!current_node->subdirectory) {
//                 current_node->subdirectory = EMPTY_SUBDIRECTORY;
//             }
//         } else if (std::filesystem::is_regular_file(entry_path)) {
//             total_bytes_ += current_node->file_size;
//             ++total_files_;
//         } else {
//             delete current_node;
//             continue;
//         }

//         insert_node(dir_node, current_node);
//     }
// }