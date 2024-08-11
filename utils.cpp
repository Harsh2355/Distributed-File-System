#include "utils.h"
#include "global.h"
#include <string>
#include <string.h>
#include "debug.h"
#include "rpc_calls.h"
#include "rpc.h"
#include <fcntl.h>
#include <iostream>
using namespace std;

char *get_full_path(const char *short_path, void *userdata) {
    struct files_store *user = (struct files_store *) userdata;
    int short_path_len = strlen(short_path);
    int dir_len = strlen(user->path_to_cache);
    int full_len = dir_len + short_path_len + 1;

    char *full_path = (char *) malloc(full_len);

    // First fill in the directory.
    strcpy(full_path, user->path_to_cache);
    // Then append the path.
    strcat(full_path, short_path);
    DLOG("Full path: %s\n", full_path);

    return full_path;
}

int get_access_mode(int flag) {
    return flag & O_ACCMODE;
}

bool file_already_open(void *userdata, char *full_path) {
    struct files_store *user = (struct files_store *) userdata;
    // search for file data in userdata
    auto it = user->cur_open_files.find(string(full_path));
    return it != user->cur_open_files.end();
}

int lock(const char *path, rw_lock_mode_t mode) {

    // SET UP THE RPC CALL
    DLOG("lock called for '%s'", path);
    
    int ARG_COUNT = 3;

    void **args = new void*[ARG_COUNT];

    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    // Fill in the arguments
    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *) path;

    arg_types[1] = (1u << ARG_INPUT) |  (ARG_INT << 16u);
    args[1] = (void *) &mode;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int returnCode = 0;
    args[2] = (void *) &returnCode;

    arg_types[3] = 0;

    // MAKE THE RPC CALL
    int rpc_ret = rpcCall((char *)"lock", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("lock rpc failed with error '%d'", rpc_ret);
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = returnCode;
    }

    // Clean up the memory we have allocated.
    delete []args;

    // Finally return the value we got from the server.
    return fxn_ret;
}

int unlock(const char *path, rw_lock_mode_t mode) {
    // SET UP THE RPC CALL
    DLOG("unlock called for '%s'", path);
    
    int ARG_COUNT = 3;

    void **args = new void*[ARG_COUNT];

    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    // Fill in the arguments
    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *) path;

    arg_types[1] = (1u << ARG_INPUT) |  (ARG_INT << 16u);
    args[1] = (void *) &mode;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int returnCode = 0;
    args[2] = (void *) &returnCode;

    arg_types[3] = 0;

    // MAKE THE RPC CALL
    int rpc_ret = rpcCall((char *)"unlock", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("lock rpc failed with error '%d'", rpc_ret);
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = returnCode;
    }

    // Clean up the memory we have allocated.
    delete []args;

    // Finally return the value we got from the server.
    return fxn_ret;
}

int download_from_server_to_client(void *userdata, char *full_path, const char *path) {
    DLOG("Downloading from server to client");

    int fxn_ret = 0;
    int returnCode = 0;
    int fd = 0;

    // lock file - read mode
    lock(path, RW_READ_LOCK);

    // get attr of file
    struct stat *statbuf = new struct stat;
    returnCode = rpc_getattr(userdata, path, statbuf);

    if (returnCode < 0) {
        DLOG("Download: File does not exist at the server");
        return returnCode;
    }

    struct fuse_file_info *fi = new struct fuse_file_info;
    fi->flags = O_RDONLY;
    struct files_store *user = (struct files_store *) userdata;
    if (!file_already_open(userdata, full_path)) {
        // open file
        fd = open(full_path, O_RDWR);
        std::cout << "Opened: " << fd << std::endl;

        // if file doesn't exists create one
        if (fd < 0) {
            DLOG("Download: File does not exist. Creating one...");
            mknod(full_path, statbuf->st_mode, statbuf->st_dev);
            fd = open(full_path, O_RDWR);
            std::cout << "Opened: " << fd << std::endl;
        }

        // 1. Open file in the server
        returnCode = rpc_open(userdata, path, fi);

        if (returnCode < 0) {
            DLOG("Download: Could not open file at server. Exiting...");
            delete fi;
            delete statbuf;
            unlock(path, RW_READ_LOCK);
            return returnCode;
        }
    }
    else {
        fd = user->cur_open_files[full_path].client_fi;
        fi->fh = user->cur_open_files[full_path].server_fi;
    }

    // read file from server
    // 2. Read file from server
    size_t size = statbuf->st_size;
    char *buf = (char *) malloc(((off_t) size) * sizeof(char));
    returnCode = rpc_read(userdata, path, buf, size, 0, fi);

    if (returnCode < 0) {
        DLOG("Download: Could not read file from server");
        free(buf);
        delete statbuf;
        delete fi;
        unlock(path, RW_READ_LOCK);
        return returnCode;
    }

    // write the file contents to client
    int write_response = 0;
    write_response = pwrite(fd, buf, size, 0);
    DLOG("Written characters: %d", write_response);

    if (write_response < 0) {
        DLOG("Download: Could not write file contents to client");
        free(buf);
        delete statbuf;
        delete fi;
        unlock(path, RW_READ_LOCK);
        return -errno;
    }

    // update file metadata at client
    struct timespec ts[2];
    ts[0] = (struct timespec) (statbuf->st_mtim);
    ts[1] = (struct timespec) (statbuf->st_mtim);

    returnCode = utimensat(0, full_path, ts, 0);
    
    if (returnCode < 0) {
        DLOG("Download: Could not update file metadata at client");
        free(buf);
        delete statbuf;
        delete fi;
        unlock(path, RW_READ_LOCK);
        return -errno;
    }

    if (!file_already_open(userdata, full_path)) {
        // release file
        returnCode = rpc_release(userdata, path, fi);

        if (returnCode < 0) {
            DLOG("Download: Could not release file at server");
            fxn_ret = returnCode;
        }

        // close file locally
        returnCode = close(fd);
        if (returnCode < 0) {
            DLOG("Download: Could not close file at client");
            free(buf);
            delete statbuf;
            delete fi;
            unlock(path, RW_READ_LOCK);
            return -errno;
        }
    }

    free(buf);
    delete statbuf;
    delete fi;

    unlock(path, RW_READ_LOCK);

    return fxn_ret;
}

