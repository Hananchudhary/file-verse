#include <iostream>
#include <cassert>
#include <cstring>
#include "menu.h"
#include "ofs_implementation.h"

using namespace std;
string omni_path = "/home/hanan/Projects/c++/file-verse/compiled/test.omni";
string config_path = "/home/hanan/Projects/c++/file-verse/compiled/default.uconf";
void test_format_and_init() {
    cout << "=== Testing Format and Init ===" << endl;
    
    // Test format
    int result = fs_format(omni_path.c_str(), config_path.c_str());
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ Format successful" << endl;
    
    // Test init
    void* instance = nullptr;
    result = fs_init(&instance, omni_path.c_str(), config_path.c_str());
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(instance != nullptr);
    cout << "✓ Init successful" << endl;
    
    fs_shutdown(instance);
    cout << "✓ Shutdown successful" << endl;
}

void test_user_management() {
    cout << "\n=== Testing User Management ===" << endl;
    
    void* instance = nullptr;
    fs_init(&instance, omni_path.c_str(), config_path.c_str());
    
    // Test admin login
    void* admin_session = nullptr;
    int result = user_login(instance, "admin", "admin123", &admin_session);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(admin_session != nullptr);
    cout << "✓ Admin login successful" << endl;
    
    // Test create user
    result = user_create(instance, admin_session, "testuser", "password123", UserRole::NORMAL);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ User creation successful" << endl;
    
    // Test duplicate user creation
    result = user_create(instance, admin_session, "testuser", "password123", UserRole::NORMAL);
    assert(result == static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS));
    cout << "✓ Duplicate user prevention working" << endl;
    
    // Test list users
    UserInfo* users = nullptr;
    int count = 0;
    result = user_list(instance, admin_session, &users, &count);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(count >= 2); // admin + testuser
    cout << "✓ User list successful, found " << count << " users" << endl;
    
    // Test normal user login
    void* user_session = nullptr;
    result = user_login(instance, "testuser", "password123", &user_session);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(user_session != nullptr);
    cout << "✓ Normal user login successful" << endl;
    
    // Test delete user
    result = user_delete(instance, admin_session, "testuser");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ User deletion successful" << endl;
    
    // Verify user is deleted
    result = user_login(instance, "testuser", "password123", &user_session);
    assert(result == static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND));
    cout << "✓ User deletion verified" << endl;
    
    delete[] users;
    user_logout(admin_session);
    fs_shutdown(instance);
}

void test_file_operations() {
    cout << "\n=== Testing File Operations ===" << endl;
    
    void* instance = nullptr;
    fs_init(&instance, omni_path.c_str(), config_path.c_str());
    
    void* admin_session = nullptr;
    user_login(instance, "admin", "admin123", &admin_session);
    
    // Test file creation
    const char* test_data = "Hello, World! This is test file content.";
    int result = file_create(instance, admin_session, "/test.txt", test_data, strlen(test_data));
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ File creation successful" << endl;
    
    // Test duplicate file creation
    result = file_create(instance, admin_session, "/test.txt", test_data, strlen(test_data));
    assert(result == static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS));
    cout << "✓ Duplicate file prevention working" << endl;
    
    // Test file exists
    result = file_exists(instance, admin_session, "/test.txt");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ File exists check working" << endl;
    
    // Test file read
    char* buffer = nullptr;
    size_t size = 0;
    result = file_read(instance, admin_session, "/test.txt", &buffer, &size);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(buffer != nullptr);
    assert(size == strlen(test_data));
    assert(strcmp(buffer, test_data) == 0);
    cout << "✓ File read successful, content: " << buffer << endl;
    
    // Test file metadata
    FileMetadata meta;
    result = get_metadata(instance, admin_session, "/test.txt", &meta);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(strcmp(meta.entry.name, "test.txt") == 0);
    assert(meta.entry.size == strlen(test_data));
    cout << "✓ File metadata retrieval successful" << endl;
    
    // Test file edit
    const char* edit_data = " Edited content!";
    result = file_edit(instance, admin_session, "/test.txt", edit_data, strlen(edit_data), 5);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ File edit successful" << endl;
    
    // Verify edit
    free_buffer(buffer);
    buffer = nullptr;
    result = file_read(instance, admin_session, "/test.txt", &buffer, &size);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ File after edit: " << buffer << endl;
    
    // Test file rename
    result = file_rename(instance, admin_session, "/test.txt", "/renamed_file.txt");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ File rename successful" << endl;
    
    // Verify old file doesn't exist
    result = file_exists(instance, admin_session, "/test.txt");
    assert(result == static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND));
    
    // Verify new file exists
    result = file_exists(instance, admin_session, "/renamed_file.txt");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    
    // Test file truncate
    result = file_truncate(instance, admin_session, "/renamed_file.txt");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ File truncate successful" << endl;
    
    // Test file deletion
    result = file_delete(instance, admin_session, "/renamed_file.txt");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ File deletion successful" << endl;
    
    // Verify deletion
    result = file_exists(instance, admin_session, "/renamed_file.txt");
    assert(result == static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND));
    
    free_buffer(buffer);
    user_logout(admin_session);
    fs_shutdown(instance);
}

