#ifndef OFS_IMPLEMENTATION_H
#define OFS_IMPLEMENTATION_H

#include "/home/hanan/Projects/c++/file-verse/source/include/odf_types.hpp"
#include "/home/hanan/Projects/c++/file-verse/source/data_structures/AVL.h"
#include "/home/hanan/Projects/c++/file-verse/source/data_structures/bitmap.h"
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <sstream>

using namespace std;

// Block structure: [next_block_offset (8 bytes)][data (block_size - 8 bytes)]
struct BlockHeader {
    uint64_t next_block_offset;  // 0 means no next block (last block)
};

struct Config {
    uint64_t total_size;
    uint64_t header_size;
    uint64_t block_size;
    uint32_t max_files;
    uint32_t max_filename_length;
    uint32_t max_users;
    std::string admin_username;
    std::string admin_password;
    bool require_auth;
    uint32_t port;
    uint32_t max_connections;
    uint32_t queue_timeout;
};

class ConfigParser {
public:
    static bool parse(const std::string& config_path, Config& config) {
        std::ifstream file(config_path);
        if (!file.is_open()) return false;

        std::string line;
        while (std::getline(file, line)) {
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }
            
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            if (line.empty() || line[0] == '[') continue;
            
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos) continue;
            
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            if (!value.empty() && value[0] == '"') {
                value = value.substr(1, value.length() - 2);
            }
            
            if (key == "total_size") config.total_size = std::stoull(value);
            else if (key == "header_size") config.header_size = std::stoull(value);
            else if (key == "block_size") config.block_size = std::stoull(value);
            else if (key == "max_files") config.max_files = std::stoul(value);
            else if (key == "max_filename_length") config.max_filename_length = std::stoul(value);
            else if (key == "max_users") config.max_users = std::stoul(value);
            else if (key == "admin_username") config.admin_username = value;
            else if (key == "admin_password") config.admin_password = value;
            else if (key == "require_auth") config.require_auth = (value == "true");
            else if (key == "port") config.port = std::stoul(value);
            else if (key == "max_connections") config.max_connections = std::stoul(value);
            else if (key == "queue_timeout") config.queue_timeout = std::stoul(value);
        }
        
        return true;
    }
};

struct Inode {
    uint32_t inode_number;
    EntryType type;
    uint64_t size;
    uint32_t permissions;
    uint64_t created_time;
    uint64_t modified_time;
    char owner[32];
    uint32_t parent_inode;
    uint64_t first_block_offset;  // Offset to first data block in file
    std::vector<uint32_t> child_inodes;
    char name[256];
    
    Inode() : inode_number(0), type(EntryType::FILE), size(0), 
              permissions(0644), created_time(0), modified_time(0), 
              parent_inode(0), first_block_offset(0) {
        std::memset(owner, 0, sizeof(owner));
        std::memset(name, 0, sizeof(name));
    }
};

class OFSInstance {
private:
    std::string omni_path;
    Config config;
    OMNIHeader header;
    std::fstream file;
    bool isFormat;
    AVLTree<UserInfo> users;
    std::vector<Inode> inodes;
    AVLTree<uint32_t> path_to_inode;
    Bitmap* free_blocks;
    
    uint32_t next_inode_number;
    uint32_t root_inode;
    uint64_t data_region_start;  // Where data blocks begin
    
    uint32_t allocateInode() {
        return next_inode_number++;
    }
    
    // Calculate offset for a block number (blocks start after metadata tables)
    uint64_t getBlockOffset(uint32_t block_num) {
        return data_region_start + (block_num * header.block_size);
    }
    
    // Get block number from offset
    uint32_t getBlockNumber(uint64_t offset) {
        if (offset < data_region_start) return 0;
        return (offset - data_region_start) / header.block_size;
    }
    
    // Allocate a new block and return its offset
    uint64_t allocateBlock() {
        std::vector<uint32_t> blocks = free_blocks->findFreeBlocks(1);
        if (blocks.empty()) return 0;
        
        free_blocks->setBlockUsed(blocks[0]);
        uint64_t offset = getBlockOffset(blocks[0]);
        
        // Verify offset is in valid range
        if (offset < data_region_start || offset >= header.total_size) {
            return 0;
        }
        
        return offset;
    }
    
    // Free a block given its offset
    void freeBlock(uint64_t offset) {
        if (offset < data_region_start || offset >= header.total_size) return;
        uint32_t block_num = getBlockNumber(offset);
        free_blocks->setBlockFree(block_num);
    }
    
    // Write a single block at the given offset
    bool writeBlock(uint64_t block_offset, const BlockHeader& header, const char* data, size_t data_size) {
        if (block_offset < data_region_start || block_offset >= this->header.total_size) {
            return false;
        }
        
        // Write block header
        file.seekp(block_offset);
        file.write(reinterpret_cast<const char*>(&header), sizeof(BlockHeader));
        
        // Write data
        if (data && data_size > 0) {
            file.write(data, data_size);
        }
        
        return file.good();
    }
    
