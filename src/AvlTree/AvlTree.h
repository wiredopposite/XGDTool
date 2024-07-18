#ifndef _AVLTREE_H_
#define _AVLTREE_H_

#include <iostream>
#include <string>
#include <filesystem>
#include <functional>

#include "Formats/Xiso.h"

#define EMPTY_SUBDIRECTORY (reinterpret_cast<AvlTree::Node*>(1))

/*  Class will construct an AVL tree from a vector of Xiso::DirectoryEntry structs or a filesystem directory,
    as well as calculate the required directory size and offsets for each directory node for use in an ISO.
    The Traverse method is provided so it can be used with a custom TraversalCallback, used to perform file IO. */
class AvlTree {
public:
    enum class Skew { NONE, LEFT, RIGHT };

    struct Node {
        uint64_t offset{0};
        uint64_t directory_start{0};
        std::string filename;
        uint64_t file_size{0};

        uint64_t start_sector{0};
        uint64_t old_start_sector{0};

        Skew skew{Skew::NONE};

        Node* subdirectory{nullptr};
        Node* left_child{nullptr};
        Node* right_child{nullptr};

        std::filesystem::path path; // Abs path if created from filesystem, otherwise relative to root

        Node(const std::string& name) : filename(name) {}
        ~Node() {
            if (subdirectory && subdirectory != EMPTY_SUBDIRECTORY) {
                delete subdirectory;
            }
            if (left_child) {
                delete left_child;
            }
            if (right_child) {
                delete right_child;
            }
        }
    };

    using TraversalCallback = std::function<void(Node*, void*, int)>;
    enum class TraversalMethod { PREFIX, INFIX, POSTFIX };
    
    AvlTree(const std::string& root_name, std::vector<Xiso::DirectoryEntry> directory_entries); 
    AvlTree(const std::string& root_name, const std::filesystem::path& root_directory);

    Node* root() { return &root_; }

    static void traverse(Node* root, TraversalMethod method, const TraversalCallback callback, void* context, int depth);
    // void collect_nodes(AvlTree::Node* node, std::vector<AvlTree::Node*>* context, int depth);

    void print_tree_info();

    uint64_t total_bytes() { return total_bytes_; }
    uint32_t total_files() { return total_files_; }
    uint64_t out_iso_size();
    
private:
    enum class Result { BALANCED, NO_ERROR, ERROR };

    struct AssignOffsetsContext {
        uint64_t directory_start;
        uint64_t* current_sector;
    };

    Node root_;

    uint64_t total_bytes_{0};
    uint32_t total_files_{0};
    uint64_t out_iso_size_{0};

    Result insert_node(Node** root_node, Node* node);
    Result left_grown(Node** node);
    Result right_grown(Node** node);
    void rotate_left(Node** node);
    void rotate_right(Node** node);
    int compare_key(const std::string& lhs, const std::string& rhs);
    // int compare_key(const  char *in_lhs, const char *in_rhs );

    void print_tree(Node* node, void* context, int depth);
    uint64_t num_sectors(uint64_t bytes);

    void generate_from_filesystem(const std::filesystem::path& in_directory, Node** dir_node);
    void generate_from_directory_entries(std::vector<Xiso::DirectoryEntry>& directory_entries, Node** dir_node); 
    // void generate_from_mock_filesystem(const std::filesystem::path& in_directory, std::vector<Xiso::DirectoryEntry>& directory_entries, Node** dir_node);
    // void generate_from_hierarchy(const std::shared_ptr<DirectoryNode>& node, Node** dir_node);

    void calculate_directory_requirements(Node* node, void* context, int depth);
    void calculate_directory_offsets(Node* node, uint64_t* current_sector, int depth);
    void calculate_directory_size(Node* node, uint64_t* out_size, int depth);
    void assign_offsets(Node* node, AssignOffsetsContext* ao_context, int depth);
    void calculate_all();
 
    void verify_tree(Node* node, void* context, int depth); 
    void collect_nodes(AvlTree::Node* node, std::vector<AvlTree::Node*>* context, int depth);
    uint64_t calculate_iso_size(AvlTree::Node* root_node);
};

/*  The AVL tree is constructed by creating a separate tree for each directory within the root directory,
    each separate tree's root node is assigned to it's parent directory node's subdirectory pointer.
    "Child" nodes are actually sibling file/dirs of their "parent" node within the same directory.

    If a node's subdirectory pointer is nullptr, it's a file node.
    If a directory is empty, it's subdirectory pointer is set to EMPTY_SUBDIRECTORY to distignuish it from a file node.
    See AvlTree::generate_from_filesystem to see what's going on.

    Take this file structure:
    root/
    ├── file1
    ├── file2
    └── file3

    The tree would look like this:
            root node
               |
          subdirectory(file1)
           /         \
    child(file2)   child(file3)

    root/
    ├── dir1/
    │   ├── file1
    │   └── file2
    └── dir2/
        ├── file3
        ├── file4
        └── file5

    That tree would look like this:
                        root node
                           |
                   subdirectory(dir1)
                    |              \
        subdirectory(file1)     child(dir2)
            /                        |
    child(file2)               subdirectory(file3)
                                /            \
                        child(file4)       child(file5)
*/

#endif // _AVLTREE_H_