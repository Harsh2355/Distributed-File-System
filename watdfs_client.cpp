//
// Starter code for CS 454/654
// You SHOULD change this file
//

#include "watdfs_client.h"
#include "rpc_calls.h"
#include "debug.h"
INIT_LOG
#include "rpc.h"
#include <string>
#include <map>
#include "global.h"
#include "utils.h"
#include <iostream>
using namespace std;


// SETUP AND TEARDOWN
void *watdfs_cli_init(struct fuse_conn_info *conn, const char *path_to_cache,
                      time_t cache_interval, int *ret_code) {
    // set up the RPC library by calling `rpcClientInit`.
    int rpcInitCode = rpcClientInit();

    // check the return code of the `rpcClientInit` call
    // `rpcClientInit` may fail, for example, if an incorrect port was exported.
    if (rpcInitCode != 0) {
        // rpc client init failed
        DLOG("Failed to initialize RPC Client ");
    }
    

    // Initialize any global state that you require for the assignment and return it.
    // The value that you return here will be passed as userdata in other functions.
    struct files_store *userdata = new struct files_store;

    //save `path_to_cache` and `cache_interval` (for A3).
    userdata->cache_interval = cache_interval;
    char *copied_path = new char[strlen(path_to_cache) + 1];
    strcpy(copied_path, path_to_cache);
    userdata->path_to_cache = copied_path;
    
    // set `ret_code` to 0 if everything above succeeded else some appropriate
    // non-zero value.
    *ret_code = rpcInitCode;

    // Return pointer to global state data.
    return (void *) userdata;
}

void watdfs_cli_destroy(void *userdata) {
    // TODO for P2: clean up your userdata state.

    struct files_store *store = (struct files_store *) userdata;
    delete store->path_to_cache;
    delete store;

    // tear down the RPC library by calling `rpcClientDestroy`.
    int rpcDestroyCode = rpcClientDestroy();

    if (rpcDestroyCode != 0) {
        // rpc client destory failed
        DLOG("Failed to Destroy RPC Client ");
    }
}


// GET FILE ATTRIBUTES
int watdfs_cli_getattr(void *userdata, const char *path, struct stat *statbuf) {

    int returnCode = 0;

    // get full path
    char *full_path = get_full_path(path, userdata);
    struct files_store *user = (struct files_store *) userdata;

    // check if file is already open
    if (!file_already_open(userdata, full_path)) {        
        // download file from server t0 client
        returnCode = download_from_server_to_client(userdata, full_path, path);
        
        if (returnCode < 0) {
            DLOG("Get Attr: Could not download from server to client");
            free(full_path);
            return returnCode;
        }
    }
    else {
        // check if open in read only mode
        if (get_access_mode(user->cur_open_files[full_path].flags) == O_RDONLY) {
            // freshness check
            bool is_fresh = is_file_fresh(userdata, full_path, path);

            // if not fresh, download fresh version
            if (!is_fresh) {
                DLOG("Get Attr: File is not fresh, fetching from server...");
                returnCode = download_from_server_to_client(userdata, full_path, path);
                if (returnCode < 0) {
                    DLOG("Get Attr: Could not fetch fresh data from the server");
                    free(full_path);
                    return returnCode;
                }

                // update tc to current time
                struct files_store *user = (struct files_store *) userdata;
                user->cur_open_files[full_path].tc = time(0);

                // updata T_client to be equal to T_server
                returnCode = update_TClient_to_TServer(userdata, full_path, path);
                if (returnCode < 0) {
                    DLOG("Read: Could not T_client to T_server");
                    free(full_path);
                    return returnCode;
                }
            }
        }

    }

    // retrieve file attr at client - stat
    returnCode = stat(full_path, statbuf);

    if (returnCode < 0) {
        DLOG("Get Attr: Could not retrieve file attributes at client");
        free(full_path);
        return -errno;
    }

    free(full_path);

    return 0;
}

// CREATE, OPEN AND CLOSE
int watdfs_cli_mknod(void *userdata, const char *path, mode_t mode, dev_t dev) {
    return rpc_mknod(userdata, path, mode, dev);
}


int watdfs_cli_open(void *userdata, const char *path,
                    struct fuse_file_info *fi) {
    
    // check if file is already open
    // return -EMFILE if it is
    char *full_path = get_full_path(path, userdata);
    if (file_already_open(userdata, full_path)) {
        DLOG("open error: file already open");
        free(full_path);
        return -EMFILE;
    }

    // transfer file from server to client
    int returnCode = 0;
    int fxn_ret = 0;

    // download file from server to client
    returnCode = download_from_server_to_client(userdata, full_path, path);
    if (returnCode < 0) {
        DLOG("Open Error: Download Failed");
        fxn_ret = returnCode;
    }

    if (fxn_ret < 0) {
        DLOG("Open Error: open failed (couldn't download file to client)");
        fxn_ret = -errno;
        free(full_path);
        return fxn_ret;
    }

    int actual_flags = fi->flags;

    // open the file at the client
    int fh = open(full_path, O_RDWR);
    std::cout << "Opened: " << fh << std::endl;
    if (fh < 0) {
        DLOG("Open Error: Could not open file on client");
        fxn_ret = -errno;
        free(full_path);
        return fxn_ret;
    }

    // open the file at the server
    returnCode = rpc_open(userdata, path, fi);
    if (returnCode < 0) {
        DLOG("Open Error: Could not open file on server");
        fxn_ret = returnCode;
        free(full_path);
        return fxn_ret;
    }

    DLOG("File successfully opened!");

    // update metadata
    std::cout << "Open File Handle: " << fh << std::endl;
    struct file_info opened_file = {fh, fi->fh, actual_flags, time(0)};
    struct files_store *user = (struct files_store *) userdata;
    user->cur_open_files[string(full_path)] = opened_file;
    fxn_ret = 0;
    free(full_path);
    
    return fxn_ret;
}

