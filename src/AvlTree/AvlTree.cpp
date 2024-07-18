#include "XGD.h"
#include "AvlTree/AvlTree.h"

AvlTree::AvlTree(const std::string& root_name, std::vector<Xiso::DirectoryEntry> directory_entries) :
    root_(root_name) {
        
    root_.start_sector = Xiso::ROOT_DIRECTORY_SECTOR;
    generate_from_directory_entries(directory_entries, &root_.subdirectory);
	directory_entries.clear();
    calculate_all();
}

AvlTree::AvlTree(const std::string& root_name, const std::filesystem::path& root_directory) :
    root_(root_name) {

    root_.start_sector = Xiso::ROOT_DIRECTORY_SECTOR;
    generate_from_filesystem(root_directory, &root_.subdirectory);
    calculate_all();
}

// Calculated total resulting ISO size in bytes
uint64_t AvlTree::out_iso_size() {
	if (out_iso_size_ == 0) {
		out_iso_size_ = calculate_iso_size(&root_);
	}
	return out_iso_size_;
}

AvlTree::Result AvlTree::insert_node(Node** root_node, Node* node) {
    if (*root_node == nullptr) {
        *root_node = node;
        return Result::BALANCED;
    }

    int key_result = compare_key(node->filename, (*root_node)->filename);

    if (key_result < 0) {
        Result avl_result = insert_node(&(*root_node)->left_child, node);
        return (avl_result == Result::BALANCED) ? left_grown(root_node) : avl_result;
    }
    if (key_result > 0) {
        Result avl_result = insert_node(&(*root_node)->right_child, node);
        return (avl_result == Result::BALANCED) ? right_grown(root_node) : avl_result;
    }
    return Result::ERROR;
}

AvlTree::Result AvlTree::left_grown(Node** node) {
    switch ((*node)->skew) {
        case Skew::LEFT: 
            if ((*node)->left_child->skew == Skew::LEFT) {
                (*node)->skew = (*node)->left_child->skew = Skew::NONE;
                rotate_right(node);
            } else {
                switch ((*node)->left_child->right_child->skew) {
                    case Skew::LEFT:
                        (*node)->skew = Skew::RIGHT;
                        (*node)->left_child->skew = Skew::NONE;
                        break;
                    case Skew::RIGHT:
                        (*node)->skew = Skew::NONE;
                        (*node)->left_child->skew = Skew::LEFT;
                        break;
                    default:
                        (*node)->skew = Skew::NONE;
                        (*node)->left_child->skew = Skew::NONE;
                        break;
                }
                (*node)->left_child->right_child->skew = Skew::NONE;
                rotate_left(&(*node)->left_child);
                rotate_right(node);
            }
            return Result::NO_ERROR;
        case Skew::RIGHT:
            (*node)->skew = Skew::NONE;
            return Result::NO_ERROR;
        default:
            (*node)->skew = Skew::LEFT;
            return Result::BALANCED;
    }
}

AvlTree::Result AvlTree::right_grown(Node** node) {
    switch ((*node)->skew) {
        case Skew::LEFT:
            (*node)->skew = Skew::NONE;
            return Result::NO_ERROR;
        case Skew::RIGHT:
            if ((*node)->right_child->skew == Skew::RIGHT) {
                (*node)->skew = (*node)->right_child->skew = Skew::NONE;
                rotate_left(node);
            } else {
                switch ((*node)->right_child->left_child->skew) {
                    case Skew::LEFT:
                        (*node)->skew = Skew::NONE;
                        (*node)->right_child->skew = Skew::RIGHT;
                        break;
                    case Skew::RIGHT:
                        (*node)->skew = Skew::LEFT;
                        (*node)->right_child->skew = Skew::NONE;
                        break;
                    default:
                        (*node)->skew = Skew::NONE;
                        (*node)->right_child->skew = Skew::NONE;
                        break;
                }
                (*node)->right_child->left_child->skew = Skew::NONE;
                rotate_right(&(*node)->right_child);
                rotate_left(node);
            }
            return Result::NO_ERROR;
        default:
            (*node)->skew = Skew::RIGHT;
            return Result::BALANCED;
    }
}

void AvlTree::rotate_left(Node** node) {
    Node* tmp = *node;
    *node = (*node)->right_child;
    tmp->right_child = (*node)->left_child;
    (*node)->left_child = tmp;
}

void AvlTree::rotate_right(Node** node) {
    Node* tmp = *node;
    *node = (*node)->left_child;
    tmp->left_child = (*node)->right_child;
    (*node)->right_child = tmp;
}

int AvlTree::compare_key(const std::string& lhs, const std::string& rhs) {
    auto it1 = lhs.begin();
    auto it2 = rhs.begin();

    while (it1 != lhs.end() || it2 != rhs.end()) {
        char a = (it1 != lhs.end()) ? *it1++ : '\0';
        char b = (it2 != rhs.end()) ? *it2++ : '\0';

        if (a >= 'a' && a <= 'z') {
            a -= 32;  // convert to uppercase
        }

        if (b >= 'a' && b <= 'z') {
            b -= 32;  // convert to uppercase
        }

        if (a) {
            if (b) {
                if (a < b) return -1;
                if (a > b) return 1;
            } else {
                return 1;
            }
        } else {
            return b ? -1 : 0;
        }
    }

    return 0;
}

