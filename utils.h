#include "rw_lock.h"

char *get_full_path(const char *short_path, void *userdata);

int get_access_mode(int flag);

bool file_already_open(void *userdata, char *full_path);

int lock(const char *path, rw_lock_mode_t mode);

int unlock(const char *path, rw_lock_mode_t mode);

int download_from_server_to_client(void *userdata, char *full_path, const char *path);

int upload_from_client_to_server(void *userdata, char *full_path, const char *path);

bool is_file_fresh(void *userdata, char *full_path, const char *path);

int update_TServer_to_TClient(void *userdata, char *full_path, const char *path);

int update_TClient_to_TServer(void *userdata, char *full_path, const char *path);