int watdfs_cli_release(void *userdata, const char *path,
                       struct fuse_file_info *fi) {

    int returnCode = 0;

   // get full path
   char *full_path = get_full_path(path, userdata);

   // if file opened in write mode
   struct files_store *user = (struct files_store *) userdata;
   if (get_access_mode(user->cur_open_files[full_path].flags) != O_RDONLY) {
        // upload from client to server
        returnCode = upload_from_client_to_server(userdata, full_path, path);

        if (returnCode < 0) {
            free(full_path);
            return returnCode;
        }

        // updata T_server to be equal to T_client
        returnCode = update_TServer_to_TClient(userdata, full_path, path);
        if (returnCode < 0) {
            DLOG("Release: Could not T_server to T_client");
            free(full_path);
            return returnCode;
        }
   }
    
    // close file at client
    int fh = user->cur_open_files[full_path].client_fi;
    returnCode = close(fh);

    if (returnCode < 0) {
        free(full_path);
        return -errno;
    }

    //release file at server
    returnCode = rpc_release(userdata, path, fi);

    if (returnCode < 0) {
        DLOG("Release: Could not release file at server");
        free(full_path);
        return returnCode;
    }

    // update cur_open_files
    auto it = user->cur_open_files.find(string(full_path));
    if (it != user->cur_open_files.end()) {
        user->cur_open_files.erase(it);
    }

    free(full_path);

    return 0;
}