void test_directory_operations() {
    cout << "\n=== Testing Directory Operations ===" << endl;
    
    void* instance = nullptr;
    fs_init(&instance, omni_path.c_str(), config_path.c_str());
    
    void* admin_session = nullptr;
    user_login(instance, "admin", "admin123", &admin_session);
    
    // Test directory creation
    int result = dir_create(instance, admin_session, "/testdir");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ Directory creation successful" << endl;
    
    // Test nested directory creation
    result = dir_create(instance, admin_session, "/testdir/subdir");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ Nested directory creation successful" << endl;
    
    // Test directory exists
    result = dir_exists(instance, admin_session, "/testdir");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ Directory exists check working" << endl;
    
    // Create files in directory
    const char* file1_data = "File 1 content";
    result = file_create(instance, admin_session, "/testdir/file1.txt", file1_data, strlen(file1_data));
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    
    const char* file2_data = "File 2 content";
    result = file_create(instance, admin_session, "/testdir/file2.txt", file2_data, strlen(file2_data));
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    
    // Test directory listing
    FileEntry* entries = nullptr;
    int count = 0;
    result = dir_list(instance, admin_session, "/testdir", &entries, &count);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(count >= 3); // subdir + file1 + file2
    cout << "✓ Directory listing successful, found " << count << " entries" << endl;
    
    // Print directory contents
    for (int i = 0; i < count; i++) {
        cout << "  - " << entries[i].name << " (" 
             << (entries[i].getType() == EntryType::DIRECTORY ? "DIR" : "FILE") 
             << ")" << endl;
    }
    
    // Test directory deletion (should fail - not empty)
    result = dir_delete(instance, admin_session, "/testdir");
    assert(result == static_cast<int>(OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY));
    cout << "✓ Directory non-empty deletion prevention working" << endl;
    
    // Delete files first
    result = file_delete(instance, admin_session, "/testdir/file1.txt");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    
    result = file_delete(instance, admin_session, "/testdir/file2.txt");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    
    result = dir_delete(instance, admin_session, "/testdir/subdir");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    
    // Now delete directory
    result = dir_delete(instance, admin_session, "/testdir");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ Directory deletion successful" << endl;
    
    delete[] entries;
    user_logout(admin_session);
    fs_shutdown(instance);
}

void test_permissions_and_stats() {
    cout << "\n=== Testing Permissions and Statistics ===" << endl;
    
    void* instance = nullptr;
    fs_init(&instance, omni_path.c_str(), config_path.c_str());
    
    void* admin_session = nullptr;
    user_login(instance, "admin", "admin123", &admin_session);
    
    // Create a test file
    const char* test_data = "Test data for permissions";
    int result = file_create(instance, admin_session, "/ptest.txt", test_data, strlen(test_data));
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    
    // Test set permissions
    result = set_permissions(instance, admin_session, "/ptest.txt", 0644);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ Set permissions successful" << endl;
    
    // Test get statistics
    FSStats* stats = nullptr;
    result = get_stats(instance, admin_session, stats);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(stats->total_size > 0);
    
    // Test session info
    SessionInfo session_info;
    result = get_session_info(instance, admin_session, &session_info);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(strcmp(session_info.user.username, "admin") == 0);
    cout << "✓ Session info retrieval successful" << endl;
    
    // Cleanup
    file_delete(instance, admin_session, "/ptest.txt");
    user_logout(admin_session);
    fs_shutdown(instance);
}

void test_error_handling() {
    cout << "\n=== Testing Error Handling ===" << endl;
    
    void* instance = nullptr;
    fs_init(&instance, omni_path.c_str(), config_path.c_str());
    
    void* admin_session = nullptr;
    user_login(instance, "admin", "admin123", &admin_session);
    
    // Test invalid path
    int result = file_create(instance, admin_session, "invalid_path", "data", 4);
    assert(result == static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH));
    cout << "✓ Invalid path handling working" << endl;
    
    // Test non-existent file operations
    result = file_read(instance, admin_session, "/nonexistent.txt", nullptr, nullptr);
    assert(result == static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND));
    cout << "✓ Non-existent file handling working" << endl;
    
    // Test invalid session
    void* invalid_session = nullptr;
    result = file_create(instance, invalid_session, "/test.txt", "data", 4);
    assert(result == static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION));
    cout << "✓ Invalid session handling working" << endl;
    
    user_logout(admin_session);
    fs_shutdown(instance);
}

