#include <iostream>
#include <string>
#include <chrono>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <nlohmann/json.hpp>
#include "/home/hanan/Projects/c++/file-verse/source/core/ofs_implementation.h"
#include "/home/hanan/Projects/c++/file-verse/source/data_structures/AVL.h"
#include "/home/hanan/Projects/c++/file-verse/source/data_structures/circularqueue.h"


using json = nlohmann::json;
using namespace std;
const string CONFIG_PATH = "/home/hanan/Projects/c++/file-verse/compiled/default.uconf";
const string OMNIFS_PATH = "/home/hanan/Projects/c++/file-verse/compiled/test.omni";
AVLTree<SessionInfo> sessions;
int PORT = 8080;
int MAX_CONNECTIONS = 20;
int QUEUE_TIMEOUT = 30; // seconds
void* fs_instance = nullptr;
struct Request {
    string request_str;
    int client_socket;
    chrono::system_clock::time_point timestamp;
    Request() = default;
};

mutex queue_mutex;

json make_error_response(const string& operation, const string& request_id, int error_code) {
    json resp;
    resp["status"] = "error";
    resp["operation"] = operation;
    resp["request_id"] = request_id;
    resp["error_code"] = error_code;
    resp["error_message"] = get_error_message(error_code);
    return resp;
}