    // Read a single block at the given offset
    bool readBlock(uint64_t block_offset, BlockHeader& header, char* data, size_t data_size) {
        if (block_offset < data_region_start || block_offset >= this->header.total_size) {
            return false;
        }
        
        // Read block header
        file.seekg(block_offset);
        file.read(reinterpret_cast<char*>(&header), sizeof(BlockHeader));
        
        // Read data
        if (data && data_size > 0) {
            file.read(data, data_size);
        }
        
        return file.good();
    }
    
    // Write data to blocks with linked allocation
    uint64_t writeDataToBlocks(const char* data, size_t size) {
        if (size == 0) return 0;
        
        uint64_t first_block_offset = allocateBlock();
        if (first_block_offset == 0) return 0;
        
        uint64_t current_offset = first_block_offset;
        size_t written = 0;
        size_t data_per_block = header.block_size - sizeof(BlockHeader);
        
        while (written < size) {
            size_t to_write = std::min(data_per_block, size - written);
            
            // Determine if we need another block
            uint64_t next_offset = 0;
            if (written + to_write < size) {
                next_offset = allocateBlock();
                if (next_offset == 0) {
                    // Failed to allocate, free what we've allocated
                    freeBlockChain(first_block_offset);
                    return 0;
                }
            }
            
            // Prepare block header with next pointer
            BlockHeader bh;
            bh.next_block_offset = next_offset;
            
            // Write block (header + data)
            if (!writeBlock(current_offset, bh, data + written, to_write)) {
                freeBlockChain(first_block_offset);
                return 0;
            }
            
            written += to_write;
            current_offset = next_offset;
        }
        
        file.flush();
        return first_block_offset;
    }
    
    // Read data from linked blocks
    bool readDataFromBlocks(uint64_t first_block_offset, char* buffer, size_t size) {
        if (first_block_offset == 0 || size == 0) return false;
        if (first_block_offset < data_region_start) return false;
        
        uint64_t current_offset = first_block_offset;
        size_t read_total = 0;
        size_t data_per_block = header.block_size - sizeof(BlockHeader);
        
        while (current_offset != 0 && read_total < size) {
            // Read block header
            BlockHeader bh;
            size_t to_read = std::min(data_per_block, size - read_total);
            
            if (!readBlock(current_offset, bh, buffer + read_total, to_read)) {
                return false;
            }
            
            read_total += to_read;
            current_offset = bh.next_block_offset;
            
            // Safety check: verify next offset is valid or null
            if (current_offset != 0 && current_offset < data_region_start) {
                return false;
            }
        }
        
        return read_total == size;
    }
    
    // Free entire block chain
    void freeBlockChain(uint64_t first_block_offset) {
        if (first_block_offset < data_region_start) return;
        
        uint64_t current_offset = first_block_offset;
        
        while (current_offset != 0 && current_offset >= data_region_start) {
            BlockHeader bh;
            file.seekg(current_offset);
            file.read(reinterpret_cast<char*>(&bh), sizeof(BlockHeader));
            
            uint64_t next_offset = bh.next_block_offset;
            freeBlock(current_offset);
            current_offset = next_offset;
        }
    }
    
    int32_t findInodeByPath(const std::string& path) {
        if (path == "/") return root_inode;
        
        uint32_t inode_num;
        if (path_to_inode.find(path, inode_num)) {
            return inode_num;
        }
        return -1;
    }
    
    std::string getParentPath(const std::string& path) {
        size_t last_slash = path.find_last_of('/');
        if (last_slash == 0) return "/";
        return path.substr(0, last_slash);
    }
    
    std::string getFileName(const std::string& path) {
        size_t last_slash = path.find_last_of('/');
        return path.substr(last_slash + 1);
    }
    
    void saveHeader() {
        file.seekp(0);
        file.write(reinterpret_cast<char*>(&header), sizeof(OMNIHeader));
        file.flush();
    }
    
public:
    OFSInstance() : free_blocks(nullptr), next_inode_number(2), root_inode(1), data_region_start(0), isFormat{false} {}
    
