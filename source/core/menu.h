#ifndef MENU_H
#define MENU_H

#include "ofs_implementation.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>

using namespace std;

// Test configuration
const string CONFIG_PATH = "/home/hanan/Projects/c++/file-verse/compiled/default.uconf";
const string OMNIFS_PATH = "/home/hanan/Projects/c++/file-verse/compiled/test.omni";

class InteractiveTester {
private:
    void* instance = nullptr;
    void* session = nullptr;
    bool user_logged_in = false;
    bool isInitialized = false;
public:
    void showLoginMenu() {
        cout << "\n=========================================" << endl;
        cout << "           OFS FILE SYSTEM" << endl;
        cout << "=========================================" << endl;
        cout << "1. Initialize System" << endl;
        cout << "2. Login" << endl;
        cout << "0. Exit" << endl;
        cout << "=========================================" << endl;
    }

    void showMainMenu() {
        cout << "\n=========================================" << endl;
        cout << "       WELCOME TO OFS FILE SYSTEM" << endl;
        cout << "=========================================" << endl;
        cout << "1. File Operations" << endl;
        cout << "2. Directory Operations" << endl;
        cout << "3. User Management" << endl;
        cout << "4. System Information" << endl;
        cout << "5. Run Quick Test" << endl;
        cout << "0. Logout" << endl;
        cout << "=========================================" << endl;
    }

    void showFileMenu() {
        cout << "\n--- File Operations ---" << endl;
        cout << "1. Create File" << endl;
        cout << "2. Read File" << endl;
        cout << "3. Edit File" << endl;
        cout << "4. Delete File" << endl;
        cout << "5. Rename File" << endl;
        cout << "6. Truncate File" << endl;
        cout << "7. Check File Exists" << endl;
        cout << "8. Get File Metadata" << endl;
        cout << "0. Back to Main Menu" << endl;
    }

    void showDirectoryMenu() {
        cout << "\n--- Directory Operations ---" << endl;
        cout << "1. Create Directory" << endl;
        cout << "2. List Directory" << endl;
        cout << "3. Delete Directory" << endl;
        cout << "4. Check Directory Exists" << endl;
        cout << "0. Back to Main Menu" << endl;
    }

    void showUserMenu() {
        cout << "\n--- User Management ---" << endl;
        cout << "1. Create User" << endl;
        cout << "2. Delete User" << endl;
        cout << "3. List Users" << endl;
        cout << "4. Get Session Info" << endl;
        cout << "0. Back to Main Menu" << endl;
    }

    string getInput(const string& prompt) {
        cout << prompt;
        string input;
        getline(cin, input);
        return input;
    }