bool parse_server_config() {
    ifstream file(CONFIG_PATH.c_str());
    if (!file.is_open()) {
        cerr << "Failed to open config file: " << CONFIG_PATH.c_str() << endl;
        return false;
    }

    string line;
    bool in_server_section = false;

    while (getline(file, line)) {
        // Remove leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty lines or comments
        if (line.empty() || line[0] == '#') continue;

        // Check for section
        if (line.front() == '[' && line.back() == ']') {
            string section = line.substr(1, line.size() - 2);
            in_server_section = (section == "server");
            continue;
        }

        if (!in_server_section) continue;

        // Parse key = value
        size_t eq_pos = line.find('=');
        if (eq_pos == string::npos) continue;

        string key = line.substr(0, eq_pos);
        string value = line.substr(eq_pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // Remove inline comments after #
        size_t comment_pos = value.find('#');
        if (comment_pos != string::npos)
            value = value.substr(0, comment_pos);

        value.erase(value.find_last_not_of(" \t") + 1);

        // Convert to int
        if (key == "port") PORT = stoi(value);
        else if (key == "max_connections") MAX_CONNECTIONS = stoi(value);
        else if (key == "queue_timeout") QUEUE_TIMEOUT = stoi(value);
    }

    file.close();
    return true;
}
CircularQueue<Request> request_queue(MAX_CONNECTIONS);

json make_json_response(const string status, const string& operation, const string& request_id, const json& result_data = {},const string& session_id = "") {
    json response;
    response["status"] = status;             // "success" or "error"
    response["operation"] = operation;
    response["request_id"] = request_id;
    response["data"] = result_data;
    return response;
}
bool validate_request(const json& req, string& error) {
    
    string op = req["operation"];
    string session = req["session_id"];
    const json& params = req["parameters"];
    string req_id = req["request_id"];
    if(session.empty() && op != "login")
        return false;
    if(req_id.empty()) 
        return false;
    // Validate operation
    if (op == "login") {
        if (!params.contains("username") || !params.contains("password")) {
            error = "Missing 'username' or 'password' for login";
            return false;
        }
    } 
    else if (op == "create_user") {
        if(!params.contains("username") || !params.contains("password") || !params.contains("role")){
            error = "Missing username, passowrd or role for " + op;
            return false;
        }
    }
    else if(op == "delete_user"){
        if(!params.contains("username")){
            error = "Missing username for " + op;
            return false;
        }
    }
    else if (op == "create_file") {
        if (!params.contains("path") || !params.contains("data")) {
            error = "Missing 'path' or 'data' or session_id for create_file";
            return false;
        }
    } 
    else if (op == "read_file" || op == "delete_file" || op == "truncate_file" || op == "exists_file") {
        if (!params.contains("path")) {
            error = "Missing 'path' or session_id for " + op;
            return false;
        }
    } 
    else if (op == "edit_file") {
        if (!params.contains("path") || !params.contains("data") || !params.contains("index")) {
            error = "Missing 'path', 'data', or 'index' or session_id for edit_file";
            return false;
        }
    } 
    else if (op == "list_directory" || op == "create_directory" || op == "delete_directory" || op == "exists_directory") {
        if (!params.contains("path")) {
            error = "Missing 'path' or session_id for " + op;
            return false;
        }
    } 
    else if(op == "rename_file"){
        if(!params.contains("path") || !params.contains("new")){
            error = "Missing old or new path";
            return false;
        }
    }
    else if (op == "get_metadata"){
        if(!params.contains("path")){
            error = "Missing path for " + op;
            return false;
        }
    }
    else if (op == "set_permissions"){
        if(!params.contains("path") || !params.contains("permissions")){
            error = "Missing path or permissions for " + op;
            return false;
        }
    }
    else if(op == "list_users" || op == "logout" || op == "get_session_info"  || op == "shutdown_system" || op == "format_system" || op == "initialize_system"){}
    else {
        error = "Unknown operation: " + op;
        return false;
    }

    return true;
}
void handle_request(Request req) {
    cout << req.request_str << endl;
    json j1 = json::parse(req.request_str);

    string op = j1["operation"];
    string session_id = j1["session_id"];
    json params = j1["parameters"];
    json response;
    response["operation"] = op;
    response["request_id"] = j1["request_id"];
    response["session_id"] = session_id;

    void* session_ptr = nullptr;
    if(op != "login" && !sessions.exists(session_id)) {
        response = make_error_response(j1["operation"], j1["request_id"], static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION));
        string buffer = response.dump();
        send(req.client_socket, buffer.c_str(), buffer.size(), 0);
        return;
    }
    if(op != "login"){
        AVLNode<SessionInfo>* node = sessions.search(session_id);
        session_ptr = &node->value;
    }

    int res;

    if(op == "login") {
        string username = params["username"];
        string password = params["password"];
        void* new_session = nullptr;
        res = user_login(fs_instance, username.c_str(), password.c_str(), &new_session);
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            SessionInfo* s = static_cast<SessionInfo*>(new_session);
            SessionInfo temp(*s);
            sessions.insert(s->session_id, temp);
            response["status"] = "success";
            response["session_id"] = s->session_id;
            response["data"]["result_data"] = s->session_id;
        }
        else {
            response = make_error_response(j1["operation"], j1["request_id"], static_cast<int>(OFSErrorCodes::ERROR_INVALID_CONFIG));
        }
    }
    else if(op == "logout") {
        res = user_logout(session_ptr);
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS))
            sessions.remove(session_id);
        else{
            response = make_error_response(j1["operation"], j1["request_id"], static_cast<int>(OFSErrorCodes::ERROR_INVALID_CONFIG));
        }
    }
    else if(op == "initialize_system") {
        AVLNode<SessionInfo>* node = sessions.search(session_id);
        if(node->value.user->role != UserRole::ADMIN){
            response = make_error_response(j1["operation"], j1["request_id"], static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED));
        }
        else{
            res = fs_init(&fs_instance, OMNIFS_PATH.c_str(), CONFIG_PATH.c_str());
            if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
                response["status"] = "success";
            }
            else{
                response = make_error_response(j1["operation"], j1["request_id"], res);
            }
        }
    }
    else if(op == "shutdown_system") {
        AVLNode<SessionInfo>* node = sessions.search(session_id);
        if(node->value.user->role != UserRole::ADMIN){
            response = make_error_response(j1["operation"], j1["request_id"], static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED));
        }
        else{
            fs_shutdown(fs_instance);
            fs_instance = nullptr;
            if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
                response["status"] = "success";
            }
            else{
                response = make_error_response(j1["operation"], j1["request_id"], res);
            }
        }
    }
    else if(op == "format_system") {
        AVLNode<SessionInfo>* node = sessions.search(session_id);
        if(node->value.user->role != UserRole::ADMIN){
            response = make_error_response(j1["operation"], j1["request_id"], static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED));
        }
        else{
            res = fs_init(&fs_instance, OMNIFS_PATH.c_str(), CONFIG_PATH.c_str());
            if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
                response["status"] = "success";
            }
            else{
                response = make_error_response(j1["operation"], j1["request_id"], res);
            }
        }
    }
    else if(op == "create_user") {
        AVLNode<SessionInfo>* node = sessions.search(session_id);
        if(node->value.user->role != UserRole::ADMIN){
            response = make_error_response(j1["operation"], j1["request_id"], static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED));
        }
        else{
            string username = params["username"];
            string password = params["password"];
            string Role = params["role"];

            UserRole role = static_cast<UserRole>(atoi(Role.c_str()));
            res = user_create(fs_instance, session_ptr, username.c_str(), password.c_str(), role);
            if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
                response["status"] = "success";
            }
            else{
                response = make_error_response(j1["operation"], j1["request_id"], res);
            }
        }
        
    }
    else if(op == "delete_user") {
        AVLNode<SessionInfo>* node = sessions.search(session_id);
        if(node->value.user->role != UserRole::ADMIN){
            response = make_error_response(j1["operation"], j1["request_id"], static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED));
        }
        else{
            string username = params["username"];
            res = user_delete(fs_instance, session_ptr, username.c_str());
            if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
                response["status"] = "success";
            }
            else{
                response = make_error_response(j1["operation"], j1["request_id"], res);
            }
        }
        
    }
    else if(op == "list_users") {
        AVLNode<SessionInfo>* node = sessions.search(session_id);
        if(node->value.user->role != UserRole::ADMIN){
            response = make_error_response(j1["operation"], j1["request_id"], static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED));
        }
        else{
            UserInfo* users = nullptr;
            int count = 0;
            res = user_list(fs_instance, session_ptr, &users, &count);
            if(res == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                response["status"] = "success";
                for(int i=0; i<count; i++){
                    response["data"]["users"].push_back({
                        {"username", users[i].username},
                        {"role", users[i].role},
                        {"is_active", users[i].is_active}
                    });
                }
                free_buffer(users);
            } 
            else {
                response = make_error_response(j1["operation"], j1["request_id"], res);
            }
        }
        
    }
    else if(op == "get_session_info") {
        SessionInfo info;
        res = get_session_info(fs_instance, session_ptr, &info);
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            response["status"] = "success";
            response["data"]["username"] = info.user->username;
            response["data"]["role"] = info.user->role;
            response["data"]["login_time"] = info.login_time;
        } 
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "system_info") {
        FSStats* stats;
        res = get_stats(fs_instance, session_ptr, stats);
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            response["status"] = "success";
            response["data"]["total_size"] = stats->total_size;
            response["data"]["used_space"] = stats->used_space;
            response["data"]["free_space"] = stats->free_space;
            response["data"]["total_files"] = stats->total_files;
            response["data"]["total_directories"] = stats->total_directories;
        } 
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "create_file") {
        string path = params["path"];
        string data = params["data"];
        res = file_create(fs_instance, session_ptr, path.c_str(), data.c_str(), data.size());
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
            response["status"] = "success";
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "read_file") {
        string path = params["path"];
        char* buffer = nullptr;
        size_t size = 0;
        res = file_read(fs_instance, session_ptr, path.c_str(), &buffer, &size);
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            response["status"] = "success";
            response["data"]["result_data"] = string(buffer, size);
            free_buffer(buffer);
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "edit_file") {
        string path = params["path"];
        string data = params["data"];
        int index = params["index"];
        res = file_edit(fs_instance, session_ptr, path.c_str(), data.c_str(), data.size(), index);
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
            response["status"] = "success";
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "delete_file") {
        string path = params["path"];
        res = file_delete(fs_instance, session_ptr, path.c_str());
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
            response["status"] = "success";
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "truncate_file") {
        string path = params["path"];
        res = file_truncate(fs_instance, session_ptr, path.c_str());
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
            response["status"] = "success";
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "exists_file") {
        string path = params["path"];
        res = file_exists(fs_instance, session_ptr, path.c_str());
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
            response["status"] = "success";
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "rename_file") {
        string old_path = params["path"];
        string new_path = params["new"];
        res = file_rename(fs_instance, session_ptr, old_path.c_str(), new_path.c_str());
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
            response["status"] = "success";
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "create_directory") {
        string path = params["path"];
        res = dir_create(fs_instance, session_ptr, path.c_str());
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
            response["status"] = "success";
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "list_directory") {
        string path = params["path"];
        FileEntry* entries = nullptr;
        int count = 0;
        res = dir_list(fs_instance, session_ptr, path.c_str(), &entries, &count);
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            response["status"] = "success";
            for(int i=0;i<count;i++){
                response["data"]["entries"].push_back({
                    {"name", entries[i].name},
                    {"type", entries[i].type},
                    {"size", entries[i].size},
                    {"owner", entries[i].owner},
                    {"permissions", entries[i].permissions}
                });
            }
            free_buffer(entries);
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "delete_directory") {
        string path = params["path"];
        res = dir_delete(fs_instance, session_ptr, path.c_str());
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
            response["status"] = "success";
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "exists_directory") {
        string path = params["path"];
        res = dir_exists(fs_instance, session_ptr, path.c_str());
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
            response["status"] = "success";
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "get_metadata") {
        string path = params["path"];
        FileMetadata meta;
        res = get_metadata(fs_instance, session_ptr, path.c_str(), &meta);
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            response["status"] = "success";
            response["data"]["path"] = meta.path;
            response["data"]["size"] = meta.entry->size;
            response["data"]["owner"] = meta.entry->owner;
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "set_permissions") {
        string path = params["path"];
        int permissions = params["permissions"];
        res = set_permissions(fs_instance, session_ptr, path.c_str(), permissions);
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)){
            response["status"] = "success";
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else if(op == "get_stats") {
        FSStats* stats;
        res = get_stats(fs_instance, session_ptr, stats);
        if(res == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            response["status"] = "success";
            response["data"]["total_size"] = stats->total_size;
            response["data"]["used_space"] = stats->used_space;
            response["data"]["free_space"] = stats->free_space;
            response["data"]["total_files"] = stats->total_files;
            response["data"]["total_directories"] = stats->total_directories;
        }
        else{
            response = make_error_response(j1["operation"], j1["request_id"], res);
        }
    }
    else {
        response = make_error_response(j1["operation"], j1["request_id"], static_cast<int>(OFSErrorCodes::ERROR_INVALID_CONFIG));
    }

    // Send response
    string buffer = response.dump();
    if(send(req.client_socket, buffer.c_str(), buffer.size(), 0) < buffer.size()){
        cout << "[Server] Warning: Not all bytes sent for request " << j1["request_id"] << endl;
    }
}