// READ AND WRITE DATA
int watdfs_cli_read(void *userdata, const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {

    int returnCode = 0;

    // get full path
    char *full_path = get_full_path(path, userdata);

    // Freshness check
    bool is_fresh = is_file_fresh(userdata, full_path, path);

    // if not fresh, download fresh version
    if (!is_fresh) {
        DLOG("Read: File is not fresh, fetching from server...");
        returnCode = download_from_server_to_client(userdata, full_path, path);
        if (returnCode < 0) {
            DLOG("Read: Could not fetch fresh data from the server");
            free(full_path);
            return returnCode;
        }

        // update tc to current time
        struct files_store *user = (struct files_store *) userdata;
        user->cur_open_files[full_path].tc = time(0);

        // updata T_client to be equal to T_server
        returnCode = update_TClient_to_TServer(userdata, full_path, path);
        if (returnCode < 0) {
            DLOG("Read: Could not T_client to T_server");
            free(full_path);
            return returnCode;
        }
    }

    // make updates to tc and T_client T_server ?

    // read file from client
    struct files_store *user = (struct files_store *) userdata;
    int fh = user->cur_open_files[full_path].client_fi;
    std::cout << "Read File Handle: " << fh << std::endl;
    int bytes_read = pread(fh, buf, size, offset);
    DLOG("Read %d chars", bytes_read);

    if (bytes_read < 0) {
        DLOG("Read: Could not read at client...");
        free(full_path);
        return -errno;
    }

    DLOG("CHAR BUFFER");
    DLOG("%s", buf);

    free(full_path);

    return bytes_read;
}


int watdfs_cli_write(void *userdata, const char *path, const char *buf,
                     size_t size, off_t offset, struct fuse_file_info *fi) {           

    int returnCode = 0; 

    // Get full path
    char *full_path = get_full_path(path, userdata);

    // Do I need to check if file is not open?

    // write to client file
    struct files_store *user = (struct files_store *) userdata;
    int fh = user->cur_open_files[full_path].client_fi;
    int bytes_written = pwrite(fh, buf, size, offset);

    if (bytes_written < 0) {
        DLOG("Write: Could not write to local copy at client");
        free(full_path);
        return -errno;
    }

    // check freshness
    bool is_fresh = is_file_fresh(userdata, full_path, path);

    // if not fresh
    if (!is_fresh) {
         // push client file to server
        returnCode = upload_from_client_to_server(userdata, full_path, path);

        if (returnCode < 0) {
            free(full_path);
            return returnCode;
        }

        // update tc to current time
        user->cur_open_files[full_path].tc = time(0);

        // updata T_client to be equal to T_server
        returnCode = update_TServer_to_TClient(userdata, full_path, path);
        if (returnCode < 0) {
            DLOG("FSYNC: Could not T_server to T_client");
            free(full_path);
            return returnCode;
        }
    }

    free(full_path);

    return bytes_written;
}


int watdfs_cli_truncate(void *userdata, const char *path, off_t newsize) {
    
    int returnCode = 0;

    // get full path
    char *full_path = get_full_path(path, userdata);

    // if file not open
    if (!file_already_open(userdata, full_path)) {
        // transfer file from server
        returnCode = download_from_server_to_client(userdata, full_path, path);

        if (returnCode < 0) {
            free(full_path);
            return returnCode;
        }

        // truncate
        returnCode = truncate(full_path, newsize);

        if (returnCode < 0) {
            DLOG("Truncate failed...");
            free(full_path);
            return -errno;
        }

        // transfer file back to server
        returnCode = upload_from_client_to_server(userdata, full_path, path);

        if (returnCode < 0) {
            free(full_path);
            return returnCode;
        }
    } 
    else {
        struct files_store *user = (struct files_store *) userdata;
        // if flag is not read only
        if (get_access_mode(user->cur_open_files[full_path].flags) != O_RDONLY) {
            // truncate
            returnCode = truncate(full_path, newsize);

            if (returnCode < 0) {
                DLOG("Truncate failed...");
                free(full_path);
                return -errno;
            }

            // check freshness
            bool is_fresh = is_file_fresh(userdata, full_path, path);

            // if not fresh, push to server and update tc
            if (!is_fresh) {
                
                returnCode = upload_from_client_to_server(userdata, full_path, path);

                if (returnCode < 0) {
                    free(full_path);
                    return returnCode;
                }

                // update tc to current time
                struct files_store *user = (struct files_store *) userdata;
                user->cur_open_files[full_path].tc = time(0);

                // updata T_client to be equal to T_server
                returnCode = update_TServer_to_TClient(userdata, full_path, path);
                if (returnCode < 0) {
                    DLOG("Read: Could not T_server to T_client");
                    free(full_path);
                    return returnCode;
                }
            }
        }
        else {
            free(full_path);
            return -EMFILE;
        }
    }
        
    free(full_path);

    return 0;
}

int watdfs_cli_fsync(void *userdata, const char *path,
                     struct fuse_file_info *fi) {
                        
    int returnCode = 0;

    // get full path
    char *full_path = get_full_path(path, userdata);

    // file open and in read only mode -> return error
    struct files_store *user = (struct files_store *) userdata;
    if (file_already_open(userdata, full_path) && get_access_mode(user->cur_open_files[full_path].flags) == O_RDONLY) {
        DLOG("FSYNC: File is open in read only mode");
        free(full_path);
        return BAD_TYPES;
    }

    // push client file to server
    returnCode = upload_from_client_to_server(userdata, full_path, path);

    if (returnCode < 0) {
        free(full_path);
        return returnCode;
    }

    // updata T_client to be equal to T_server
    returnCode = update_TServer_to_TClient(userdata, full_path, path);
    if (returnCode < 0) {
        DLOG("FSYNC: Could not T_server to T_client");
        free(full_path);
        return returnCode;
    }

    free(full_path);

    return 0;
}

// CHANGE METADATA
int watdfs_cli_utimensat(void *userdata, const char *path,
                       const struct timespec ts[2]) {

    int returnCode = 0;

    // get full path
    char *full_path = get_full_path(path, userdata);

    // if file not open
    if (!file_already_open(userdata, full_path)) {
        // transfer file from server
        returnCode = download_from_server_to_client(userdata, full_path, path);

        if (returnCode < 0) {
            free(full_path);
            return returnCode;
        }

        // utimensat
        returnCode = utimensat(0, full_path, ts, 0);

        if (returnCode < 0) {
            DLOG("utimensat failed...");
            free(full_path);
            return -errno;
        }

        // transfer file back to server
        returnCode = upload_from_client_to_server(userdata, full_path, path);

        if (returnCode < 0) {
            free(full_path);
            return returnCode;
        }
    } 
    else {
        struct files_store *user = (struct files_store *) userdata;
        // if flag is not read only
        if (get_access_mode(user->cur_open_files[full_path].flags) != O_RDONLY) {
            // truncate
            returnCode = utimensat(0, full_path, ts, 0);

            if (returnCode < 0) {
                DLOG("open file utimensat failed...");
                free(full_path);
                return -errno;
            }

            // check freshness
            bool is_fresh = is_file_fresh(userdata, full_path, path);

            // if not fresh, push to server and update tc
            if (!is_fresh) {
                
                returnCode = upload_from_client_to_server(userdata, full_path, path);

                if (returnCode < 0) {
                    free(full_path);
                    return returnCode;
                }

                // update tc to current time
                struct files_store *user = (struct files_store *) userdata;
                user->cur_open_files[full_path].tc = time(0);

                // updata T_client to be equal to T_server
                returnCode = update_TServer_to_TClient(userdata, full_path, path);
                if (returnCode < 0) {
                    DLOG("Utimensat: Could not T_server to T_client");
                    free(full_path);
                    return returnCode;
                }
            }
        }
        else {
            free(full_path);
            return -EMFILE;
        }
    }
        
    free(full_path);

    return 0;
}