    ~OFSInstance() {
        if (!file.is_open()) {
            delete free_blocks;
            return;
        }
        
        file.close();
        if(!isFormat){
        // Reopen to save state
        file.open(omni_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!file.is_open()) {
            delete free_blocks;
            return;
        }

        // Update header
        header.user_table_offset = sizeof(OMNIHeader);
        uint32_t user_table_size = header.max_users * sizeof(UserInfo);
        uint32_t file_table_offset = header.user_table_offset + user_table_size;
        
        // Write header
        file.seekp(0);
        file.write(reinterpret_cast<const char*>(&header), sizeof(OMNIHeader));

        // Write users
        std::vector<UserInfo> all_users = users.getAllValues();
        file.seekp(header.user_table_offset);

        for (size_t i = 0; i < all_users.size() && i < header.max_users; i++) {
            file.write(reinterpret_cast<const char*>(&all_users[i]), sizeof(UserInfo));
        }

        UserInfo empty_user{};
        for (size_t i = all_users.size(); i < header.max_users; i++) {
            file.write(reinterpret_cast<const char*>(&empty_user), sizeof(UserInfo));
        }

        // Write file entries
        file.seekp(file_table_offset);
        
        for (size_t i = 0; i < inodes.size() && i < config.max_files; i++) {
            const Inode& inode = inodes[i];
            
            FileEntry entry;
            std::memset(&entry, 0, sizeof(FileEntry));
            
            std::strncpy(entry.name, inode.name, sizeof(entry.name) - 1);
            entry.type = static_cast<uint8_t>(inode.type);
            entry.size = inode.size;
            entry.permissions = inode.permissions;
            entry.created_time = inode.created_time;
            entry.modified_time = inode.modified_time;
            std::strncpy(entry.owner, inode.owner, sizeof(entry.owner) - 1);
            
            // Store first block offset in inode field
            entry.inode = static_cast<uint32_t>(inode.first_block_offset);
            
            // Store parent inode and actual inode number in reserved space
            *reinterpret_cast<uint32_t*>(entry.reserved) = inode.parent_inode;
            *reinterpret_cast<uint32_t*>(entry.reserved + 4) = inode.inode_number;
            
            file.write(reinterpret_cast<const char*>(&entry), sizeof(FileEntry));
        }

        // Fill remaining slots
        FileEntry empty_entry{};
        empty_entry.type = 2;
        for (size_t i = inodes.size(); i < config.max_files; i++) {
            file.write(reinterpret_cast<const char*>(&empty_entry), sizeof(FileEntry));
        }

        file.close();
        }
        delete free_blocks;
    }
    
