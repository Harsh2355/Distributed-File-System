#include <map>
#include <string>
#include "rw_lock.h"
using namespace std;

// ------------------------------- GLOBAL DATA ---------------------------------------------

struct file_info {
    int client_fi;
    int server_fi;
    int flags;
    time_t tc;
};

struct files_store {
    map<string, struct file_info> cur_open_files;
    time_t cache_interval;
    const char *path_to_cache;
};

struct file_mutex {
    int mode;
    int num_times_opened = 0;
    rw_lock_t *lock;
};

class server_mutex {
    map<string, struct file_mutex> files;

    public:

    void add_file_entry(char * path);

    void remove_file_entry(char * path);

    bool is_file_open(char *path);

    int get_mode(char *path);

    void update_mode(char *path, int mode);

    rw_lock_t *get_lock(char *path);

    bool is_can_write(char *path);

    void change(char *path, bool dir);

    int get_count(char *path);

    ~server_mutex();
};

// -----------------------------------------------------------------------------------------