    void printResult(int result, const string& operation) {
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            cout << "✓ " << operation << " - SUCCESS" << endl;
        } else {
            cout << "✗ " << operation << " - FAILED: " << get_error_message(result) << endl;
        }
    }

    bool initializeSystem() {
        cout << "\nInitializing file system..." << endl;
        
        // First, try to format if file doesn't exist or user wants to reformat
        string format_choice = getInput("Format new file system? (y/n): ");
        if (format_choice == "y" || format_choice == "Y") {
            cout << "Formatting file system..." << endl;
            int result = fs_format(OMNIFS_PATH.c_str(), CONFIG_PATH.c_str());
            if (result != static_cast<int>(OFSErrorCodes::SUCCESS)) {
                cout << "Format failed. Trying to use existing file system..." << endl;
            } else {
                cout << "✓ File system formatted successfully" << endl;
            }
        }
        isInitialized = true;
        // Initialize the file system
        int result = fs_init(&instance, OMNIFS_PATH.c_str(), CONFIG_PATH.c_str());
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            cout << "✓ File system initialized successfully" << endl;
            return true;
        } else {
            cout << "✗ Failed to initialize file system: " << get_error_message(result) << endl;
            return false;
        }
    }

    bool loginUser() {
        int result = 0;
        if(!isInitialized)
            result = fs_init(&instance, OMNIFS_PATH.c_str(), CONFIG_PATH.c_str());
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            cout << "✓ File system initialized successfully" << endl;
        } else {
            cout << "✗ Failed to initialize file system: " << get_error_message(result) << endl;
            return false;
        }
        cout << "\n=== USER LOGIN ===" << endl;
        string username = getInput("Username: ");
        string password = getInput("Password: ");
        
        result = user_login(instance, username.c_str(), password.c_str(), &session);
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            user_logged_in = true;
            cout << "✓ Login successful! Welcome, " << username << "!" << endl;
            
            // Show session info
            SessionInfo info;
            if (get_session_info(instance, session, &info) == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                cout << "Role: " << (info.user.role == UserRole::ADMIN ? "Administrator" : "Normal User") << endl;
            }
            return true;
        } else {
            cout << "✗ Login failed: " << get_error_message(result) << endl;
            return false;
        }
    }

    void handleFileOperations() {
        while (true) {
            showFileMenu();
            string choice = getInput("Enter your choice: ");

            if (choice == "0") break;

            if (choice == "1") {
                // Create File
                string path = getInput("Enter file path: ");
                string content = getInput("Enter file content: ");
                int result = file_create(instance, session, path.c_str(), content.c_str(), content.length());
                printResult(result, "Create file " + path);

            } else if (choice == "2") {
                // Read File
                string path = getInput("Enter file path to read: ");
                char* buffer = nullptr;
                size_t size = 0;
                int result = file_read(instance, session, path.c_str(), &buffer, &size);
                printResult(result, "Read file " + path);
                if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                    cout << "Content: \"" << string(buffer, size) << "\"" << endl;
                    free_buffer(buffer);
                }

            } else if (choice == "3") {
                // Edit File
                string path = getInput("Enter file path to edit: ");
                string content = getInput("Enter new content: ");
                string index_str = getInput("Enter starting index: ");
                try {
                    uint32_t index = stoi(index_str);
                    int result = file_edit(instance, session, path.c_str(), content.c_str(), content.length(), index);
                    printResult(result, "Edit file " + path);
                } catch (...) {
                    cout << "Invalid index. Please enter a number." << endl;
                }

            } else if (choice == "4") {
                // Delete File
                string path = getInput("Enter file path to delete: ");
                int result = file_delete(instance, session, path.c_str());
                printResult(result, "Delete file " + path);

            } else if (choice == "5") {
                // Rename File
                string old_path = getInput("Enter current file path: ");
                string new_path = getInput("Enter new file path: ");
                int result = file_rename(instance, session, old_path.c_str(), new_path.c_str());
                printResult(result, "Rename file from " + old_path + " to " + new_path);

            } else if (choice == "6") {
                // Truncate File
                string path = getInput("Enter file path to truncate: ");
                int result = file_truncate(instance, session, path.c_str());
                printResult(result, "Truncate file " + path);
                if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                    cout << "File content replaced with 'siruamr'" << endl;
                }

            } else if (choice == "7") {
                // Check File Exists
                string path = getInput("Enter file path to check: ");
                int result = file_exists(instance, session, path.c_str());
                if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                    cout << "✓ File exists: " << path << endl;
                } else {
                    cout << "✗ File does not exist: " << path << endl;
                }

            } else if (choice == "8") {
                // Get File Metadata
                string path = getInput("Enter file path: ");
                FileMetadata meta;
                int result = get_metadata(instance, session, path.c_str(), &meta);
                printResult(result, "Get metadata for " + path);
                if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                    cout << "Path: " << meta.path << endl;
                    cout << "Name: " << meta.entry.name << endl;
                    cout << "Type: " << (meta.entry.type == 0 ? "FILE" : "DIRECTORY") << endl;
                    cout << "Size: " << meta.entry.size << " bytes" << endl;
                    cout << "Permissions: " << meta.entry.permissions << endl;
                    cout << "Owner: " << meta.entry.owner << endl;
                    cout << "Created: " << meta.entry.created_time << endl;
                    cout << "Modified: " << meta.entry.modified_time << endl;
                    cout << "Blocks used: " << meta.blocks_used << endl;
                }

            } else {
                cout << "Invalid choice. Please try again." << endl;
            }
        }
    }

    void handleDirectoryOperations() {
        while (true) {
            showDirectoryMenu();
            string choice = getInput("Enter your choice: ");

            if (choice == "0") break;

            if (choice == "1") {
                // Create Directory
                string path = getInput("Enter directory path: ");
                int result = dir_create(instance, session, path.c_str());
                printResult(result, "Create directory " + path);

            } else if (choice == "2") {
                // List Directory
                string path = getInput("Enter directory path to list: ");
                FileEntry* entries = nullptr;
                int count = 0;
                int result = dir_list(instance, session, path.c_str(), &entries, &count);
                printResult(result, "List directory " + path);
                if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                    if (count == 0) {
                        cout << "Directory is empty." << endl;
                    } else {
                        cout << "Directory contains " << count << " items:" << endl;
                        cout << "----------------------------------------" << endl;
                        for (int i = 0; i < count; i++) {
                            cout << "Name: " << entries[i].name << endl;
                            cout << "Type: " << (entries[i].type == 0 ? "FILE" : "DIRECTORY") << endl;
                            cout << "Size: " << entries[i].size << " bytes" << endl;
                            cout << "Owner: " << entries[i].owner << endl;
                            cout << "Permissions: " << entries[i].permissions << endl;
                            cout << "----------------------------------------" << endl;
                        }
                    }
                    free_buffer(entries);
                }

            } else if (choice == "3") {
                // Delete Directory
                string path = getInput("Enter directory path to delete: ");
                int result = dir_delete(instance, session, path.c_str());
                printResult(result, "Delete directory " + path);

            } else if (choice == "4") {
                // Check Directory Exists
                string path = getInput("Enter directory path to check: ");
                int result = dir_exists(instance, session, path.c_str());
                if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                    cout << "✓ Directory exists: " << path << endl;
                } else {
                    cout << "✗ Directory does not exist: " << path << endl;
                }

            } else {
                cout << "Invalid choice. Please try again." << endl;
            }
        }
    }

    void handleUserManagement() {
        while (true) {
            showUserMenu();
            string choice = getInput("Enter your choice: ");

            if (choice == "0") break;

            if (choice == "1") {
                // Create User (Admin only)
                SessionInfo info;
                if (get_session_info(instance, session, &info) == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                    if (info.user.role != UserRole::ADMIN) {
                        cout << "✗ Permission denied. Only administrators can create users." << endl;
                        continue;
                    }
                }

                string username = getInput("Enter new username: ");
                string password = getInput("Enter password: ");
                string role_str = getInput("Enter role (0 for NORMAL, 1 for ADMIN): ");
                UserRole role = (role_str == "1") ? UserRole::ADMIN : UserRole::NORMAL;
                int result = user_create(instance, session, username.c_str(), password.c_str(), role);
                printResult(result, "Create user " + username);

            } else if (choice == "2") {
                // Delete User (Admin only)
                SessionInfo info;
                if (get_session_info(instance, session, &info) == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                    if (info.user.role != UserRole::ADMIN) {
                        cout << "✗ Permission denied. Only administrators can delete users." << endl;
                        continue;
                    }
                }

                string username = getInput("Enter username to delete: ");
                int result = user_delete(instance, session, username.c_str());
                printResult(result, "Delete user " + username);

            } else if (choice == "3") {
                // List Users (Admin only)
                SessionInfo info;
                if (get_session_info(instance, session, &info) == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                    if (info.user.role != UserRole::ADMIN) {
                        cout << "✗ Permission denied. Only administrators can list users." << endl;
                        continue;
                    }
                }

                UserInfo* users = nullptr;
                int count = 0;
                int result = user_list(instance, session, &users, &count);
                printResult(result, "List users");
                if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                    cout << "System has " << count << " users:" << endl;
                    cout << "----------------------------------------" << endl;
                    for (int i = 0; i < count; i++) {
                        cout << "Username: " << users[i].username << endl;
                        cout << "Role: " << (users[i].role == UserRole::ADMIN ? "ADMIN" : "NORMAL") << endl;
                        cout << "Created: " << users[i].created_time << endl;
                        cout << "Last Login: " << users[i].last_login << endl;
                        cout << "Active: " << (users[i].is_active ? "Yes" : "No") << endl;
                        cout << "----------------------------------------" << endl;
                    }
                    free_buffer(users);
                }

            } else if (choice == "4") {
                // Get Session Info
                SessionInfo info;
                int result = get_session_info(instance, session, &info);
                printResult(result, "Get session info");
                if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
                    cout << "Session ID: " << info.session_id << endl;
                    cout << "User: " << info.user.username << endl;
                    cout << "Role: " << (info.user.role == UserRole::ADMIN ? "ADMIN" : "NORMAL") << endl;
                    cout << "Login Time: " << info.login_time << endl;
                    cout << "Last Activity: " << info.last_activity << endl;
                    cout << "Operations Count: " << info.operations_count << endl;
                }

            } else {
                cout << "Invalid choice. Please try again." << endl;
            }
        }
    }

    void showSystemInformation() {
        FSStats stats;
        int result = get_stats(instance, session, &stats);
        printResult(result, "Get system statistics");
        if (result == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            cout << "\n=== FILE SYSTEM STATISTICS ===" << endl;
            cout << "Total Size: " << stats.total_size << " bytes" << endl;
            cout << "Used Space: " << stats.used_space << " bytes" << endl;
            cout << "Free Space: " << stats.free_space << " bytes" << endl;
            cout << "Total Files: " << stats.total_files << endl;
            cout << "Total Directories: " << stats.total_directories << endl;
            cout << "Total Users: " << stats.total_users << endl;
            cout << "Active Sessions: " << stats.active_sessions << endl;
            
            // Calculate usage percentage
            double usage_percent = (static_cast<double>(stats.used_space) / stats.total_size) * 100;
            cout << "Usage: " << usage_percent << "%" << endl;
            
            cout << "=========================================" << endl;
        }
    }

    void runQuickTest() {
        cout << "\nRunning quick test..." << endl;
        
        // Create a test directory
        cout << "1. Creating test directory..." << endl;
        dir_create(instance, session, "/quick_test");
        
        // Create a test file
        cout << "2. Creating test file..." << endl;
        file_create(instance, session, "/quick_test/test_file.txt", "Hello from quick test!", 22);
        
        // Read the file
        cout << "3. Reading test file..." << endl;
        char* buffer = nullptr;
        size_t size = 0;
        if (file_read(instance, session, "/quick_test/test_file.txt", &buffer, &size) == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            cout << "   Content: " << string(buffer, size) << endl;
            free_buffer(buffer);
        }
        
        // List the directory
        cout << "4. Listing test directory..." << endl;
        FileEntry* entries = nullptr;
        int count = 0;
        if (dir_list(instance, session, "/quick_test", &entries, &count) == static_cast<int>(OFSErrorCodes::SUCCESS)) {
            cout << "   Found " << count << " items" << endl;
            free_buffer(entries);
        }
        
        cout << "✓ Quick test completed!" << endl;
    }

    void run() {
        cout << "Welcome to OFS File System!" << endl;
        cout << "File: " << OMNIFS_PATH << endl;
        cout << "Config: " << CONFIG_PATH << endl;

        // Login Phase
        while (true) {
            showLoginMenu();
            string choice = getInput("Enter your choice: ");

            if (choice == "0") {
                cout << "Goodbye!" << endl;
                return;
            } else if (choice == "1") {
                initializeSystem();
            } else if (choice == "2") {
                if (loginUser()) {
                    break; // Exit login loop and proceed to main menu
                }
            } else {
                cout << "Invalid choice. Please try again." << endl;
            }
        }

        // Main Operations Phase (after successful login)
        while (true) {
            showMainMenu();
            string choice = getInput("Enter your choice: ");

            if (choice == "0") {
                cout << "Logging out..." << endl;
                break;
            } else if (choice == "1") {
                handleFileOperations();
            } else if (choice == "2") {
                handleDirectoryOperations();
            } else if (choice == "3") {
                handleUserManagement();
            } else if (choice == "4") {
                showSystemInformation();
            } else if (choice == "5") {
                runQuickTest();
            } else {
                cout << "Invalid choice. Please try again." << endl;
            }
        }

        // Cleanup
        if (user_logged_in) {
            user_logout(session);
            cout << "✓ Logged out successfully" << endl;
        }
        fs_shutdown(instance);
        cout << "✓ System shutdown successfully" << endl;
    }
};

#endif