bool enqueue_request(const Request& req) {
    lock_guard<mutex> lock(queue_mutex);
    if (request_queue.size() >= MAX_CONNECTIONS) {
        return false;
    }
    request_queue.enqueue(req);
    return true;
}

// Function to process requests (just prints them for now)
void process_queue() {
    while (true) {
        lock_guard<mutex> lock(queue_mutex);
        while (!request_queue.empty()) {
            Request req = request_queue.dequeue();
            auto now = chrono::system_clock::now();
            auto age = chrono::duration_cast<chrono::seconds>(now - req.timestamp).count();
            if (age > QUEUE_TIMEOUT) {
                cout << "[Server] Request timed out: " << req.request_str << endl;
                continue;
            }
            handle_request(req);
        }
    }
}

int main() {
    fs_format(OMNIFS_PATH.c_str(), CONFIG_PATH.c_str());
    int res = fs_init(&fs_instance, OMNIFS_PATH.c_str(), CONFIG_PATH.c_str());
    if(OFSErrorCodes(res) != static_cast<OFSErrorCodes>(OFSErrorCodes::SUCCESS)){
        cout << "Failed to initialize";
        return 1;
    }
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    // Bind
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    cout << "[Server] Listening on port " << PORT << "..." << endl;

    // Start queue processor thread
    thread processor_thread(process_queue);
    processor_thread.detach();

    while (true) {

        // Accept new client
        if ((new_socket = accept(server_fd, nullptr, nullptr)) < 0) {
            perror("accept");
            continue;
        }

        // Read request
        char buffer[4096] = {0};
        int valread = read(new_socket, buffer, sizeof(buffer));
        if (valread <= 0) {
            close(new_socket);
            continue;
        }

        // Create request struct
        Request req;
        req.request_str = string(buffer, valread);
        json j = json::parse(req.request_str);
        string error;
        req.timestamp = chrono::system_clock::now();
        req.client_socket = new_socket;
        if(!validate_request(j, error)){
            cout << error << endl;
            json j = make_error_response(j["operation"], j["request_id"], static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION));
            string buffer = j.dump();
            send(req.client_socket, buffer.c_str(), buffer.size(), 0);
            continue;
        }

        // Enqueue request
        enqueue_request(req);

    }

    return 0;
}