    int init(const std::string& omni_path_param, const std::string& config_path) {
        omni_path = omni_path_param;
        
        // ---- Parse configuration ----
        if (!ConfigParser::parse(config_path, config)) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_CONFIG);
        }
    
        // ---- Open OMNI file ----
        file.open(omni_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
        }
    
        // ---- Read header ----
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&header), sizeof(OMNIHeader));
    
        if (std::string(header.magic, 8) != "OMNIFS01") {
            file.close();
            return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
        }
    
        // ---- Calculate offsets ----
        uint32_t user_table_size = header.max_users * sizeof(UserInfo);
        uint32_t file_table_size = config.max_files * sizeof(FileEntry);
        data_region_start = sizeof(OMNIHeader) + user_table_size + file_table_size;
    
        // ---- Initialize bitmap ----
        uint32_t total_blocks = (header.total_size - data_region_start) / header.block_size;
        free_blocks = new Bitmap(total_blocks);
    
        // ---- Load users ----
        file.seekg(header.user_table_offset);
        for (uint32_t i = 0; i < header.max_users; i++) {
            UserInfo user;
            file.read(reinterpret_cast<char*>(&user), sizeof(UserInfo));
            if (user.is_active) {
                users.insert(std::string(user.username), user);
            }
        }
    
        // ---- Load file entries ----
        uint32_t file_table_offset = header.user_table_offset + user_table_size;
        file.seekg(file_table_offset);
    
        for (uint32_t i = 0; i < config.max_files; i++) {
            FileEntry entry;
            file.read(reinterpret_cast<char*>(&entry), sizeof(FileEntry));
        
            if (entry.type != 2) { // not empty
                Inode node;
            
                node.inode_number = *reinterpret_cast<const uint32_t*>(entry.reserved + 4);
                node.type = static_cast<EntryType>(entry.type);
                node.permissions = entry.permissions;
                node.created_time = entry.created_time;
                node.modified_time = entry.modified_time;
                node.size = entry.size;
                std::strncpy(node.owner, entry.owner, sizeof(node.owner) - 1);
                node.owner[sizeof(node.owner) - 1] = '\0';
                std::strncpy(node.name, entry.name, sizeof(node.name) - 1);
                node.name[sizeof(node.name) - 1] = '\0';
                node.parent_inode = *reinterpret_cast<const uint32_t*>(entry.reserved);
                node.first_block_offset = entry.inode;
            
                inodes.push_back(node);
                path_to_inode.insert(std::string(node.name), node.inode_number);
            
                if (node.inode_number >= next_inode_number) {
                    next_inode_number = node.inode_number + 1;
                }
            }
        }
    
        // ---- Close after reading metadata ----
        file.close();
    
        // ---- Rebuild parent-child relationships ----
        for (auto& parent : inodes) {
            if (parent.type != EntryType::DIRECTORY) continue;
        
            std::string parent_name = parent.name;
            if (parent_name != "/" && parent_name.back() != '/') {
                parent_name += '/';
            }
        
            for (auto& child : inodes) {
                if (&child == &parent) continue;
            
                std::string child_name = child.name;
            
                // Only direct children (no deeper subdirs)
                if (child_name.rfind(parent_name, 0) == 0) {
                    std::string relative = child_name.substr(parent_name.size());
                    if (relative.find('/') == std::string::npos) {
                        parent.child_inodes.push_back(child.inode_number);
                    }
                }
            }
        }
    
        // ---- Reopen file to mark used blocks ----
        file.open(omni_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
        }
    
        for (auto& node : inodes) {
            if (node.first_block_offset == 0) continue;
            if (node.type != EntryType::FILE) continue;
        
            uint64_t current_offset = node.first_block_offset;
            while (current_offset != 0) {
                uint32_t block_num = (current_offset - data_region_start) / header.block_size;
                free_blocks->setBlockUsed(block_num);
            
                BlockHeader bh;
                file.seekg(current_offset);
                file.read(reinterpret_cast<char*>(&bh), sizeof(BlockHeader));
                current_offset = bh.next_block_offset;
            }
        }
    
        file.close();
    
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }

    
    int format(const std::string& omni_path_param, const std::string& config_path) {
        isFormat = true;
        if (!ConfigParser::parse(config_path, config)) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_CONFIG);
        }
        
        file.open(omni_path_param, std::ios::binary | std::ios::trunc | std::ios::out);
        if (!file.is_open()) {
            return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
        }
        
        // Initialize header
        std::memset(&header, 0, sizeof(OMNIHeader));
        std::strncpy(header.magic, "OMNIFS01", 8);
        header.format_version = 0x00010000;
        header.total_size = config.total_size;
        header.header_size = config.header_size;
        header.block_size = config.block_size;
        header.max_users = config.max_users;
        header.user_table_offset = sizeof(OMNIHeader);
        
        file.write(reinterpret_cast<char*>(&header), sizeof(OMNIHeader));
        
        // Create admin user
        UserInfo admin;
        std::memset(&admin, 0, sizeof(UserInfo));
        std::strncpy(admin.username, config.admin_username.c_str(), sizeof(admin.username) - 1);
        std::strncpy(admin.password_hash, config.admin_password.c_str(), sizeof(admin.password_hash) - 1);
        admin.role = UserRole::ADMIN;
        admin.created_time = std::time(nullptr);
        admin.is_active = 1;
        
        users.insert(std::string(admin.username), admin);
        file.write(reinterpret_cast<char*>(&admin), sizeof(UserInfo));
        
        // Empty user slots
        UserInfo empty_user{};
        for (uint32_t i = 1; i < config.max_users; i++) {
            file.write(reinterpret_cast<char*>(&empty_user), sizeof(UserInfo));
        }
        
        // Calculate data region start
        uint32_t user_table_size = header.max_users * sizeof(UserInfo);
        uint32_t file_table_size = config.max_files * sizeof(FileEntry);
        data_region_start = sizeof(OMNIHeader) + user_table_size + file_table_size;
        
        uint32_t total_blocks = (header.total_size - data_region_start) / header.block_size;
        free_blocks = new Bitmap(total_blocks);
        
        // Create root directory
        Inode root;
        root.inode_number = root_inode;
        root.type = EntryType::DIRECTORY;
        root.permissions = 0755;
        root.created_time = std::time(nullptr);
        root.modified_time = root.created_time;
        root.parent_inode = 0;
        root.first_block_offset = 0;  // Directories don't have data blocks
        std::strncpy(root.owner, config.admin_username.c_str(), sizeof(root.owner) - 1);
        std::strncpy(root.name, "/", sizeof(root.name) - 1);
        
        inodes.push_back(root);
        path_to_inode.insert("/", root_inode);
        
        // Write root as FileEntry
        FileEntry root_entry;
        std::memset(&root_entry, 0, sizeof(FileEntry));
        std::strncpy(root_entry.name, "/", sizeof(root_entry.name) - 1);
        root_entry.type = static_cast<uint8_t>(EntryType::DIRECTORY);
        root_entry.permissions = 0755;
        root_entry.created_time = root.created_time;
        root_entry.modified_time = root.modified_time;
        std::strncpy(root_entry.owner, config.admin_username.c_str(), sizeof(root_entry.owner) - 1);
        root_entry.inode = 0;  // No data blocks for root
        *reinterpret_cast<uint32_t*>(root_entry.reserved) = 0;  // parent
        *reinterpret_cast<uint32_t*>(root_entry.reserved + 4) = root.inode_number;
        
        file.write(reinterpret_cast<char*>(&root_entry), sizeof(FileEntry));
        
        // Empty file entries
        FileEntry empty_entry{};
        empty_entry.type = 2;
        for (uint32_t i = 1; i < config.max_files; i++) {
            file.write(reinterpret_cast<char*>(&empty_entry), sizeof(FileEntry));
        }
        
        // Pad to total size
        uint64_t current_pos = file.tellp();
        if (current_pos < config.total_size) {
            std::vector<char> padding(config.total_size - current_pos, 0);
            file.write(padding.data(), padding.size());
        }
        
        file.close();
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int userLogin(const std::string& username, const std::string& password, void** session) {
        UserInfo user;
        if (!users.find(username, user)) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        if (password != std::string(user.password_hash)) {
            return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
        }
        
        user.last_login = std::time(nullptr);
        users.insert(username, user);
        
        UserInfo* session_user = new UserInfo(user);
        *session = session_user;
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int createFile(void* session, const std::string& path, const char* data, size_t size) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        if (findInodeByPath(path) >= 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
        }
        
        std::string parent_path = getParentPath(path);
        int32_t parent_inode_num = findInodeByPath(parent_path);
        if (parent_inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        // Write data to blocks
        uint64_t first_block_offset = writeDataToBlocks(data, size);
        if (size > 0 && first_block_offset == 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
        }
        
        // Create inode
        UserInfo* user = static_cast<UserInfo*>(session);
        Inode new_inode;
        std::strncpy(new_inode.name, path.c_str(), sizeof(new_inode.name) - 1);
        new_inode.inode_number = allocateInode();
        new_inode.type = EntryType::FILE;
        new_inode.size = size;
        new_inode.permissions = 0644;
        new_inode.created_time = std::time(nullptr);
        new_inode.modified_time = new_inode.created_time;
        std::strncpy(new_inode.owner, user->username, sizeof(new_inode.owner) - 1);
        new_inode.parent_inode = parent_inode_num;
        new_inode.first_block_offset = first_block_offset;
        
        inodes.push_back(new_inode);
        path_to_inode.insert(path, new_inode.inode_number);
        
        // Add to parent
        for (auto& inode : inodes) {
            if (inode.inode_number == parent_inode_num) {
                inode.child_inodes.push_back(new_inode.inode_number);
                break;
            }
        }
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int readFile(void* session, const std::string& path, char** buffer, size_t* size) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        int32_t inode_num = findInodeByPath(path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        Inode* inode = nullptr;
        for (auto& node : inodes) {
            if (node.inode_number == inode_num) {
                inode = &node;
                break;
            }
        }
        
        if (!inode || inode->type != EntryType::FILE) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
        }
        
        *size = inode->size;
        *buffer = new char[*size + 1];
        
        if (*size > 0 && !readDataFromBlocks(inode->first_block_offset, *buffer, *size)) {
            delete[] *buffer;
            return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
        }
        
        (*buffer)[*size] = '\0';
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int deleteFile(void* session, const std::string& path) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        int32_t inode_num = findInodeByPath(path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        Inode* inode = nullptr;
        for (auto& node : inodes) {
            if (node.inode_number == inode_num) {
                inode = &node;
                break;
            }
        }
        
        if (!inode || inode->type != EntryType::FILE) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
        }
        
        // Free block chain
        freeBlockChain(inode->first_block_offset);
        
        // Remove from parent
        for (auto& parent : inodes) {
            if (parent.inode_number == inode->parent_inode) {
                auto it = std::find(parent.child_inodes.begin(), parent.child_inodes.end(), inode_num);
                if (it != parent.child_inodes.end()) {
                    parent.child_inodes.erase(it);
                }
                break;
            }
        }
        
        path_to_inode.remove(path);
        inode->type = static_cast<EntryType>(2);  // Mark deleted
        inode->first_block_offset = 0;
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int createDirectory(void* session, const std::string& path) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        if (findInodeByPath(path) >= 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
        }
        
        std::string parent_path = getParentPath(path);
        int32_t parent_inode_num = findInodeByPath(parent_path);
        if (parent_inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        UserInfo* user = static_cast<UserInfo*>(session);
        Inode new_dir;
        std::strncpy(new_dir.name, path.c_str(), sizeof(new_dir.name) - 1);
        new_dir.inode_number = allocateInode();
        new_dir.type = EntryType::DIRECTORY;
        new_dir.size = 0;
        new_dir.permissions = 0755;
        new_dir.created_time = std::time(nullptr);
        new_dir.modified_time = new_dir.created_time;
        std::strncpy(new_dir.owner, user->username, sizeof(new_dir.owner) - 1);
        new_dir.parent_inode = parent_inode_num;
        new_dir.first_block_offset = 0;  // Directories don't use data blocks
        
        inodes.push_back(new_dir);
        path_to_inode.insert(path, new_dir.inode_number);
        
        // Add to parent
        for (auto& inode : inodes) {
            if (inode.inode_number == parent_inode_num) {
                inode.child_inodes.push_back(new_dir.inode_number);
                break;
            }
        }
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int listDirectory(void* session, const std::string& path, FileEntry** entries, int* count) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        int32_t inode_num = findInodeByPath(path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        Inode* dir_inode = nullptr;
        for (auto& node : inodes) {
            if (node.inode_number == inode_num) {
                dir_inode = &node;
                break;
            }
        }
        
        if (!dir_inode || dir_inode->type != EntryType::DIRECTORY) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
        }
        
        *count = dir_inode->child_inodes.size();
        *entries = new FileEntry[*count];
        
        for (size_t i = 0; i < dir_inode->child_inodes.size(); i++) {
            Inode* child = nullptr;
            for (auto& node : inodes) {
                if (node.inode_number == dir_inode->child_inodes[i]) {
                    child = &node;
                    break;
                }
            }
            
            if (!child) continue;
            
            FileEntry& entry = (*entries)[i];
            std::memset(&entry, 0, sizeof(FileEntry));
            
            std::strncpy(entry.name, child->name, sizeof(entry.name) - 1);
            entry.type = static_cast<uint8_t>(child->type);
            entry.size = child->size;
            entry.permissions = child->permissions;
            entry.created_time = child->created_time;
            entry.modified_time = child->modified_time;
            std::strncpy(entry.owner, child->owner, sizeof(entry.owner) - 1);
            entry.inode = child->inode_number;
        }
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int deleteDirectory(void* session, const std::string& path) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        if (path == "/") {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
        }
        
        int32_t inode_num = findInodeByPath(path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        Inode* inode = nullptr;
        for (auto& node : inodes) {
            if (node.inode_number == inode_num) {
                inode = &node;
                break;
            }
        }
        
        if (!inode || inode->type != EntryType::DIRECTORY) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
        }
        
        if (!inode->child_inodes.empty()) {
            return static_cast<int>(OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY);
        }
        
        // Remove from parent
        for (auto& parent : inodes) {
            if (parent.inode_number == inode->parent_inode) {
                auto it = std::find(parent.child_inodes.begin(), parent.child_inodes.end(), inode_num);
                if (it != parent.child_inodes.end()) {
                    parent.child_inodes.erase(it);
                }
                break;
            }
        }
        
        path_to_inode.remove(path);
        inode->type = static_cast<EntryType>(2);  // Mark deleted
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int fileExists(void* session, const std::string& path) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        int32_t inode_num = findInodeByPath(path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        for (const auto& inode : inodes) {
            if (inode.inode_number == inode_num && inode.type == EntryType::FILE) {
                return static_cast<int>(OFSErrorCodes::SUCCESS);
            }
        }
        
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    int directoryExists(void* session, const std::string& path) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        int32_t inode_num = findInodeByPath(path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        for (const auto& inode : inodes) {
            if (inode.inode_number == inode_num && inode.type == EntryType::DIRECTORY) {
                return static_cast<int>(OFSErrorCodes::SUCCESS);
            }
        }
        
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    int getMetadata(void* session, const std::string& path, FileMetadata* meta) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        int32_t inode_num = findInodeByPath(path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        Inode* inode = nullptr;
        for (auto& node : inodes) {
            if (node.inode_number == inode_num) {
                inode = &node;
                break;
            }
        }
        
        if (!inode) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        std::strncpy(meta->path, path.c_str(), sizeof(meta->path) - 1);
        std::strncpy(meta->entry.name, getFileName(path).c_str(), sizeof(meta->entry.name) - 1);
        meta->entry.type = static_cast<uint8_t>(inode->type);
        meta->entry.size = inode->size;
        meta->entry.permissions = inode->permissions;
        meta->entry.created_time = inode->created_time;
        meta->entry.modified_time = inode->modified_time;
        std::strncpy(meta->entry.owner, inode->owner, sizeof(meta->entry.owner) - 1);
        meta->entry.inode = inode->inode_number;
        
        // Calculate blocks used
        uint32_t blocks_used = 0;
        if (inode->first_block_offset != 0) {
            uint64_t current_offset = inode->first_block_offset;
            while (current_offset != 0) {
                blocks_used++;
                BlockHeader bh;
                file.seekg(current_offset);
                file.read(reinterpret_cast<char*>(&bh), sizeof(BlockHeader));
                current_offset = bh.next_block_offset;
            }
        }
        
        meta->blocks_used = blocks_used;
        meta->actual_size = blocks_used * header.block_size;
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int getStats(void* session, FSStats* stats) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        stats->total_size = header.total_size - data_region_start;
        
        uint32_t free_blocks_count = free_blocks->countFreeBlocks();
        stats->free_space = free_blocks_count * header.block_size;
        stats->used_space = stats->total_size - stats->free_space;
        
        stats->total_files = 0;
        stats->total_directories = 0;
        
        for (const Inode& inode : inodes) {
            if (inode.type != static_cast<EntryType>(2)) {
                if (inode.type == EntryType::FILE) {
                    stats->total_files++;
                } else if (inode.type == EntryType::DIRECTORY) {
                    stats->total_directories++;
                }
            }
        }
        
        stats->total_users = users.getAllValues().size();
        stats->active_sessions = 1;
        stats->fragmentation = 0.0;
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int renameFile(void* session, const std::string& old_path, const std::string& new_path) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        int32_t inode_num = findInodeByPath(old_path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        if (findInodeByPath(new_path) >= 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
        }
        
        Inode* inode = nullptr;
        for (auto& node : inodes) {
            if (node.inode_number == inode_num) {
                inode = &node;
                break;
            }
        }
        
        if (!inode) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        // Update name
        std::strncpy(inode->name, new_path.c_str(), sizeof(inode->name) - 1);
        inode->modified_time = std::time(nullptr);
        
        // Update path mapping
        path_to_inode.remove(old_path);
        path_to_inode.insert(new_path, inode_num);
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int editFile(void* session, const std::string& path, const char* data, size_t size, uint32_t index) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        int32_t inode_num = findInodeByPath(path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        Inode* inode = nullptr;
        for (auto& node : inodes) {
            if (node.inode_number == inode_num) {
                inode = &node;
                break;
            }
        }
        
        if (!inode || inode->type != EntryType::FILE) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
        }
        
        if (index > inode->size) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
        }
        
        // Read existing content
        char* buffer = nullptr;
        size_t file_size = 0;
        
        if (inode->size > 0) {
            buffer = new char[inode->size];
            if (!readDataFromBlocks(inode->first_block_offset, buffer, inode->size)) {
                delete[] buffer;
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }
            file_size = inode->size;
        }
        
        // Create new content
        size_t new_size = std::max(file_size, static_cast<size_t>(index + size));
        char* new_buffer = new char[new_size];
        
        if (buffer) {
            std::memcpy(new_buffer, buffer, file_size);
            delete[] buffer;
        }
        
        std::memcpy(new_buffer + index, data, size);
        
        // Free old blocks
        if (inode->first_block_offset != 0) {
            freeBlockChain(inode->first_block_offset);
        }
        
        // Write new content
        uint64_t new_offset = writeDataToBlocks(new_buffer, new_size);
        delete[] new_buffer;
        
        if (new_size > 0 && new_offset == 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
        }
        
        inode->first_block_offset = new_offset;
        inode->size = new_size;
        inode->modified_time = std::time(nullptr);
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int truncateFile(void* session, const std::string& path) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        int32_t inode_num = findInodeByPath(path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        Inode* inode = nullptr;
        for (auto& node : inodes) {
            if (node.inode_number == inode_num) {
                inode = &node;
                break;
            }
        }
        
        if (!inode || inode->type != EntryType::FILE) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
        }
        
        // Free old blocks
        if (inode->first_block_offset != 0) {
            freeBlockChain(inode->first_block_offset);
        }
        
        // Write magic string
        const char* magic = "siruamr";
        size_t magic_len = std::strlen(magic);
        
        uint64_t new_offset = writeDataToBlocks(magic, magic_len);
        if (new_offset == 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
        }
        
        inode->first_block_offset = new_offset;
        inode->size = magic_len;
        inode->modified_time = std::time(nullptr);
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int setPermissions(void* session, const std::string& path, uint32_t permissions) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        int32_t inode_num = findInodeByPath(path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        for (auto& inode : inodes) {
            if (inode.inode_number == inode_num) {
                inode.permissions = permissions;
                inode.modified_time = std::time(nullptr);
                return static_cast<int>(OFSErrorCodes::SUCCESS);
            }
        }
        
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    int createUser(void* session, const std::string& username, const std::string& password, UserRole role) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        UserInfo* admin = static_cast<UserInfo*>(session);
        if (admin->role != UserRole::ADMIN) {
            return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
        }
        
        UserInfo existing;
        if (users.find(username, existing)) {
            return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
        }
        
        UserInfo new_user;
        std::memset(&new_user, 0, sizeof(UserInfo));
        std::strncpy(new_user.username, username.c_str(), sizeof(new_user.username) - 1);
        std::strncpy(new_user.password_hash, password.c_str(), sizeof(new_user.password_hash) - 1);
        new_user.role = role;
        new_user.created_time = std::time(nullptr);
        new_user.is_active = 1;
        
        users.insert(username, new_user);
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int deleteUser(void* session, const std::string& username) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        UserInfo* admin = static_cast<UserInfo*>(session);
        if (admin->role != UserRole::ADMIN) {
            return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
        }
        
        if (!users.remove(username)) {
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int listUsers(void* session, UserInfo** user_list, int* count) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        UserInfo* admin = static_cast<UserInfo*>(session);
        if (admin->role != UserRole::ADMIN) {
            return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
        }
        
        std::vector<UserInfo> all_users = users.getAllValues();
        *count = all_users.size();
        *user_list = new UserInfo[*count];
        
        for (size_t i = 0; i < all_users.size(); i++) {
            (*user_list)[i] = all_users[i];
        }
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int getSessionInfo(void* session, SessionInfo* info) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        UserInfo* user = static_cast<UserInfo*>(session);
        info->user = *user;
        info->login_time = user->last_login;
        info->last_activity = std::time(nullptr);
        std::strncpy(info->session_id, "session_001", sizeof(info->session_id) - 1);
        
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
};

// C API Functions
int fs_init(void** instance, const char* omni_path, const char* config_path) {
    OFSInstance* ofs = new OFSInstance();
    int result = ofs->init(omni_path, config_path);
    if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        *instance = ofs;
    } else {
        delete ofs;
    }
    return result;
}

int fs_format(const char* omni_path, const char* config_path) {
    OFSInstance ofs;
    return ofs.format(omni_path, config_path);
}

void fs_shutdown(void* instance) {
    if (instance) {
        delete static_cast<OFSInstance*>(instance);
    }
}

int user_login(void* instance, const char* username, const char* password, void** session) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->userLogin(username, password, session);
}

int user_logout(void* session) {
    if (session) {
        delete static_cast<UserInfo*>(session);
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
}

int user_create(void* instance, void* admin_session, const char* username, const char* password, UserRole role) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->createUser(admin_session, username, password, role);
}

int user_delete(void* instance, void* admin_session, const char* username) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->deleteUser(admin_session, username);
}

int user_list(void* instance, void* admin_session, UserInfo** users, int* count) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->listUsers(admin_session, users, count);
}

int file_create(void* instance, void* session, const char* path, const char* data, size_t size) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->createFile(session, path, data, size);
}

int file_read(void* instance, void* session, const char* path, char** buffer, size_t* size) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->readFile(session, path, buffer, size);
}

int file_delete(void* instance, void* session, const char* path) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->deleteFile(session, path);
}

