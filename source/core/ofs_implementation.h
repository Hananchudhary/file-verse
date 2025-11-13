#ifndef OFS_IMPLEMENTATION_H
#define OFS_IMPLEMENTATION_H

#include "/home/hanan/Projects/c++/file-verse/source/include/odf_types.hpp"
#include "/home/hanan/Projects/c++/file-verse/source/data_structures/AVL.h"
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <sstream>

using namespace std;
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
struct Tree{
    uint32_t prnt;
    vector<uint32_t> childs;
};
struct BlockHdr{
    uint32_t nxt;
    bool isValid;
    uint32_t size;
    BlockHdr(): isValid(true), nxt(0), size(4096){} 
};
class OFSInstance {
private:
    std::string omni_path;
    Config config;
    OMNIHeader header;
    std::fstream file;
    bool isFormat;
    AVLTree<UserInfo> users;
    vector<FileEntry> files;
    vector<Tree> Root;
    AVLTree<FileMetadata> Mds;
    vector<uint32_t> free_segments;
    uint32_t next_inode_number;
    uint32_t root_idx;
    uint64_t data_region_start;  // Where data blocks begin
    
    uint32_t allocateInode() {
        return next_inode_number++;
    }
    
    
    std::vector<std::string> splitPathTokens(const std::string &path) {
        std::vector<std::string> toks;
        std::string token;
        std::istringstream ss(path);
        while (std::getline(ss, token, '/')) {
            if (!token.empty()) toks.push_back(token);
        }
        return toks;
    }
    void traverse(uint32_t node_idx, string crnt_path){
        if (node_idx >= files.size()) return;

        const FileEntry& entry = files[node_idx];
        // Construct path
        std::string full_path = crnt_path; 
        // Create metadata and insert into AVL tree
        FileMetadata meta(full_path, entry);
        if (entry.getType() == EntryType::FILE) {
            uint64_t blocks = 0;
            uint64_t actual_bytes = 0;
            uint32_t current_block = entry.inode;

            while (current_block != 0) { // replace 0 with 0xFFFFFFFF if needed
                uint64_t block_offset = data_region_start +
                                        static_cast<uint64_t>(current_block) * (sizeof(BlockHdr) + header.block_size);
                BlockHdr bh;
                file.seekg(static_cast<std::streamoff>(block_offset), std::ios::beg);
                file.read(reinterpret_cast<char*>(&bh), sizeof(bh));
                if (!file) break; // stop on error

                blocks++;
                actual_bytes += bh.size;

                current_block = bh.nxt; // next block index
            }

            meta.blocks_used = blocks;
            meta.actual_size = actual_bytes;
            Mds.insert(full_path, meta);
        }
        // Recursively process children
        if (node_idx < Root.size()) {
            for (auto child_idx : Root[node_idx].childs) {
                traverse(child_idx, full_path);
            }
        }
    }
    void initializeMetadataTree() {

        traverse(root_idx, "/");
    }
    // Helper: find file index by absolute path. Returns UINT32_MAX if not found.
    // Uses files[] and Root[] structure (where Root[i].childs are indices into files).
    uint32_t findFileIndexByPath(const std::string &path) {
        if (path == "/" || path.empty()) return root_idx;

        std::vector<std::string> toks = splitPathTokens(path);
        uint32_t cur = root_idx;
        for (size_t t = 0; t < toks.size(); ++t) {
            bool found = false;
            const auto &children = Root[cur].childs;
            for (uint32_t child_idx : children) {
                // compare file/directory name with token
                if (std::string(files[child_idx].name) == toks[t]) {
                    cur = child_idx;
                    found = true;
                    break;
                }
            }
            if (!found) return UINT32_MAX;
        }
        return cur;
    }
    void saveHeader() {
        file.seekp(0);
        file.write(reinterpret_cast<char*>(&header), sizeof(OMNIHeader));
        file.flush();
    }
    Tree* find_entry(uint32_t idx){
        int size = this->Root.size();
        for(int i=0;i<size;i++){
            if(Root[i].prnt == idx) return &Root[i];
        } 
        return nullptr;
    }
    
    
public:
    OFSInstance() : next_inode_number(1), root_idx(0), data_region_start{0}, isFormat{false} {}
    
