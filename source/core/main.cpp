#include <iostream>
#include <cstring>
#include<string>
#include<sstream>
#include<memory>
#include "ofs_implementation.h"
#include "menu.h"
using namespace std;
int main222(){

    const char* path = "hello/hello/hello";
    std::string p(path);
    cout << p;
    const char* path1 = "hello/hello/hello";
    std::string p1(path1);
    cout << p;
    const char* path2 = "hello/hello/hello";
    std::string p2(path2);
    cout << p;
    const char* path3 = "hello/hello/hello";
    std::string p3(path3);
    cout << p;
    const char* path4 = "hello/hello/hello";
    std::string p4(path4);
    cout << p;
    return 0;
}
int main2(){
    void* fs = nullptr;

    int res = fs_format("/home/hanan/Projects/c++/file-verse/compiled/test.omni", "/home/hanan/Projects/c++/file-verse/compiled/default.uconf");
    if (res != static_cast<int>(OFSErrorCodes::SUCCESS)) {
        std::cout << "FS init failed: " << get_error_message(res) << "\n";
        return 1;
    }
    return 0;
}
int main(){
    InteractiveTester menu;
    menu.run();
    return 0;
}
int main22() {
    void* fs = nullptr;
    void* admin_session = nullptr;
    void* user_session = nullptr;
    FileEntry* entries = nullptr;
    int count = 0;
    FSStats stats;

    // ----------------------------
    // 1. Initialize File System
    // ----------------------------
    int res = fs_init(&fs,"/home/hanan/Projects/c++/file-verse/compiled/test.omni", "/home/hanan/Projects/c++/file-verse/compiled/default.uconf");
    if (res != static_cast<int>(OFSErrorCodes::SUCCESS)) {
        std::cout << "FS init failed: " << get_error_message(res) << "\n";
        return 1;
    }
    std::cout << "File system initialized successfully.\n";

    // ----------------------------
    // 2. Create Admin User
    // ----------------------------
    res = user_login(fs, "admin", "admin123", &admin_session);
    if (res != static_cast<int>(OFSErrorCodes::SUCCESS)) {
        std::cout << "Admin login failed: " << get_error_message(res) << "\n";
    } else {
        std::cout << "Admin logged in successfully.\n";
    }

    // ----------------------------
    // 3. Create a directory
    // ----------------------------
    res = dir_create(fs, admin_session, "/documents");
    std::cout << "Create /documents: " << get_error_message(res) << "\n";

    res = dir_create(fs, admin_session, "/documents/projects");
    std::cout << "Create /documents/projects: " << get_error_message(res) << "\n";

    // ----------------------------
    // 4. List directory
    // ----------------------------
    res = dir_list(fs, admin_session, "/documents", &entries, &count);
    if (res == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        std::cout << "/documents contains " << count << " entries:\n";
        for (int i = 0; i < count; ++i) {
            std::cout << "  - " << entries[i].name << "\n";
        }
    } else {
        std::cout << "List directory failed: " << get_error_message(res) << "\n";
    }

    // ----------------------------
    // 5. Create a file
    // ----------------------------

    std::string data("Hello, OMNIFS!", 15);
    std::string path("/documents.projects.test.txt", 29);
    res = file_create(fs, admin_session, "/documents.projects.test.txt", "Hello, OMNIFS!", 15);
    std::cout << "Create file /documents/projects/test.txt: " << get_error_message(res) << "\n";

    // ----------------------------
    // 6. Read file
    // ----------------------------
    char* buffer = nullptr;
    size_t size = 0;
    res = file_read(fs, admin_session, "/documents/projects/test.txt", &buffer, &size);
    if (res == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        std::cout << "Read /documents/projects/test.txt (" << size << " bytes): " 
                  << std::string(buffer, size) << "\n";
        free_buffer(buffer);
    } else {
        std::cout << "Read file failed: " << get_error_message(res) << "\n";
    }

    // ----------------------------
    // 7. Edit file
    // ----------------------------
    const char* new_data = "OMNIFS is awesome!";
    res = file_edit(fs, admin_session, "/documents/projects/test.txt", new_data, strlen(new_data), 0);
    std::cout << "Edit file: " << get_error_message(res) << "\n";

    // ----------------------------
    // 8. Truncate file
    // ----------------------------
    res = file_truncate(fs, admin_session, "/documents/projects/test.txt");
    std::cout << "Truncate file: " << get_error_message(res) << "\n";

    // ----------------------------
    // 9. Check if directory exists
    // ----------------------------
    res = dir_exists(fs, admin_session, "/documents/projects");
    std::cout << "Directory exists check: " << get_error_message(res) << "\n";

    // ----------------------------
    // 10. Get FS stats
    // ----------------------------
    res = get_stats(fs, admin_session, &stats);
    if (res == static_cast<int>(OFSErrorCodes::SUCCESS)) {
        std::cout << "FS Stats:\n";
        std::cout << "  Total size: " << stats.total_size << "\n";
        std::cout << "  Used space: " << stats.used_space << "\n";
        std::cout << "  Free space: " << stats.free_space << "\n";
        std::cout << "  Files: " << stats.total_files << "\n";
        std::cout << "  Directories: " << stats.total_directories << "\n";
        std::cout << "  Fragmentation: " << stats.fragmentation << "%\n";
    }

    // ----------------------------
    // 11. Delete file
    // ----------------------------
    res = file_delete(fs, admin_session, "/documents/projects/test.txt");
    std::cout << "Delete file: " << get_error_message(res) << "\n";

    // ----------------------------
    // 12. Delete directory
    // ----------------------------
    res = dir_delete(fs, admin_session, "/documents/projects");
    std::cout << "Delete directory: " << get_error_message(res) << "\n";

    // ----------------------------
    // 13. Logout and shutdown
    // ----------------------------
    user_logout(admin_session);
    fs_shutdown(fs);
    std::cout << "FS shutdown successfully.\n";

    return 0;
}