// AvlTree::Result AvlTree::insert_node( Node **in_root, Node *in_node ) {
// 	Result tmp;
// 	int result;
	
// 	if ( *in_root == nullptr ) { *in_root = in_node; return Result::BALANCED; }

// 	result = compare_key( in_node->filename.c_str(), (*in_root)->filename.c_str() );
	
// 	if ( result < 0 ) return ( tmp = insert_node( &(*in_root)->left_child, in_node ) ) == Result::BALANCED ? left_grown( in_root ) : tmp;
// 	if ( result > 0 ) return ( tmp = insert_node( &(*in_root)->right_child, in_node ) ) == Result::BALANCED ? right_grown( in_root ) : tmp;
	
// 	return Result::ERROR;
// }


// AvlTree::Result AvlTree::left_grown( Node **in_root ) {
// 	switch ( (*in_root)->skew ) {
// 		case Skew::LEFT: {
// 			if ( (*in_root)->left_child->skew == Skew::LEFT ) {
// 				(*in_root)->skew = (*in_root)->left_child->skew = Skew::NONE;
// 				rotate_right( in_root );
// 			} else {
// 				switch ( (*in_root)->left_child->right_child->skew ) {
// 					case Skew::LEFT: {
// 						(*in_root)->skew = Skew::RIGHT;
// 						(*in_root)->left_child->skew = Skew::NONE;
// 					} break;
					
// 					case Skew::RIGHT: {
// 						(*in_root)->skew = Skew::NONE;
// 						(*in_root)->left_child->skew = Skew::LEFT;
// 					} break;
					
// 					default: {
// 						(*in_root)->skew = Skew::NONE;
// 						(*in_root)->left_child->skew = Skew::NONE;
// 					} break;
// 				}
// 				(*in_root)->left_child->right_child->skew = Skew::NONE;
// 				rotate_left( &(*in_root)->left_child );
// 				rotate_right( in_root );
// 			}
// 		} return Result::NO_ERROR;
		
// 		case Skew::RIGHT: {
// 			(*in_root)->skew = Skew::NONE;
// 		} return Result::NO_ERROR;
		
// 		default: {
// 			(*in_root)->skew = Skew::LEFT;
// 		} return Result::BALANCED;
// 	}
// }


// AvlTree::Result AvlTree::right_grown( Node **in_root ) {
// 	switch ( (*in_root)->skew ) {
// 		case Skew::LEFT: {
// 			(*in_root)->skew = Skew::NONE;
// 		} return Result::NO_ERROR;
		
// 		case Skew::RIGHT: {
// 			if ( (*in_root)->right_child->skew == Skew::RIGHT ) {
// 				(*in_root)->skew = (*in_root)->right_child->skew = Skew::NONE;
// 				rotate_left( in_root );
// 			} else {
// 				switch ( (*in_root)->right_child->left_child->skew ) {
// 					case Skew::LEFT: {
// 						(*in_root)->skew = Skew::NONE;
// 						(*in_root)->right_child->skew = Skew::RIGHT;
// 					} break;
					
// 					case Skew::RIGHT: {
// 						(*in_root)->skew = Skew::LEFT;
// 						(*in_root)->right_child->skew = Skew::NONE;
// 					} break;
					
// 					default: {
// 						(*in_root)->skew = Skew::NONE;
// 						(*in_root)->right_child->skew = Skew::NONE;
// 					} break;
// 				}
// 				(*in_root)->right_child->left_child->skew = Skew::NONE;
// 				rotate_right( &(*in_root)->right_child );
// 				rotate_left( in_root );
// 			}
// 		} return Result::NO_ERROR;
		
// 		default: {
// 			(*in_root)->skew = Skew::RIGHT;
// 		} return Result::BALANCED;
// 	}
// }


// void AvlTree::rotate_left( Node **in_root ) {
// 	Node *tmp = *in_root;
	
// 	*in_root = (*in_root)->right_child;
// 	tmp->right_child = (*in_root)->left_child;
// 	(*in_root)->left_child = tmp;
// }


// void AvlTree::rotate_right( Node **in_root ) {
// 	Node *tmp = *in_root;
	
// 	*in_root = (*in_root)->left_child;
// 	tmp->left_child = (*in_root)->right_child;
// 	(*in_root)->right_child = tmp;
// }


// int AvlTree::compare_key(const char *in_lhs, const char *in_rhs ) {
// 	char a, b;

// 	for ( ;; ) {
// 		a = *in_lhs++;
// 		b = *in_rhs++;
		
// 		if ( a >= 'a' && a <= 'z' ) a -= 32;	// uppercase(a);
// 		if ( b >= 'a' && b <= 'z' ) b -= 32;	// uppercase(b);
		
// 		if ( a ) {
// 			if ( b ) {
// 				if ( a < b ) return -1;
// 				if ( a > b ) return 1;
// 			} else return 1;
// 		} else return b ? -1 : 0;
// 	}
// }