int file_edit(void* instance, void* session, const char* path, const char* data, size_t size, uint32_t index) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->editFile(session, path, data, size, index);
}

int file_truncate(void* instance, void* session, const char* path) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->truncateFile(session, path);
}

int file_exists(void* instance, void* session, const char* path) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->fileExists(session, path);
}

int file_rename(void* instance, void* session, const char* old_path, const char* new_path) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->renameFile(session, old_path, new_path);
}

int dir_create(void* instance, void* session, const char* path) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->createDirectory(session, path);
}

int dir_list(void* instance, void* session, const char* path, FileEntry** entries, int* count) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->listDirectory(session, path, entries, count);
}

int dir_delete(void* instance, void* session, const char* path) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->deleteDirectory(session, path);
}

int dir_exists(void* instance, void* session, const char* path) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->directoryExists(session, path);
}

int get_metadata(void* instance, void* session, const char* path, FileMetadata* meta) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->getMetadata(session, path, meta);
}

int set_permissions(void* instance, void* session, const char* path, uint32_t permissions) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->setPermissions(session, path, permissions);
}

int get_stats(void* instance, void* session, FSStats* stats) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->getStats(session, stats);
}

int get_session_info(void* instance, void* session, SessionInfo* info) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->getSessionInfo(session, info);
}

