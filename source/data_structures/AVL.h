#ifndef AVL_H
#define AVL_H
#include<iostream>
#include<vector>
#include<string>
using namespace std;
template<typename T>
struct AVLNode {
    std::string key;
    T value;
    int height;
    AVLNode* left;
    AVLNode* right;
    
    AVLNode(const std::string& k, const T& v) 
        : key(k), value(v), height(1), left(nullptr), right(nullptr) {}
};

template<typename T>
class AVLTree {
private:
    AVLNode<T>* root;
    int _size;
    int getHeight(AVLNode<T>* node) {
        return node ? node->height : 0;
    }
    
    int getBalance(AVLNode<T>* node) {
        return node ? getHeight(node->left) - getHeight(node->right) : 0;
    }
    
    void updateHeight(AVLNode<T>* node) {
        if (node) {
            node->height = 1 + std::max(getHeight(node->left), getHeight(node->right));
        }
    }
    
    AVLNode<T>* rotateRight(AVLNode<T>* y) {
        AVLNode<T>* x = y->left;
        AVLNode<T>* T2 = x->right;
        
        x->right = y;
        y->left = T2;
        
        updateHeight(y);
        updateHeight(x);
        
        return x;
    }
    
    AVLNode<T>* rotateLeft(AVLNode<T>* x) {
        AVLNode<T>* y = x->right;
        AVLNode<T>* T2 = y->left;
        
        y->left = x;
        x->right = T2;
        
        updateHeight(x);
        updateHeight(y);
        
        return y;
    }
    
    AVLNode<T>* insertNode(AVLNode<T>*& node, const std::string& key, const T& value) {
        if (!node){ 
            return new AVLNode<T>(key, value);
            _size++;
        }
        
        if (key < node->key) {
            node->left = insertNode(node->left, key, value);
        } else if (key > node->key) {
            node->right = insertNode(node->right, key, value);
        } else {
            node->value = value;
            return node;
        }
        
        updateHeight(node);
        int balance = getBalance(node);
        
        // Left-Left
        if (balance > 1 && key < node->left->key) {
            return rotateRight(node);
        }
        
        // Right-Right
        if (balance < -1 && key > node->right->key) {
            return rotateLeft(node);
        }
        
        // Left-Right
        if (balance > 1 && key > node->left->key) {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }
        
        // Right-Left
        if (balance < -1 && key < node->right->key) {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }
        
        return node;
    }
    
    AVLNode<T>* findMin(AVLNode<T>* node) {
        while (node->left) node = node->left;
        return node;
    }
    
    AVLNode<T>* deleteNode(AVLNode<T>* node, const std::string& key) {
        if (!node) return nullptr;
        
        if (key < node->key) {
            node->left = deleteNode(node->left, key);
        } else if (key > node->key) {
            node->right = deleteNode(node->right, key);
        } else {
            if (!node->left || !node->right) {
                AVLNode<T>* temp = node->left ? node->left : node->right;
                if (!temp) {
                    temp = node;
                    node = nullptr;
                } else {
                    *node = *temp;
                }
                delete temp;
            } else {
                AVLNode<T>* temp = findMin(node->right);
                node->key = temp->key;
                node->value = temp->value;
                node->right = deleteNode(node->right, temp->key);
            }
        }
        
        if (!node) return nullptr;
        
        updateHeight(node);
        int balance = getBalance(node);
        
        // Left-Left
        if (balance > 1 && getBalance(node->left) >= 0) {
            return rotateRight(node);
        }
        
        // Left-Right
        if (balance > 1 && getBalance(node->left) < 0) {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }
        
        // Right-Right
        if (balance < -1 && getBalance(node->right) <= 0) {
            return rotateLeft(node);
        }
        
        // Right-Left
        if (balance < -1 && getBalance(node->right) > 0) {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }
        
        return node;
    }
    
    AVLNode<T>* searchNode(AVLNode<T>* node, const std::string& key) {
        if (!node || node->key == key) return node;
        if (key < node->key) return searchNode(node->left, key);
        return searchNode(node->right, key);
    }
    
    void collectAll(AVLNode<T>* node, std::vector<T>& result) {
        if (!node) return;
        collectAll(node->left, result);
        result.push_back(node->value);
        collectAll(node->right, result);
    }
    
    void destroyTree(AVLNode<T>* node) {
        if (!node) return;
        destroyTree(node->left);
        destroyTree(node->right);
        delete node;
    }
    
public:
    AVLTree() : root(nullptr) {}
    int size(){return this->_size;}
    ~AVLTree() {
        destroyTree(root);
    }
    AVLNode<T>* search(const std::string& key) {
        return searchNode(root, key);
    }
    void insert(const std::string& key, const T& value) {
        root = insertNode(root, key, value);
    }
    
    bool find(const std::string& key, T& value) {
        AVLNode<T>* node = searchNode(root, key);
        if (node) {
            value = node->value;
            return true;
        }
        return false;
    }
    
    bool remove(const std::string& key) {
        if (!searchNode(root, key)) return false;
        root = deleteNode(root, key);
        return true;
    }
    
    std::vector<T> getAllValues() {
        std::vector<T> result;
        collectAll(root, result);
        return result;
    }
    
    bool exists(const std::string& key) {
        return searchNode(root, key) != nullptr;
    }
};
#endif