int upload_from_client_to_server(void *userdata, char *full_path, const char *path) {

    DLOG("Uploading from client to server");

    int fxn_ret = 0;
    int returnCode = 0;

    // aquire lock - write mode
    lock(path, RW_WRITE_LOCK);

    // get attr of file at client
    struct stat *statbuf = new struct stat;
    returnCode = stat(full_path, statbuf);

    if (returnCode < 0) {
        DLOG("Upload: Could not get file metadata at client");
        delete statbuf;
        unlock(path, RW_WRITE_LOCK);
        return -errno;
    }

    int fh = 0;
    struct fuse_file_info *fi = new struct fuse_file_info;
    struct files_store *user = (struct files_store *) userdata;
    if (!file_already_open(userdata, full_path)) {
        // open file at server
        fi->flags = O_RDWR;
        returnCode = rpc_open(userdata, path, fi);

        // if can't open, create a file and then open
        if (returnCode < 0) {
            DLOG("Upload: File does not exist server. Creating one...");
            mode_t mode = statbuf->st_mode;
            dev_t dev = statbuf->st_dev;
            returnCode = rpc_mknod(userdata, path, mode, dev);
            if (returnCode < 0) {
                DLOG("Upload: Could not create file at server");
                delete statbuf;
                delete fi;
                unlock(path, RW_WRITE_LOCK);
                return returnCode;
            }
            returnCode = rpc_open(userdata, path, fi);
            if (returnCode < 0) {
                DLOG("Upload: Could not open file at server");
                delete statbuf;
                delete fi;
                unlock(path, RW_WRITE_LOCK);
                return returnCode;
            }
        }

        // open file at client
        fh = open(full_path, O_RDONLY);
        std::cout << "Opened: " << fh << std::endl;

        if (fh < 0) {
            DLOG("Upload: Could not open file at client");
            delete statbuf;
            delete fi;
            unlock(path, RW_WRITE_LOCK);
            return -errno;
        }

    }
    else {
        // file is already open
        fh = user->cur_open_files[full_path].client_fi;
        returnCode = user->cur_open_files[full_path].server_fi;
    }
    

    // read file at client
    std::cout << "File Handle Here: " << fh << std::endl;
    size_t size = statbuf->st_size;
    char *buf = (char *) malloc(((off_t) size) * sizeof(char));
    returnCode = pread(fh, buf, size, 0);

    if (returnCode < 0) {
        DLOG("Upload: Could not read file at client");
        free(buf);
        delete statbuf;
        delete fi;
        unlock(path, RW_WRITE_LOCK);
        return -errno;
    }

    // truncate file at server
    returnCode = rpc_truncate(userdata, path, 0);
    if (returnCode < 0) {
        DLOG("Upload: Could not truncate file at server");
        free(buf);
        delete statbuf;
        delete fi;
        unlock(path, RW_WRITE_LOCK);
        return returnCode;
    }

    // write file to server
    fi->fh = user->cur_open_files[full_path].server_fi;
    returnCode = rpc_write(userdata, path, buf, (off_t) size, 0, fi);
    if (returnCode < 0) {
        DLOG("Upload: Could not write to file at server");
        free(buf);
        delete statbuf;
        delete fi;
        unlock(path, RW_WRITE_LOCK);
        return returnCode;
    }

    // update metadata
    struct timespec ts[2];
    ts[0] = (struct timespec) statbuf->st_mtim;
    ts[1] = (struct timespec) statbuf->st_mtim;
    returnCode = rpc_utimensat(userdata, path, ts);

    if (returnCode < 0) {
        DLOG("Upload: Could not write to update timestamp at server");
        free(buf);
        delete statbuf;
        delete fi;
        unlock(path, RW_WRITE_LOCK);
        return returnCode;
    }


    if (!file_already_open(userdata, full_path)) {
        // close file local
        returnCode = close(fh);

        if (returnCode < 0) {
            DLOG("Upload: Could not close file at client");
            free(buf);
            delete statbuf;
            delete fi;
            unlock(path, RW_WRITE_LOCK);
            return -errno;
        }

        // release file server
        returnCode = rpc_release(userdata, path, fi);

        if (returnCode < 0) {
            DLOG("Upload: Could not release file at server");
            free(buf);
            delete statbuf;
            delete fi;
            unlock(path, RW_WRITE_LOCK);
            return returnCode;
        }
    }
    
    free(buf);
    delete statbuf;
    delete fi;
    // release lock
    unlock(path, RW_WRITE_LOCK);

    return fxn_ret;
}

