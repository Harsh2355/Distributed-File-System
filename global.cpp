#include "global.h"
#include "rw_lock.h"
#include <iostream>
#include <fcntl.h>
#include "debug.h"
using namespace std;

// Create a new entry file entry
void server_mutex::add_file_entry(char *path) {
    std::cout << "Adding file entry: " << path << std::endl;
    struct file_mutex new_mutex;
    new_mutex.lock = new rw_lock_t;
    rw_lock_init(new_mutex.lock);
    files[string(path)] = new_mutex;
}

// Remove file entry on release and release memory
void server_mutex::remove_file_entry(char *path) {
    auto it = files.find(string(path));
    if (it != files.end()) {
        delete it->second.lock;
        files.erase(it);
    }
}

// Check to see if file exists in map
bool server_mutex::is_file_open(char *path) {
    auto it = files.find(string(path));
    std::cout << "IS FILE OPEN: " << it->first << std::endl;

    return files.find(string(path)) != files.end();
}

// Implement the get_mode function
int server_mutex::get_mode(char *path) {
    auto it = files.find(std::string(path));
    if (it != files.end()) {
        return it->second.mode;
    }
    return -1; // Not found
}

// Implement the update_mode function
void server_mutex::update_mode(char *path, int mode) {
    auto it = files.find(std::string(path));
    if (it != files.end()) {
        it->second.mode = mode;
    }
}

// Implement the get_lock function
rw_lock_t *server_mutex::get_lock(char *path) {
    auto it = files.find(std::string(path));
    if (it != files.end()) {
        return it->second.lock;
    }
    return nullptr; // Not found
}

// Implement the is_can_write function
bool server_mutex::is_can_write(char *path) {
    auto it = files.find(std::string(path));
    if (it != files.end()) {
        // Check if the mode allows write access
        std::cout << "Flag 1: " << (it->second.mode & O_ACCMODE)<< std::endl;
        return (it->second.mode & O_ACCMODE) == O_WRONLY || (it->second.mode & O_ACCMODE) == O_RDWR; 
    }
    return false; // Not found
}

void server_mutex::change(char *path, bool dir) {
    auto it = files.find(std::string(path));
    if (it != files.end()) {
        if (dir) {
            it->second.num_times_opened += 1;
            return;
        }
        it->second.num_times_opened -= 1;
    }
}

int server_mutex::get_count(char *path) {
    auto it = files.find(std::string(path));
    if (it != files.end()) {
        return it->second.num_times_opened;
    }
    return 0; // Not found
}

// Destructor
server_mutex::~server_mutex() {
    // Iterate over each entry in the map
    for (auto& entry : files) {
        delete entry.second.lock;
    }
    // Clear the map
    files.clear();
}