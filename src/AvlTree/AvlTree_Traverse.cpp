#include "XGD.h"
#include "AvlTree/AvlTree.h"

void AvlTree::print_tree(Node* node, void* context, int depth) 
{
    if (!node || node == EMPTY_SUBDIRECTORY) 
    {
        return;
    }

    std::cerr   << "Depth: " << depth 
                << "   Name: " << node->filename 
                << "   Size: " << node->file_size 
                << "   Start sector: " << node->start_sector << std::endl;

    if (node->subdirectory) 
    {
        traverse<void>(node->subdirectory, TraversalMethod::POSTFIX, 
            [this](Node* node, void* context, int depth) {
                print_tree(node, context, depth);
            }, nullptr, depth);
    }
}

void AvlTree::print_tree_info() 
{
    if (!root_.subdirectory || root_.subdirectory == EMPTY_SUBDIRECTORY) 
    {
        return;
    }

    traverse<void>(root_.subdirectory, TraversalMethod::POSTFIX, 
        [this](Node* node, void* context, int depth) {
            print_tree(node, context, depth);
        }, nullptr, 0);
}

// Checks if file size and start sector will overflow uint32_t
void AvlTree::verify_tree(Node* node, void* context, int depth) 
{
    if (!node || node == EMPTY_SUBDIRECTORY) 
    {
        return;
    }
    if (node->file_size > UINT32_MAX) 
    {
        throw XGDException(ErrCode::AVL_SIZE, HERE(), "File size exceeds maximum value: " + node->filename + " (" + std::to_string(node->file_size) + ")");
    }
    if (node->start_sector > UINT32_MAX) 
    {
        throw XGDException(ErrCode::AVL_SIZE, HERE(), "Start sector exceeds maximum value: " + node->filename + " (" + std::to_string(node->start_sector) + ")");
    }
    if (node->subdirectory) 
    {
        traverse<void>(node->subdirectory, TraversalMethod::PREFIX, 
            [this](Node* node, void* context, int depth) {
                verify_tree(node, context, depth);
            }, nullptr, 0);
    }
}

void AvlTree::collect_nodes(AvlTree::Node* node, std::vector<AvlTree::Node*>* context, int depth) 
{
    if (!node || node == EMPTY_SUBDIRECTORY) 
    {
        return;
    }

    context->push_back(node);

    if (node->subdirectory) 
    {
        traverse<std::vector<AvlTree::Node*>>(node->subdirectory, AvlTree::TraversalMethod::PREFIX, 
            [this](AvlTree::Node* node, std::vector<AvlTree::Node*>* context, int depth) {
                collect_nodes(node, context, depth);
            }, context, 0);
    }
}