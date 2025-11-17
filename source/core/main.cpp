#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctime>
#include<thread>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace std;

#define SERVER_PORT 8080
#define SERVER_HOST "127.0.0.1"

string session_id = "";
bool logged_in = false;
string generate_request_id() {
    string id = "REQ_";
    id += to_string(time(nullptr));
    id += "_";
    id += to_string(rand() % 1000000);
    return id;
}
int sock = 0;
// Send string to server and receive response
string send_request(const string &json_request) {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) cout << "ERROR: Could not create socket";

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_HOST, &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0)
        cout << "ERROR: Could not connect to server";

    send(sock, json_request.c_str(), json_request.size(), 0);
    char buffer[4096] = {0};
    while(recv(sock, buffer, sizeof(buffer) - 1, 0)<1){
        
    }
    return string(buffer);
}

// Build JSON request
string make_json_request(const string& op, const json& params_json) {
    json req;
    req["operation"]   = op;
    req["session_id"]  = session_id;           // assuming session_id is global or captured
    req["parameters"]  = params_json;          // directly insert JSON object
    req["request_id"]  = generate_request_id();
    return req.dump();
}
// Show menus
void showMainMenu() {
    cout << "\n===== OFS MAIN MENU =====\n";
    cout << "1. File Operations\n";
    cout << "2. Directory Operations\n";
    cout << "3. User Management\n";
    cout << "4. System Info\n";
    cout << "5. Session Info\n";
    cout << "0. Logout\n";
}
void showFileMenu() {
    cout << "\n--- File Operations ---\n";
    cout << "1. Create File\n";
    cout << "2. Read File\n";
    cout << "3. Edit File\n";
    cout << "4. Delete File\n";
    cout << "5. Truncate File\n";
    cout << "6. Rename File\n";
    cout << "7. File Exists\n";
    cout << "8. Get Metadata of File\n";
    cout << "0. Back\n";
}
void showDirMenu() {
    cout << "\n--- Directory Operations ---\n";
    cout << "1. Create Directory\n";
    cout << "2. List Directory\n";
    cout << "3. Delete Directory\n";
    cout << "0. Back\n";
}
void showUserMenu() {
    cout << "\n--- User Management ---\n";
    cout << "1. Create User\n";
    cout << "2. Delete User\n";
    cout << "3. List Users\n";
    cout << "0. Back\n";
}

string input(const string& prompt) {
    cout << prompt;
    string s;
    getline(cin, s);
    return s;
}
// --------------------- File Operations ---------------------
void handleFileOps() {
    while (true) {
        showFileMenu();
        cout << "Choice: ";
        string choice; 
        getline(cin, choice);

        if (choice == "0") return;

        string path, new_name, content, idx;
        json params;
        string req;

        if (choice == "1") {   // Create
            path = input("File path: ");
            content = input("Content: ");
            params["path"] = path;
            params["data"] = content;
            req = make_json_request("create_file", params);
        }
        else if (choice == "2") { // Read
            path = input("File path: ");
            params["path"] = path;
            req = make_json_request("read_file", params);
        }
        else if (choice == "3") { // Edit
            path = input("File path: ");
            content = input("New content: ");
            idx = input("Start index: ");
            params["path"] = path;
            params["data"] = content;
            params["index"] = stoi(idx);
            req = make_json_request("edit_file", params);
        }
        else if (choice == "4") { // Delete
            path = input("File path: ");
            params["path"] = path;
            req = make_json_request("delete_file", params);
        }
        else if (choice == "5") { // Delete
            path = input("File path: ");
            params["path"] = path;
            req = make_json_request("truncate_file", params);
        }
        else if (choice == "6") { // Rename
            path = input("Old path: ");
            new_name = input("New path: ");
            params["path"] = path;
            params["new"] = new_name;
            req = make_json_request("rename_file", params);
        }
        else if (choice == "7") {
            path = input("Path: ");
            params["path"] = path;
            req = make_json_request("exists_file", params);
        }
        else if (choice == "8") {
            path = input("Path: ");
            params["path"] = path;
            req = make_json_request("get_metadata", params);
        }
        else continue;
        string res = send_request(req);
        cout << res << endl;
    }
}
void handleSession(){
    json params;
    string req = make_json_request("get_session_info", params);
    string res = send_request(req);
    cout << res << endl;
}
// --------------------- Directory Operations ---------------------
void handleDirectoryOps() {
    while (true) {
        showDirMenu();
        cout << "Choice: ";
        string choice; 
        getline(cin, choice);

        if (choice == "0") return;

        string path;
        json params;
        string req;

        if (choice == "1") { // Create
            path = input("Directory path: ");
            params["path"] = path;
            req = make_json_request("create_directory", params);
        }
        else if (choice == "2") { // List
            path = input("Directory path: ");
            params["path"] = path;
            req = make_json_request("list_directory", params);
        }
        else if (choice == "3") { // Delete
            path = input("Directory path: ");
            params["path"] = path;
            req = make_json_request("delete_directory", params);
        }
        else if (choice == "4") { // Check Exists
            path = input("Directory path: ");
            params["path"] = path;
            req = make_json_request("exists_directory", params);
        }
        else continue;

        string res = send_request(req);
        cout << res << endl;
    }
}