    ~OFSInstance() {

        if (!isFormat) {
            // Reopen the file for writing the updated state
            file.open(omni_path, std::ios::binary | std::ios::in | std::ios::out);
            if (!file.is_open()) {
                return;
            }
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (!file) {
                file.close();
                return;
            }

            // Collect all users from AVL tree
            std::vector<UserInfo> all_users = users.getAllValues();
            uint32_t written_users = 0;
            for (const auto& u : all_users) {
                streampos p = file.tellp();
                file.write(reinterpret_cast<const char*>(&u), sizeof(UserInfo));
                if (!file) {
                    file.close();
                    return;
                }
                ++written_users;
                if (written_users >= header.max_users)
                    break; // avoid overflow if AVL > max_users (shouldn’t happen)
            }

            // Fill remaining user slots with empty data
            UserInfo emptyUser;
            std::memset(&emptyUser, 0, sizeof(emptyUser));
            for (; written_users < header.max_users; ++written_users) {
                file.write(reinterpret_cast<const char*>(&emptyUser), sizeof(UserInfo));
                if (!file) {
                    file.close();
                    return;
                }
            }
            uint32_t written_files = 0;
            for (const auto& entry : files) {
                file.write(reinterpret_cast<const char*>(&entry), sizeof(FileEntry));
                if (!file) {
                    file.close();
                    return;
                }
                ++written_files;
            }
            FileEntry emptyEntry;
            std::memset(&emptyEntry, 0, sizeof(emptyEntry));
            emptyEntry.type = 2;
            for (; written_files < config.max_files; ++written_files) {
                file.write(reinterpret_cast<const char*>(&emptyEntry), sizeof(FileEntry));
                if (!file) {
                    file.close();
                    return;
                }
            }

        }
        file.close();
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
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file) {
            file.close();
            return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
        }
        for (uint32_t i = 0; i < header.max_users; ++i) {
            UserInfo u;
            file.read(reinterpret_cast<char*>(&u), sizeof(UserInfo));
            if (!file) {
                file.close();
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }
            if (u.is_active) {
                users.insert(u.username, u); // adjust insert() if your AVLTree differs
            }
            else   
                break;
        }
        file.seekg(sizeof(OMNIHeader) + (config.max_users*sizeof(UserInfo)));
        for (uint32_t i = 0; i < config.max_files; ++i) {
            streampos p = file.tellg();

            FileEntry entry;
            file.read(reinterpret_cast<char*>(&entry), sizeof(FileEntry));
            if (!file) {
                file.close();
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }
            if(entry.type != 2){
                Tree temp;
                if(string(entry.name) == string("/")) temp.prnt = root_idx;
                else temp.prnt = allocateInode();
                Root.push_back(temp);
                if (entry.parent_idx != 0xFFFFFFFF && entry.parent_idx < config.max_files) {
                    Tree* prnt = find_entry(entry.parent_idx);
                    Root[prnt->prnt].childs.push_back(temp.prnt);
                }
                files.push_back(entry);
                
            }
            else
                break;
        }
        file.seekg(sizeof(OMNIHeader) + (config.max_users*sizeof(UserInfo)) + (config.max_files*sizeof(FileEntry)));
        data_region_start = static_cast<uint64_t>(file.tellg());
        uint64_t total_size = header.total_size;
        uint64_t block_size = header.block_size;
        uint64_t bytes_remaining = total_size - data_region_start;
        uint32_t block_index = 0;
        while (bytes_remaining > 0) {
            BlockHdr bh;
            std::streampos pos_before = file.tellg();
            file.read(reinterpret_cast<char*>(&bh), sizeof(BlockHdr));
            if (!file) break;
            // If block header marked valid => free block
            if (bh.isValid) {
                free_segments.push_back(pos_before);
            }
            // Skip block data
            file.seekg(static_cast<uint32_t>(pos_before) + config.block_size + sizeof(BlockHdr));
            // Advance counters
            bytes_remaining -= static_cast<uint64_t>(sizeof(BlockHdr) + bh.size);
            block_index++;
        }
        file.close();
        initializeMetadataTree();
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }


    
    int format(const std::string& omni_path_param, const std::string& config_path) {
        isFormat = true;
        omni_path = omni_path_param;
        if (!ConfigParser::parse(config_path, config)) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_CONFIG);
        }
        
        file.open(omni_path_param, std::ios::binary | std::ios::trunc | std::ios::out);
        if (!file.is_open()) {
            return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
        }
        
        std::memset(&header, 0, sizeof(header));
        // magic
        const char magic_expected[] = "OMNIFS01";
        std::strncpy(header.magic, magic_expected, sizeof(header.magic));
        header.format_version = 0x00010000;
        header.total_size = config.total_size;
        header.header_size = sizeof(OMNIHeader); // 512 as required
        header.block_size = config.block_size;
        std::strncpy(header.student_id, "BSCS24001", 9);
        std::strncpy(header.submission_date, "14-11-25", sizeof(header.submission_date)-1);
        std::strncpy(header.config_hash, "SHA-256", sizeof(header.config_hash)-1);
        header.config_timestamp = static_cast<uint64_t>(std::time(nullptr));

        // Place the user table directly after header for this implementation
        uint32_t user_table_offset = sizeof(OMNIHeader); // offset in bytes
        header.user_table_offset = user_table_offset;
        header.max_users = config.max_users;

        file.write(reinterpret_cast<const char*>(&header), sizeof(OMNIHeader));
        if (!file) { file.close(); return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR); }

        UserInfo admin(config.admin_username, config.admin_password, UserRole::ADMIN, static_cast<uint64_t>(std::time(nullptr)));
        file.write(reinterpret_cast<const char*>(&admin), sizeof(UserInfo));
        if (!file) { file.close(); return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR); }

        // Remaining users (empty)
        UserInfo emptyUser;
        std::memset(&emptyUser, 0, sizeof(emptyUser));
        for (uint32_t i = 1; i < config.max_users; ++i) {
            file.write(reinterpret_cast<const char*>(&emptyUser), sizeof(emptyUser));
            if (!file) { file.close(); return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR); }
        }
        FileEntry rootEntry("/", EntryType::DIRECTORY, 0, 0755, config.admin_username, root_idx);
        rootEntry.created_time = static_cast<uint64_t>(std::time(nullptr));
        rootEntry.modified_time = rootEntry.created_time;
        rootEntry.parent_idx = 0xFFFFFFFF; // root has no parent (use sentinel)
        file.write(reinterpret_cast<const char*>(&rootEntry), sizeof(FileEntry));
        if (!file) { file.close(); return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR); }
        
        // Write remaining FileEntry slots as empty
        FileEntry emptyFileEntry;
        std::memset(&emptyFileEntry, 0, sizeof(emptyFileEntry));
        emptyFileEntry.type = 2;
        for (uint32_t i = 1; i < config.max_files; ++i) {
            file.write(reinterpret_cast<const char*>(&emptyFileEntry), sizeof(emptyFileEntry));
            if (!file) { file.close(); return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR); }
        }

        // Compute how many bytes left in file target (total_size - already written)
        uint64_t written_so_far = static_cast<uint64_t>(file.tellp());
        if (written_so_far >= config.total_size) {
            // Nothing left for data region
            file.close();
            return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
        }
        uint64_t bytes_left = config.total_size - written_so_far;
        uint64_t blk_size = config.block_size;
        uint32_t num_blocks = static_cast<uint32_t>( (bytes_left + blk_size - 1) / blk_size );

        // For each block: write BlockHdr then write block bytes (zero)
        for (uint32_t b = 0; b < num_blocks; ++b) {
            BlockHdr hdr;
            hdr.isValid = true;
            hdr.nxt = 0; // chain not used here
            hdr.size = 0;

            // If last block and partial, adjust size to remaining bytes
            uint64_t block_start_pos = static_cast<uint64_t>(file.tellp());
            uint64_t remaining_total = config.total_size - block_start_pos;
            uint32_t write_block_size = static_cast<uint32_t>( std::min<uint64_t>(blk_size, remaining_total) );

            // Write header
            file.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
            file.seekp((file.tellp() + static_cast<streampos>(config.block_size)));
            if (!file) { file.close(); return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR); }
            
        }


        // Close file
        file.close();

        // NOTE: free_segments lives only in memory here. Persist it as needed by your FS design.
        // Example: you could write it into header.file_state_storage_offset area or elsewhere.

        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int userLogin(const std::string& username, const std::string& password, void** session) {
        UserInfo user;
        if(users.size() +1 > config.max_users){
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
        }
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

        if (path.empty() || path[0] != '/') {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
        }

        // Cast session
        SessionInfo* sess = static_cast<SessionInfo*>(session);
        if(Mds.exists(path)){
            
            return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
        }

        std::string parent_path, filename;
        if (path == "/") {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION); // cannot create root
        }

        size_t last_slash = path.find_last_of('/');

        parent_path = (last_slash == 0) ? std::string("/") : path.substr(0, last_slash);
        filename = path.substr(last_slash + 1);
        if (filename.empty() || path.size() > 512 || (path.size() - last_slash) > config.max_filename_length) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);

        uint32_t parent_idx = findFileIndexByPath(parent_path);
        if (parent_idx == UINT32_MAX) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
        }
        // ensure parent is a directory
        if (parent_idx < 0 || files[parent_idx].type != static_cast<uint8_t>(EntryType::DIRECTORY)) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
        }

        // --- 3) Prepare FileEntry ---
        FileEntry newEntry(filename, EntryType::FILE, size, 0644, string(sess->user.username), 0);
        uint64_t now = static_cast<uint64_t>(std::time(nullptr));
        newEntry.created_time = now;
        newEntry.modified_time = now;
        newEntry.parent_idx = parent_idx;

        // --- 4) Check and allocate free segments ---
        uint64_t blk_payload = header.block_size;
        if (blk_payload == 0) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_CONFIG);
        uint32_t needed_blocks = static_cast<uint32_t>((size + blk_payload - 1) / blk_payload);

        if (needed_blocks == 0) needed_blocks = 1; // allocate at least one block for zero-length file?

        if (free_segments.size() < needed_blocks) {
            return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
        }

        // Choose blocks: pick the first N free segments (simple strategy).
        std::vector<uint32_t> allocated_blocks;
        allocated_blocks.reserve(needed_blocks);
        for (uint32_t i = 0; i < needed_blocks; ++i) {
            allocated_blocks.push_back(free_segments[i]);
            free_segments.erase(free_segments.begin() + i);
        }

        newEntry.inode = allocated_blocks[0];

        // --- 5) Write data into blocks and update BlockHdr chain ---
        // Ensure file stream is open
        if (!file.is_open()) {
            file.open(omni_path, std::ios::in | std::ios::out | std::ios::binary);
            if (!file.is_open()) {
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }
        }

        size_t remaining = size;
        size_t offset_in_data = 0;
        for (size_t i = 0; i < allocated_blocks.size(); ++i) {
            uint32_t blk_idx = allocated_blocks[i];

            // prepare header
            BlockHdr bh;
            bh.isValid = false; // now used
            bh.size = static_cast<uint32_t>( std::min<uint64_t>(blk_payload, remaining) );
            bh.nxt = (i + 1 < allocated_blocks.size()) ? allocated_blocks[i + 1] : 0; // 0 means no next

            // write header
            file.seekp(static_cast<std::streamoff>(blk_idx));
            file.write(reinterpret_cast<const char*>(&bh), sizeof(bh));
            if (!file) { file.close(); return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR); }

            // write data chunk
            uint32_t chunk = bh.size;
            if (chunk > 0) {
                file.write(data + offset_in_data, static_cast<std::streamsize>(chunk));
                if (!file) { file.close(); return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR); }
                offset_in_data += chunk;
                remaining -= chunk;
            }
            // NOTE: we do NOT explicitly pad the block to header.block_size here; format/reader use bh.size to know how many bytes to read.
        }

        // --- 6) Persist the new FileEntry into files vector and tree structures ---
        uint32_t new_file_index = allocateInode();
        files.push_back(newEntry);

        // ensure Root vector capacity
        if (Root.size() <= new_file_index) Root.resize(new_file_index + 1);
        Root[new_file_index].prnt = parent_idx;
        Root[parent_idx].childs.push_back(new_file_index);

        // update next_inode_number if you also use it elsewhere as unique id
        if (newEntry.inode >= next_inode_number) next_inode_number = newEntry.inode + 1;

        // --- 7) Create FileMetadata and insert to Mds ---
        FileMetadata meta(path, newEntry);
        meta.blocks_used = allocated_blocks.size();
        meta.actual_size = size;
        // Insert into Mds (assumed method). ADAPT if your AVL API differs.
        Mds.insert(path, meta);

        // --- 8) Update session last_activity & operations_count ---
        sess->last_activity = static_cast<uint64_t>(std::time(nullptr));
        sess->operations_count += 1;
        file.close();

        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }   
    
    int readFile(void* session, const std::string& path, char** buffer, size_t* size) {
        if (!session) 
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);

        SessionInfo* sess = static_cast<SessionInfo*>(session);

        if(Mds.size()!= 0 && Mds.exists(path)){
            return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
        }

        uint32_t file_idx = findFileIndexByPath(path);
        if (file_idx == UINT32_MAX)
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);

        FileEntry& entry = files[file_idx];
        if (entry.type != static_cast<uint8_t>(EntryType::FILE))
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

        // ---- 2. Prepare to read data ----
        *size = static_cast<size_t>(entry.size);
        *buffer = new char[*size + 1];
        if (!*buffer)
            return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);

        size_t remaining = *size;
        size_t total_read = 0;
        uint32_t current_block = entry.inode;

        if (!file.is_open()) {
            file.open(omni_path, std::ios::in | std::ios::binary);
            if (!file.is_open()) {
                delete[] *buffer;
                *buffer = nullptr;
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }
        }

        // ---- 3. Read block chain ----
        while (current_block != 0 && remaining > 0) {
            BlockHdr bh;

            // Read Block Header
            file.seekg(current_block);
            file.read(reinterpret_cast<char*>(&bh), sizeof(bh));
            if (!file) {
                delete[] *buffer;
                *buffer = nullptr;
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }

            // Sanity check
            if (bh.isValid) {
                // If marked free, that's an error in file system consistency
                delete[] *buffer;
                *buffer = nullptr;
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }

            size_t to_read = std::min(static_cast<size_t>(bh.size), remaining);
            file.read((*buffer) + total_read, static_cast<std::streamsize>(to_read));
            if (!file) {
                delete[] *buffer;
                *buffer = nullptr;
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }

            total_read += to_read;
            remaining -= to_read;
            current_block = bh.nxt;
        }

        // ---- 4. Null-terminate and finalize ----
        (*buffer)[*size] = '\0';

        // ---- 5. Update session ----
        sess->last_activity = static_cast<uint64_t>(std::time(nullptr));
        sess->operations_count++;
        file.close();
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int deleteFile(void* session, const std::string& path) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        SessionInfo* s = reinterpret_cast<SessionInfo*>(session);

        // --- 1. Check if file exists in Mds ---
        FileMetadata meta;
        if (!Mds.find(path, meta)) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
        }

        // --- 2. Get the FileEntry associated with this path ---
        FileEntry entry = meta.entry;
        if (entry.getType() == EntryType::DIRECTORY) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION); // not allowed to delete directory here
        }

        uint32_t file_idx = findFileIndexByPath(path);

        // --- 3. Remove metadata entry from AVL Tree (Mds) ---
        Mds.remove(path);

        // --- 4. Remove from vector<FileEntry> files ---
        if (file_idx < files.size()) {
            uint32_t prnt_idx = files[file_idx].parent_idx;
            for(int i=0;i< Root[prnt_idx].childs.size();i++){
                if(Root[prnt_idx].childs[i] == file_idx){
                    Root[prnt_idx].childs.erase(Root[prnt_idx].childs.begin() + i);
                    break;
                }
            }
            files.erase(files.begin() + file_idx);  // mark invalid
        }
        // --- 6. Free the data blocks ---
         if (!file.is_open()) {
            file.open(omni_path, std::ios::in | std::ios::binary | std::ios::out);
            if (!file.is_open()) {
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }
        }
        uint32_t current_block = entry.inode;
        while (current_block != 0) {

            BlockHdr bh;
            file.seekg(static_cast<std::streamoff>(current_block));
            file.read(reinterpret_cast<char*>(&bh), sizeof(bh));
            if (!file) break;

            uint32_t next_block = bh.nxt;

            // Mark block as free
            bh.isValid = true;
            bh.nxt = 0;
            bh.size =0;

            // Write back updated BlockHdr
            file.seekp(static_cast<std::streamoff>(current_block), std::ios::beg);
            file.write(reinterpret_cast<const char*>(&bh), sizeof(bh));

            // Add block back to free_segments
            free_segments.push_back(current_block);

            current_block = next_block;
        }
        file.close();
        // --- 7. Update session (optional) ---
        s->last_activity = std::time(nullptr);
        s->operations_count++;

        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }

    
    int createDirectory(void* session, const std::string& path) {
        if (!session) 
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        if(files.size() + 1 > config.max_files)
            return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);

        SessionInfo* s = reinterpret_cast<SessionInfo*>(session);
        
        if (path.empty() || path[0] != '/')
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);

        // Split the path into tokens
        vector<std::string> toks = splitPathTokens(path);
        if (toks.empty())
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);

        // 3️⃣ Traverse to find parent directory
        std::string dir_name = toks.back();
        toks.pop_back();

        uint32_t current_idx = root_idx;
        for (const auto& tok : toks) {
            bool found = false;
            for (uint32_t child_idx : Root[current_idx].childs) {
                if (files[child_idx].getType() == EntryType::DIRECTORY &&
                    std::string(files[child_idx].name) == tok) {
                    current_idx = child_idx;
                    found = true;
                    break;
                }
            }
            if (!found)
                return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH); // parent not found
        }

        // 4️⃣ Check if directory already exists in parent
        for (uint32_t child_idx : Root[current_idx].childs) {
            if (std::string(files[child_idx].name) == dir_name &&
                files[child_idx].getType() == EntryType::DIRECTORY) {
                return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
            }
        } 

        // 5️⃣ Create new directory FileEntry
        FileEntry newDir(dir_name, EntryType::DIRECTORY, 0, 
                         static_cast<uint32_t>(FilePermissions::OWNER_READ) |
                         static_cast<uint32_t>(FilePermissions::OWNER_WRITE) |
                         static_cast<uint32_t>(FilePermissions::OWNER_EXECUTE),
                         s->user.username, next_inode_number++);
        newDir.created_time = std::time(nullptr);
        newDir.modified_time = newDir.created_time;
        newDir.parent_idx = current_idx;

        // 6️⃣ Add to files and tree
        uint32_t newIdx = files.size();
        files.push_back(newDir);

        Tree tnode;
        tnode.prnt = current_idx;
        Root.push_back(tnode);

        Root[current_idx].childs.push_back(newIdx);
        s->last_activity = time(nullptr);
        s->operations_count++;
        // 7️⃣ Return success
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }

    
    int listDirectory(void* session, const std::string& path, FileEntry** entries, int* count) {
        // 1️⃣ Validate session
        if (!session)
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);

        SessionInfo* s = reinterpret_cast<SessionInfo*>(session);
        

        // 3️⃣ Validate path
        if (path.empty() || path[0] != '/')
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);

        // 4️⃣ Split path into tokens
        std::vector<std::string> toks = splitPathTokens(path);

        // Start from root
        uint32_t current_idx = root_idx;

        // 5️⃣ Traverse directories to reach target folder
        for (const auto& tok : toks) {
            bool found = false;
            for (uint32_t child_idx : Root[current_idx].childs) {
                if (std::string(files[child_idx].name) == tok &&
                    files[child_idx].getType() == EntryType::DIRECTORY) {
                    current_idx = child_idx;
                    found = true;
                    break;
                }
            }
            if (!found)
                return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
        }

        // 6️⃣ Get children of this directory
        const std::vector<uint32_t>& child_indices = Root[current_idx].childs;
        uint32_t n = (child_indices.size());

        // 7️⃣ Allocate memory for returning entries
        if (n > 0) {
            *entries = new FileEntry[n];
            for (int i = 0; i < n; ++i) {
                (*entries)[i] = files[child_indices[i]];
            }
        } 
        else {
            *entries = nullptr;
        }

        *count = n;

        // 8️⃣ Return success
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }

    
    int deleteDirectory(void* session, const std::string& path) {
        // 1️⃣ Validate session
        if (!session) 
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);

        // 2️⃣ Validate path
        if (path.empty() || path[0] != '/')
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);

        // 3️⃣ Split path into tokens (assuming helper function)
        std::vector<std::string> tokens = splitPathTokens(path);
        if (tokens.empty()) 
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);

        // 4️⃣ Traverse to find the directory node
        uint32_t current_idx = root_idx;
        bool found = true;
        string parent = tokens[tokens.size() - 2];
        for (size_t i = 0; i < tokens.size(); ++i) {
            found = false;
            for (uint32_t child : Root[current_idx].childs) {
                if (std::string(files[child].name) == tokens[i]) {
                    if (i == tokens.size() - 1) {
                        current_idx = child;
                        found = true;
                        break;
                    }
                    if (files[child].getType() == EntryType::DIRECTORY) {
                        current_idx = child;
                        found = true;
                        break;
                    }
                }
            }
            if (!found)
                return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }

        if (files[current_idx].getType() != EntryType::DIRECTORY)
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

        // 6️⃣ Check if directory is empty
        if (!Root[current_idx].childs.empty())
            return static_cast<int>(OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY);

        // 7️⃣ Remove directory from its parent's child list
        uint32_t parent_idx = Root[current_idx].prnt;
        int size = Root[parent_idx].childs.size();
        for(int i=0;i<size;i++){
            if(Root[parent_idx].childs[i] == current_idx){
                Root[parent_idx].childs.erase(Root[parent_idx].childs.begin() + i);
            }
        }
        files.erase(files.begin() + current_idx);
        // 9️⃣ Update session info (activity timestamp)
        SessionInfo* s = static_cast<SessionInfo*>(session);
        s->last_activity = std::time(nullptr);
        s->operations_count++;

        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }

    
    int fileExists(void* session, const std::string& path) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        SessionInfo* s = reinterpret_cast<SessionInfo*>(session);
        s->last_activity = std::time(nullptr);
        s->operations_count++;
        if(Mds.exists(path))
            return static_cast<int>(OFSErrorCodes::SUCCESS);
        
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }
    
    int directoryExists(void* session, const std::string& path) {
        
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        SessionInfo* s = reinterpret_cast<SessionInfo*>(session);
        s->last_activity = std::time(nullptr);
        s->operations_count++;
        if (path.empty() || path[0] != '/') 
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);

        std::vector<std::string> tokens = splitPathTokens(path);
        if (tokens.empty()) 
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);

        uint32_t current_idx = root_idx;
        bool found = true;

        for (size_t i = 0; i < tokens.size(); ++i) {
            found = false;
            for (uint32_t child : Root[current_idx].childs) {
                if (std::string(files[child].name) == tokens[i] &&
                    files[child].getType() == EntryType::DIRECTORY) {
                    current_idx = child;
                    found = true;
                    break;
                }
            }
            if (!found)
                return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        }

        // 5️⃣ Directory exists
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }

    
    int getMetadata(void* session, const std::string& path, FileMetadata* meta) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        SessionInfo* s = reinterpret_cast<SessionInfo*>(session);
        s->last_activity = std::time(nullptr);
        s->operations_count++;
        AVLNode<FileMetadata>* node = Mds.search(path);
        if(!node)
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
        memcpy(meta, &node->value, sizeof(FileMetadata));
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
    
    int getStats(void* session, FSStats* stats) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        stats = new FSStats();
        stats->total_size = header.total_size;
        stats->total_users = users.size(); // assuming AVLTree has size()
        stats->active_sessions = 1;        // update if you track active sessions
    
        // Count files, directories, and used space
        for (auto &f : files) {
            stats->used_space += f.size;
            if (f.getType() == EntryType::FILE) stats->total_files++;
            else if (f.getType() == EntryType::DIRECTORY) stats->total_directories++;
        }
    
        // Free space = total size - used space (or sum of free segments * block_size)
        stats->free_space = header.total_size - stats->used_space;
    
        // Fragmentation calculation
        // If free_segments are scattered, fragmentation increases
        uint64_t contiguous_free = 0;
        uint64_t total_free_blocks = free_segments.size();
        if (total_free_blocks > 0) {
            contiguous_free = 1;
            for (size_t i = 1; i < free_segments.size(); i++) {
                if (free_segments[i] != free_segments[i-1] + header.block_size) {
                    contiguous_free++;
                }
            }
        }
        stats->fragmentation = total_free_blocks > 0 ? 100.0 * (contiguous_free - 1) / total_free_blocks : 0.0;
    
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }

    
    int renameFile(void* session, const std::string& old_path, const std::string& new_path) {
        // 1️⃣ Validate session
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        SessionInfo* s = reinterpret_cast<SessionInfo*>(session);
        s->last_activity = std::time(nullptr);
        s->operations_count++;
        // 2️⃣ Validate paths
        if (old_path.empty() || old_path[0] != '/' || 
            new_path.empty() || new_path[0] != '/')
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
        if(old_path == new_path)
            return static_cast<int>(OFSErrorCodes::SUCCESS);

        if (Mds.exists(new_path))
                return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
        // 3️⃣ Split old and new paths
        std::vector<std::string> old_tokens = splitPathTokens(old_path);
        std::vector<std::string> new_tokens = splitPathTokens(new_path);
        if (old_tokens.empty() || new_tokens.empty()) 
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);

        // 4️⃣ Find the file to rename
        uint32_t current_idx = root_idx;

        AVLNode<FileMetadata>* node = Mds.search(old_path);
        if (!node)
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        FileEntry* target_file = &node->value.entry;

        if (!target_file)
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);

        // 6️⃣ Rename the file
        std::strncpy(target_file->name, new_tokens.back().c_str(), sizeof(target_file->name) - 1);
        target_file->name[sizeof(target_file->name) - 1] = '\0';

        // 7️⃣ Update modified time
        target_file->modified_time = std::time(nullptr);

        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }

    
    int editFile(void* session, const std::string& path, const char* data, size_t size, uint32_t index) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);

        // 1. Find the file entry
        AVLNode<FileMetadata>* node = Mds.search(path);
        if (!node)
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
        FileEntry* fileEntry = &node->value.entry;

        // 2. Calculate blocks required
        uint32_t block_size = header.block_size;
        uint32_t total_size_needed = index + size;
        uint32_t blocks_needed = (total_size_needed + block_size) / block_size;
        blocks_needed--;
        if(free_segments.size() < node->value.blocks_used - blocks_needed)
            return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
        
         if (!file.is_open()) {
            file.open(omni_path, std::ios::in | std::ios::binary | std::ios::out);
            if (!file.is_open()) {
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }
        }
        std::vector<uint32_t> file_blocks;
        uint32_t current_inode = fileEntry->inode;
        uint32_t write_index =  0;
        int i =0;
        while (true) {
            file_blocks.push_back(current_inode);
            file.seekg(current_inode);
            BlockHdr bh;
            file.read(reinterpret_cast<char*>(&bh), sizeof(bh));
            if(bh.nxt != 0){
                current_inode = bh.nxt;
            }
            else break;
        }
        i = index/block_size;
        write_index = file_blocks[i++] + index%block_size + sizeof(BlockHdr);
        file.close();
         if (!file.is_open()) {
            file.open(omni_path, std::ios::in | std::ios::binary | std::ios::out);
            if (!file.is_open()) {
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }
        }
        uint32_t remaining = size;
        uint32_t written = 0;

        while (written < size) {
            uint32_t write_size = std::min((block_size - (write_index % block_size)), remaining);

            file.seekp(write_index);
            file.write(data, write_size);

            remaining -= write_size;
            written += write_size;
            if (remaining == 0) break;

            if(i<file_blocks.size()) write_index = file_blocks[i++];
            else {
                BlockHdr bh;
                bh.size = min(block_size, remaining);
                bh.isValid = false;
                free_segments.erase(free_segments.begin());
                write_index = free_segments[0]; 
            }
        }
        file.close();
        // 6. Update file entry metadata
        fileEntry->size = std::max(fileEntry->size, static_cast<uint64_t>(index + size));

        // 7. Update session info (cast session to your session struct)
        ((SessionInfo*)session)->operations_count++;
        ((SessionInfo*)session)->last_activity = std::time(nullptr);

        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }


    
    int truncateFile(void* session, const std::string& path) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);

        AVLNode<FileMetadata>* node = Mds.search(path);
        if (!node)
            return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
        FileEntry* fileEntry = &node->value.entry;

        uint32_t block_size = header.block_size;
         if (!file.is_open()) {
            file.open(omni_path, std::ios::in | std::ios::binary | std::ios::out);
            if (!file.is_open()) {
                return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
            }
        }
        // 2. Read all blocks allocated to this file
        std::vector<uint32_t> file_blocks;
        uint32_t current_inode = fileEntry->inode;
        uint32_t write_index =  0;
        while (true) {
            file.seekg(current_inode);
            file_blocks.push_back(current_inode);
            BlockHdr bh;
            file.read(reinterpret_cast<char*>(&bh), sizeof(bh));
            if(bh.nxt != 0){
                current_inode = bh.nxt;
            }
            else break;
        }

        // 3. Overwrite each block with 'sir umer'
        const std::string fill_str = "sir umer";
        for (auto block_idx : file_blocks) {
            file.seekp(block_idx, std::ios::beg);
            for (uint32_t offset = 0; offset < block_size; offset += fill_str.size()) {
                uint32_t write_size = std::min(static_cast<uint32_t>(fill_str.size()), block_size - offset);
                file.write(fill_str.c_str(), write_size);
            }
        }

        auto s = reinterpret_cast<SessionInfo*>(session);
        s->operations_count++;
        s->last_activity = std::time(nullptr);

        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }

    
    int setPermissions(void* session, const std::string& path, uint32_t permissions) {
        if (!session) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
        
        uint32_t inode_num = findFileIndexByPath(path);
        if (inode_num < 0) {
            return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
        }
        files[inode_num].permissions = permissions;
        
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
    std::string p(path);
    return static_cast<OFSInstance*>(instance)->createFile(session, path, data, size);
}

int file_read(void* instance, void* session, const char* path, char** buffer, size_t* size) {
    if (!instance) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    return static_cast<OFSInstance*>(instance)->readFile(session, (path), buffer, size);
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