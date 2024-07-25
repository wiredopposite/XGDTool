#include <algorithm>

#include "AvlTree/AvlIterator.h"

AvlIterator::AvlIterator(AvlTree& avl_tree) 
{
    std::vector<AvlTree::Node*> avl_nodes;

    AvlTree::traverse<std::vector<AvlTree::Node*>>(avl_tree.root()->subdirectory, AvlTree::TraversalMethod::PREFIX, 
        [this](AvlTree::Node* node, std::vector<AvlTree::Node*>* context, int depth) {
            collect_nodes(node, context, depth);
        }, &avl_nodes, 0);

    for (auto node : avl_nodes) 
    {
        if (!node->subdirectory) 
        {
            avl_entries_.push_back({ node->start_sector * Xiso::SECTOR_SIZE, false, node }); // file nodes
        }

        avl_entries_.push_back({ node->directory_start + node->offset, true, node }); // directory table nodes
    }

    std::sort(avl_entries_.begin(), avl_entries_.end(), [](const Entry& a, const Entry& b) 
    {
        return a.offset < b.offset;
    });
}

void AvlIterator::collect_nodes(AvlTree::Node* node, std::vector<AvlTree::Node*>* context, int depth) 
{
    if (!node || node == EMPTY_SUBDIRECTORY) 
    {
        return;
    }

    context->push_back(node);

    if (node->subdirectory) 
    {
        AvlTree::traverse<std::vector<AvlTree::Node*>>(node->subdirectory, AvlTree::TraversalMethod::PREFIX, 
            [this](AvlTree::Node* node, std::vector<AvlTree::Node*>* context, int depth) {
                collect_nodes(node, context, depth);
            }, context, 0);
    }
} 