bool is_file_fresh(void *userdata, char *full_path, const char *path) {
    DLOG("Checking freshness of file at client");

    int returnCode = 0;

    // retrieve file meta data
    struct files_store *user = (struct files_store *) userdata;
    time_t t = user->cache_interval;
    struct file_info client_file = user->cur_open_files[full_path];

     // check if time since last cache validation is within cache interval
    time_t tc = client_file.tc;
    time_t current_time = time(0);
    if ((current_time - tc) < t) {
        DLOG("Fressness: Within the specified time period");
        return true;
    }

    // get files attributes at client
    struct stat *statbuf_client = new struct stat;
    returnCode = stat(full_path, statbuf_client);

    if (returnCode < 0) {
        DLOG("Freshness: Could not retrieve file attr at client");
        delete statbuf_client;
        return false;
    }

    // fetch file attributes from server
    struct stat *statbuf_server = new struct stat;
    returnCode = rpc_getattr(userdata, path, statbuf_server);

    if (returnCode < 0) {
        DLOG("Freshness: Could not retrieve file attr from server");
        delete statbuf_client;
        delete statbuf_server;
        return returnCode;
    }

    // check if client time and server time are equal
    time_t T_client = statbuf_client->st_mtime;
    time_t T_server = statbuf_server->st_mtime;
    if (T_client == T_server) {
        // update tc to current time
        user->cur_open_files[full_path].tc = current_time;
        delete statbuf_client;
        delete statbuf_server;
        return true;
    }

    // both conditions failed
    delete statbuf_client;
    delete statbuf_server;
    return false;
}

int update_TServer_to_TClient(void *userdata, char *full_path, const char *path) {
    DLOG("Updating T_server to be equal to T_client");

    int returnCode = 0;

    // get files attributes at client
    struct stat *statbuf_client = new struct stat;
    returnCode = stat(full_path, statbuf_client);

    if (returnCode < 0) {
        DLOG("Update Server Time: Could not retrieve file attr at client");
        delete statbuf_client;
        return -errno;
    }

    // updata T_server to be equal to T_Client
    struct timespec ts[2];
    ts[0] = (struct timespec) statbuf_client->st_mtim;
    ts[1] = (struct timespec) statbuf_client->st_mtim;
    returnCode = rpc_utimensat(userdata, path, ts);

    if (returnCode < 0) {
        DLOG("Update Server Time: Could not set file attr at server");
        delete statbuf_client;
        return returnCode;
    }

    delete statbuf_client;

    return 0;

}

int update_TClient_to_TServer(void *userdata, char *full_path, const char *path) {
    DLOG("Updating T_server to be equal to T_client");

    int returnCode = 0;

    // get files attributes at server
    struct stat *statbuf_server = new struct stat;
    returnCode = rpc_getattr(userdata, path, statbuf_server);

    if (returnCode < 0) {
        DLOG("Update Client Time: Could not retrieve file attr at server");
        delete statbuf_server;
        return returnCode;
    }

    // update client time
    struct timespec ts[2];
    ts[0] = (struct timespec) statbuf_server->st_mtim;
    ts[1] = (struct timespec) statbuf_server->st_mtim;
    returnCode = utimensat(0, full_path, ts, 0);

    if (returnCode < 0) {
        DLOG("Update Client Time: Could not set file attr at Client");
        delete statbuf_server;
        return -errno;
    }

    delete statbuf_server;

    return 0;
}