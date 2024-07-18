#include "XGD.h"
#include "AvlTree/AvlTree.h"

void AvlTree::traverse(Node* root, TraversalMethod method, const TraversalCallback callback, void* context, int depth) {
    if (!root || root == EMPTY_SUBDIRECTORY) {
        return;
    }
    switch (method) {
        case TraversalMethod::PREFIX:
            callback(root, context, depth);
            traverse(root->left_child, method, callback, context, depth + 1);
            traverse(root->right_child, method, callback, context, depth + 1);
            break;
        case TraversalMethod::INFIX:
            traverse(root->left_child, method, callback, context, depth + 1);
            callback(root, context, depth);
            traverse(root->right_child, method, callback, context, depth + 1);
            break;
        case TraversalMethod::POSTFIX:
            traverse(root->left_child, method, callback, context, depth + 1);
            traverse(root->right_child, method, callback, context, depth + 1);	
            callback(root, context, depth);
            break;
        default:
            break;
    }
}

void AvlTree::print_tree(Node* node, void* context, int depth) {
    if (!node || node == EMPTY_SUBDIRECTORY) {
        return;
    }

    std::cerr   << "Depth: " << depth 
                << "   Name: " << node->filename 
                << "   Size: " << node->file_size 
                << "   Start sector: " << node->start_sector << std::endl;

    if (node->subdirectory) {
        traverse(node->subdirectory, TraversalMethod::POSTFIX, [this](Node* node, void* context, int depth) {
            print_tree(node, context, depth);
        }, nullptr, 0);
    }
}

void AvlTree::print_tree_info() {
    if (!root_.subdirectory || root_.subdirectory == EMPTY_SUBDIRECTORY) {
        return;
    }
    traverse(root_.subdirectory, TraversalMethod::POSTFIX, [this](Node* node, void* context, int depth) {
        print_tree(node, context, depth);
    }, nullptr, 0);
}

// Checks if file size and start sector will overflow uint32_t
void AvlTree::verify_tree(Node* node, void* context, int depth) {
    if (!node || node == EMPTY_SUBDIRECTORY) {
        return;
    }
    if (node->file_size > UINT32_MAX) {
        throw XGDException(ErrCode::AVL_SIZE, HERE(), "File size exceeds maximum value: " + node->filename + " (" + std::to_string(node->file_size) + ")");
    }
    if (node->start_sector > UINT32_MAX) {
        throw XGDException(ErrCode::AVL_SIZE, HERE(), "Start sector exceeds maximum value: " + node->filename + " (" + std::to_string(node->start_sector) + ")");
    }
    if (node->subdirectory) {
        traverse(node->subdirectory, TraversalMethod::PREFIX, [this](Node* node, void* context, int depth) {
            verify_tree(node, context, depth);
        }, nullptr, 0);
    }
}

void AvlTree::collect_nodes(AvlTree::Node* node, std::vector<AvlTree::Node*>* context, int depth) {
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