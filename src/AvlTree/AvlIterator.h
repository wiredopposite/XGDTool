#ifndef _AVL_ITERATOR_H_
#define _AVL_ITERATOR_H_

#include <cstdint>
#include <vector>

#include "AvlTree/AvlTree.h"

/*  Collects Avl nodes and creates an array with each entry's absolute offset in the out file.
    Nodes are repeated in the array, one version for its directory table entry and one for its 
    file entry. These entries can then be iterated through instead of using recursion, so that 
    everything can be written sector by sector. Space between entries should be padded with 0xFF.  */
class AvlIterator 
{
public: 
    struct Entry {
        uint64_t offset;
        bool directory_entry;
        AvlTree::Node* node;
    };

    AvlIterator(AvlTree& avl_tree);
    ~AvlIterator() = default;   

    const std::vector<Entry>& entries() const { return avl_entries_; }

private:
    std::vector<Entry> avl_entries_;
    
    void collect_nodes(AvlTree::Node* node, std::vector<AvlTree::Node*>* context, int depth);
};

#endif // _AVL_ITERATOR_H_