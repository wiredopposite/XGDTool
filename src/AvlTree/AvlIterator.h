#ifndef _AVL_ITERATOR_H_
#define _AVL_ITERATOR_H_

#include "AvlTree/AvlTree.h"

class AvlIterator {
public: 
    struct Entry {
        uint64_t offset;
        bool directory_entry;
        AvlTree::Node* node;
    };

    AvlIterator(AvlTree& avl_tree) {
        std::vector<AvlTree::Node*> avl_nodes;

        AvlTree::traverse(avl_tree.root()->subdirectory, AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
            collect_nodes(node, static_cast<std::vector<AvlTree::Node*>*>(context), depth);
        }, &avl_nodes, 0);

        for (auto node : avl_nodes) {
            if (!node->subdirectory) {
                avl_entries_.push_back({ node->start_sector * Xiso::SECTOR_SIZE, false, node }); // file nodes
            }

            avl_entries_.push_back({ node->directory_start + node->offset, true, node }); // directory nodes
        }

        std::sort(avl_entries_.begin(), avl_entries_.end(), [](const Entry& a, const Entry& b) {
            return a.offset < b.offset;
        });
    }

    const std::vector<Entry>& entries() const { return avl_entries_; }

private:
    std::vector<Entry> avl_entries_;
    
    void collect_nodes(AvlTree::Node* node, std::vector<AvlTree::Node*>* context, int depth) {
        if (!node || node == EMPTY_SUBDIRECTORY) {
            return;
        }

        context->push_back(node);

        if (node->subdirectory) {
            AvlTree::traverse(node->subdirectory, AvlTree::TraversalMethod::PREFIX, [this](AvlTree::Node* node, void* context, int depth) {
                collect_nodes(node, static_cast<std::vector<AvlTree::Node*>*>(context), depth);
            }, context, 0);
        }
    }  
};

#endif // _AVL_ITERATOR_H_