void free_buffer(void* buffer) {
    if (buffer) {
        delete[] static_cast<char*>(buffer);
    }
}

const char* get_error_message(int error_code) {
    switch (static_cast<OFSErrorCodes>(error_code)) {
        case OFSErrorCodes::SUCCESS: return "Operation successful";
        case OFSErrorCodes::ERROR_NOT_FOUND: return "File/directory/user not found";
        case OFSErrorCodes::ERROR_PERMISSION_DENIED: return "Permission denied";
        case OFSErrorCodes::ERROR_IO_ERROR: return "I/O error";
        case OFSErrorCodes::ERROR_INVALID_PATH: return "Invalid path";
        case OFSErrorCodes::ERROR_FILE_EXISTS: return "File/directory already exists";
        case OFSErrorCodes::ERROR_NO_SPACE: return "No space left";
        case OFSErrorCodes::ERROR_INVALID_CONFIG: return "Invalid configuration";
        case OFSErrorCodes::ERROR_NOT_IMPLEMENTED: return "Not implemented";
        case OFSErrorCodes::ERROR_INVALID_SESSION: return "Invalid session";
        case OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY: return "Directory not empty";
        case OFSErrorCodes::ERROR_INVALID_OPERATION: return "Invalid operation";
        default: return "Unknown error";
    }
}

#endif // OFS_IMPLEMENTATION_HPP