// --------------------- User Operations ---------------------
void handleUserOps() {
    while (true) {
        showUserMenu();
        cout << "Choice: ";
        string choice; 
        getline(cin, choice);

        if (choice == "0") return;

        string username, password, role;
        json params;
        string req;

        if (choice == "1") { // Create User
            username = input("Username: ");
            password = input("Password: ");
            role = input("Role (NORMAL/ADMIN): ");
            params["username"] = username;
            params["password"] = password;
            params["role"] = role;
            req = make_json_request("create_user", params);
        }
        else if (choice == "2") { // Delete User
            username = input("Username to delete: ");
            params["username"] = username;
            req = make_json_request("delete_user", params);
        }
        else if (choice == "3") { // List Users
            req = make_json_request("list_users", params);
        }
        else continue;

        string res = send_request(req);
        cout << res << endl;
    }
}

// --------------------- System Operations ---------------------
void handleSystemOps() {
    while (true) {
        cout << "\n--- System Menu ---\n1.Initialize\n2.Shutdown\n3.Format\n4.System Info\n0.Back\nChoice: ";
        string choice; getline(cin, choice);

        if (choice == "0") return;

        json params;
        string req;

        if (choice == "1") req = make_json_request("initialize_system", params);
        else if (choice == "2") req = make_json_request("shutdown_system", params);
        else if (choice == "3") req = make_json_request("format_system", params);
        else if (choice == "4") req = make_json_request("system_info", params);
        else continue;

        string res = send_request(req);
        cout << res << endl;
    }
}

// --------------------- Login/Logout ---------------------
void handleAuth() {
    while (true) {
        cout << "\n--- Auth Menu ---\n1.Login\n2.Logout\n0.Back\nChoice: ";
        string choice; getline(cin, choice);

        if (choice == "0") return;

        json params;
        string req;

        if (choice == "1") { // Login
            string username = input("Username: ");
            string password = input("Password: ");
            params["username"] = username;
            params["password"] = password;
            req = make_json_request("login", params);
        }
        else if (choice == "2") { // Logout
            req = make_json_request("logout", params);
            logged_in = false;
        }
        else continue;

        string res = send_request(req);
        cout << res << endl;
        json j = json::parse(res);
        if(j["status"] != "success") return;
        if(j["operation"] == "login"){
            logged_in = true;
            j = j["data"];
            session_id = j["result_data"];
            break;
        } 
    }
}


int main() {
    srand(time(nullptr));
    while (!logged_in) {
        handleAuth();
    }

    while (logged_in) {
        showMainMenu();
        string choice = input("Choice: ");

        if (choice == "0") {
            cout << "Logging out...\n";
            session_id = "";
            close(sock);
            return 0;
        }

        if (choice == "1") handleFileOps();
        else if (choice == "2") handleDirectoryOps();
        else if (choice == "3") handleUserOps();
        else if (choice == "4") handleSystemOps();
        else if (choice == "5") handleSession();

    }

    return 0;
}