void test_larg_operations() {
    cout << "\n=== Testing Large File Operations ===" << endl;
    
    void* instance = nullptr;
    fs_init(&instance, omni_path.c_str(), config_path.c_str());
    
    void* admin_session = nullptr;
    user_login(instance, "admin", "admin123", &admin_session);
    
    // Create a large file (10KB)
    string large_data;
    for (int i = 0; i < 1024; i++) {
        large_data += "0123456789"; // 10 bytes per iteration
    }
    
    int result = file_create(instance, admin_session, "/larg.txt", large_data.c_str(), large_data.size());
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ Large file creation successful (" << large_data.size() << " bytes)" << endl;
    
    // Read back and verify
    char* buffer = nullptr;
    size_t size = 0;
    result = file_read(instance, admin_session, "/larg.txt", &buffer, &size);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(size == large_data.size());
    assert(memcmp(buffer, large_data.c_str(), size) == 0);
    cout << "✓ Large file read and verification successful" << endl;
    
    // Cleanup
    free_buffer(buffer);
    file_delete(instance, admin_session, "/larg.txt");
    user_logout(admin_session);
    fs_shutdown(instance);
}

void test_edge_cases() {
    cout << "\n=== Testing Edge Cases ===" << endl;
    
    void* instance = nullptr;
    fs_init(&instance, omni_path.c_str(), config_path.c_str());
    
    void* admin_session = nullptr;
    user_login(instance, "admin", "admin123", &admin_session);
    
    // Test empty file creation
    int result = file_create(instance, admin_session, "/empty.txt", "", 0);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ Empty file creation successful" << endl;
    
    // Test reading empty file
    char* buffer = nullptr;
    size_t size = 0;
    result = file_read(instance, admin_session, "/empty.txt", &buffer, &size);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(size == 0);
    assert(buffer != nullptr);
    cout << "✓ Empty file read successful" << endl;
    
    // Test root directory operations
    FileEntry* entries = nullptr;
    int count = 0;
    result = dir_list(instance, admin_session, "/", &entries, &count);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    assert(count >= 0);
    cout << "✓ Root directory listing successful, found " << count << " entries" << endl;
    
    // Test creating file with same name as directory (should work in different contexts)
    result = file_create(instance, admin_session, "/test_name", "file data", 9);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    
    result = dir_create(instance, admin_session, "/test_name_dir");
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    
    cout << "✓ File and directory with similar names handled correctly" << endl;
    
    // Cleanup
    free_buffer(buffer);
    file_delete(instance, admin_session, "/empty.txt");
    file_delete(instance, admin_session, "/test_name");
    dir_delete(instance, admin_session, "/test_name_dir");
    delete[] entries;
    user_logout(admin_session);
    fs_shutdown(instance);
}

int main1() {
    cout << "Starting OFS Implementation Test Suite" << endl;
    cout << "======================================" << endl;
    
    try {
        test_format_and_init();
        test_user_management();
        test_file_operations();
        test_directory_operations();
        test_permissions_and_stats();
        test_error_handling();
        test_larg_operations();
        test_edge_cases();
        
        cout << "\n🎉 ALL TESTS PASSED! 🎉" << endl;
        cout << "OFS Implementation is working correctly." << endl;
        
    } catch (const exception& e) {
        cout << "\n❌ TEST FAILED: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
int main2(){
    InteractiveTester menu;
    menu.run();
    return 0;
}
int main(){
     cout << "\n=== Testing File Operations ===" << endl;
    
    void* instance = nullptr;
    fs_init(&instance, omni_path.c_str(), config_path.c_str());
    
    void* admin_session = nullptr;
    user_login(instance, "admin", "admin123", &admin_session);
    
    // Test file creation
    string test_data = "Hello, World! This is test file content.";
    while(test_data.size() < 4100) 
        test_data+=test_data;
    int result = file_create(instance, admin_session, "/test.txt", test_data.c_str(), test_data.size());
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    cout << "✓ File creation successful" << endl;
    char* buffer = nullptr;
    size_t size = 0;

    result = file_read(instance, admin_session, "/test.txt", &buffer, &size);
    assert(result == static_cast<int>(OFSErrorCodes::SUCCESS));
    // assert(buffer == nullptr);
    cout << strlen(buffer) << endl;
    return 0;
}