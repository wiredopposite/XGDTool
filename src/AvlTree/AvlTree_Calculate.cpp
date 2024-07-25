#include <algorithm>

#include "XGD.h"
#include "AvlTree/AvlTree.h"

uint64_t AvlTree::num_sectors(uint64_t bytes) 
{ 
    return bytes / Xiso::SECTOR_SIZE + ((bytes % Xiso::SECTOR_SIZE) ? 1 : 0);
}

void AvlTree::calculate_directory_requirements(Node* node, void* context, int depth) 
{
    if (!node->subdirectory) 
    {
        return;
    }

    if (node->subdirectory != EMPTY_SUBDIRECTORY) 
    {
        traverse<uint64_t>(node->subdirectory, TraversalMethod::PREFIX, 
            [this](Node* node, uint64_t* context, int depth) {
                calculate_directory_size(node, context, depth);
            }, &node->file_size, 0);
        
        traverse<void>(node->subdirectory, TraversalMethod::PREFIX, 
            [this](Node* node, void* context, int depth) {
                calculate_directory_requirements(node, context, depth);
            }, context, 0);
    } 
    else 
    {
        node->file_size = Xiso::SECTOR_SIZE;
    }
}

void AvlTree::calculate_directory_offsets(Node* node, uint64_t* current_sector, int depth) 
{
    if (!node->subdirectory) 
    {
        return;
    }

    if (node->subdirectory == EMPTY_SUBDIRECTORY) 
    {
        node->start_sector = *current_sector;
        *current_sector += 1;
    } 
    else 
    {
        AssignOffsetsContext ao_context;
        ao_context.current_sector = current_sector;
        ao_context.directory_start = (node->start_sector = *current_sector) * Xiso::SECTOR_SIZE;

        *current_sector += num_sectors(node->file_size);

        traverse<AssignOffsetsContext>(node->subdirectory, TraversalMethod::PREFIX, 
            [this](Node* node, AssignOffsetsContext* context, int depth) {
                assign_offsets(node, context, depth);
            }, &ao_context, 0);
        
        traverse<uint64_t>(node->subdirectory, TraversalMethod::PREFIX, 
            [this](Node* node, uint64_t* context, int depth) {
                calculate_directory_offsets(node, context, depth);
            }, current_sector, 0);
    }
}

void AvlTree::calculate_directory_size(Node* node, uint64_t* out_size, int depth) {
    if (depth == 0) 
    {
        *out_size = 0;
    }

    uint32_t length = sizeof(Xiso::DirectoryEntry::Header) + static_cast<uint32_t>(node->filename.size());
    length += (sizeof(uint32_t) - (length % sizeof(uint32_t))) % sizeof(uint32_t);

    if (num_sectors(*out_size + length) > num_sectors(*out_size)) 
    {
        *out_size += (Xiso::SECTOR_SIZE - (*out_size % Xiso::SECTOR_SIZE)) % Xiso::SECTOR_SIZE;
    }

    node->offset = *out_size;
    *out_size += length;
}

void AvlTree::assign_offsets(Node* node, AssignOffsetsContext* ao_context, int depth) 
{
    node->directory_start = ao_context->directory_start;

    if (!node->subdirectory) 
    {
        node->start_sector = *ao_context->current_sector;
        *ao_context->current_sector += num_sectors(node->file_size);
    }
}

void AvlTree::calculate_all() {
    uint64_t start_sector = root_.start_sector;

    traverse<void>(&root_, TraversalMethod::PREFIX, 
        [this](Node* node, void* context, int depth) {
            calculate_directory_requirements(node, context, depth);
        }, nullptr, 0);

    traverse<uint64_t>(&root_, TraversalMethod::PREFIX, 
        [this](Node* node, uint64_t* context, int depth) {
            calculate_directory_offsets(node, context, depth);
        }, &start_sector, 0);

    traverse<void>(&root_, TraversalMethod::PREFIX, 
        [this](Node* node, void* context, int depth) {
            verify_tree(node, context, depth);
        }, nullptr, 0);

    XGDLog(Debug) << "Tree verified, no values exceed maximum" << XGDLog::Endl;
}

uint64_t AvlTree::calculate_iso_size(Node* root_node) 
{
    std::vector<Node*> avl_nodes;

    traverse<std::vector<Node*>>(root_node, TraversalMethod::PREFIX, 
        [this](Node* node, std::vector<Node*>* context, int depth) {
            collect_nodes(node, context, depth);
        }, &avl_nodes, 0);

    auto max_it = std::max_element(avl_nodes.begin(), avl_nodes.end(), [](Node* a, Node* b) 
    {
        return a->start_sector < b->start_sector;
    });

    uint64_t out_iso_size = 0;

    if (max_it != avl_nodes.end()) 
    {
        out_iso_size = (*max_it)->start_sector * Xiso::SECTOR_SIZE;
        out_iso_size += (*max_it)->file_size;
        if (out_iso_size % Xiso::SECTOR_SIZE) 
        {
            out_iso_size += Xiso::SECTOR_SIZE - (out_iso_size % Xiso::SECTOR_SIZE);
        }
    } 
    else 
    {
        throw XGDException(ErrCode::ISO_INVALID, HERE(), "Input XISO contains no files");
    }

    std::sort(avl_nodes.begin(), avl_nodes.end(), [](Node* a, Node* b) 
    {
        return a->directory_start < b->directory_start;
    });

    uint64_t current_dir_start = 0;
    uint64_t offset_in_file = 0;

    for (const auto& node : avl_nodes) 
    {
        if (node->directory_start != current_dir_start) 
        {
            offset_in_file = node->directory_start * Xiso::SECTOR_SIZE;
            current_dir_start = node->directory_start;
        }

        uint64_t padding_len = node->offset + node->directory_start - offset_in_file;
        uint64_t entry_size = (2 * sizeof(uint16_t)) + (2 * sizeof(uint32_t)) + (2 * sizeof(uint8_t)) + std::min(node->filename.size(), static_cast<size_t>(UINT8_MAX));
        offset_in_file += padding_len + entry_size;

        if (offset_in_file > out_iso_size) 
        {
            out_iso_size = offset_in_file;
        }
    }

    if (out_iso_size % Xiso::FILE_MODULUS) 
    {
        out_iso_size += Xiso::FILE_MODULUS - (out_iso_size % Xiso::FILE_MODULUS);
    }

    return out_iso_size;
}