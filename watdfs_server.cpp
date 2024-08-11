//
// Starter code for CS 454/654
// You SHOULD change this file
//

#include "rpc.h"
#include "debug.h"
#include "global.h"
INIT_LOG

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <fuse.h>
#include <iostream>

// Global state server_persist_dir.
char *server_persist_dir = nullptr;

// Open Files Data
server_mutex *open_files = nullptr;

// Important: the server needs to handle multiple concurrent client requests.
// You have to be careful in handling global variables, especially for updating them.
// Hint: use locks before you update any global variable.

// We need to operate on the path relative to the server_persist_dir.
// This function returns a path that appends the given short path to the
// server_persist_dir. The character array is allocated on the heap, therefore
// it should be freed after use.
// Tip: update this function to return a unique_ptr for automatic memory management.
char *get_full_path(char *short_path) {
    int short_path_len = strlen(short_path);
    int dir_len = strlen(server_persist_dir);
    int full_len = dir_len + short_path_len + 1;

    char *full_path = (char *)malloc(full_len);

    // First fill in the directory.
    strcpy(full_path, server_persist_dir);
    // Then append the path.
    strcat(full_path, short_path);
    DLOG("Full path: %s\n", full_path);

    return full_path;
}

// The server implementation of getattr.
int watdfs_getattr(int *argTypes, void **args) {
    // Get the arguments.
    // The first argument is the path relative to the mountpoint.
    char *short_path = (char *)args[0];
    // The second argument is the stat structure, which should be filled in
    // by this function.
    struct stat *statbuf = (struct stat *)args[1];
    // The third argument is the return code, which should be set be 0 or -errno.
    int *ret = (int *)args[2];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    // Initially we set the return code to be 0.
    *ret = 0;

    // Make the stat system call, which is the corresponding system call needed
    // to support getattr. You should use the statbuf as an argument to the stat system call.
    // Let sys_ret be the return code from the stat system call.
    int sys_ret = 0;
    sys_ret = stat(full_path, statbuf);

    
    if (sys_ret < 0) {
        // If there is an error on the system call, then the return code should
        // be -errno.
        *ret = -errno;
    }

    // Clean up the full path, it was allocated on the heap.
    free(full_path);

    DLOG("Returning code for getattr: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}


// The server implementation of getattr.
int watdfs_mknod(int *argTypes, void **args) {

    char *short_path = (char *)args[0];

    mode_t *mode = (mode_t *) args[1];

    dev_t *dev = (dev_t *) args[2];

    int *ret = (int *) args[3];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    // Initially we set the return code to be 0.
    *ret = 0;

    // make syscall to mknode
    int sys_ret = 0;
    sys_ret = mknod(full_path, *mode, *dev);

    DLOG("MKNODE sys_ret: %d", sys_ret);
    if (sys_ret < 0) {
        *ret = -errno;
    }

    // Clean up the full path, it was allocated on the heap.
    free(full_path);

    DLOG("Returning code for mknode: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}


int watdfs_ultimensat(int *argTypes, void **args) {

    // Get the arguments.
    // The first argument is the path relative to the mountpoint.
    char *short_path = (char *)args[0];

    // The second argument is timespec
    struct timespec *ts = (struct timespec *)args[1];

    // The third argument is return code
    int *ret = (int*)args[2];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    // Initially we set the return code to be 0.
    *ret = 0;

    // Let sys_ret be the return code from the stat system call.
    int sys_ret = 0;

    sys_ret = utimensat(0, full_path, ts, 0);

    if (sys_ret < 0) {
      *ret = -errno;
      DLOG("sys call: utimens failed");
    }

    // Clean up the full path, it was allocated on the heap.
    free(full_path);
    DLOG("Returning code for utimens: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}


int watdfs_open(int *argTypes, void **args) {

    char *short_path = (char *) args[0];

    struct fuse_file_info *fi = (struct fuse_file_info *) args[1];

    int *ret = (int *) args[2];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    // Initially we set the return code to be 0.
    *ret = 0;

    std::cout << "Open Called: " << (fi->flags & O_ACCMODE) << std::endl;
    // add/update file metadata
    if (!open_files->is_file_open(full_path)) {
        // add an entry
        open_files->add_file_entry(full_path);
        open_files->update_mode(full_path, fi->flags);
    }
    else {
        std::cout << "FLAG 2: " << (fi->flags & O_ACCMODE) << std::endl;
        if (open_files->is_can_write(full_path)) {
            if ((fi->flags & O_ACCMODE) == O_RDWR || (fi->flags & O_ACCMODE) == O_WRONLY) {
                 // file is open in write mode, so request for write access is denied
                DLOG("OPEN: Cannot allow concurrent writes");
                return -EACCES;
            }
            else {
                // Multiple readers can open a file in read only mode, do nothing
            }
        }
        else {
            DLOG("OPEN: new file mode -> %d", fi->flags);
            open_files->update_mode(full_path, fi->flags);
        }
    }

    int sys_ret = 0;
    sys_ret = open(full_path, O_RDWR);

    DLOG("OPEN sys_ret: %d", sys_ret);
    if (sys_ret < 0) {
        *ret = -errno;
    }
    else {
        fi->fh = sys_ret;
        open_files->change(full_path, true);
    }

    // Clean up the full path, it was allocated on the heap.
    free(full_path);

    DLOG("Returning code for open: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}

int watdfs_release(int *argTypes, void **args) {
    
    char *short_path = (char *) args[0];

    struct fuse_file_info *fi = (struct fuse_file_info *) args[1];

    int *ret = (int *) args[2];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    // Initially we set the return code to be 0.
    *ret = 0;

    int sys_ret = 0;
    sys_ret = close(fi->fh);

    open_files->change(full_path, false);

    // remove file from opened_files tracker
    int count = open_files->get_count(full_path);
    std::cout << "Release Called: " << count << std::endl;
    if (count == 0) {
        open_files->remove_file_entry(full_path);
    }

    DLOG("RELEASE sys_ret: %d", sys_ret);
    if (sys_ret < 0) {
        *ret = -errno;
    }

     // Clean up the full path, it was allocated on the heap.
    free(full_path);

    DLOG("Returning code for release: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}

int watdfs_read(int *argTypes, void **args) {

    char *short_path = (char *) args[0];

    void *buf = args[1];

    size_t *size = (size_t *) args[2];

    off_t *offset = (off_t *) args[3];

    struct fuse_file_info *fi = (struct fuse_file_info *) args[4];

    int *ret = (int *) args[5];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = 0;
    sys_ret = pread(fi->fh, buf, *size, *offset);

    if (sys_ret < 0) {
        *ret = -errno;
    }
    else {
        *ret = sys_ret;
    }

     // Clean up the full path, it was allocated on the heap.
    free(full_path);

    DLOG("Returning code for read: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}

int watdfs_write(int *argTypes, void **args) {

    char *short_path = (char *) args[0];

    void *buf = args[1];

    size_t *size = (size_t *) args[2];

    off_t *offset = (off_t *) args[3];

    struct fuse_file_info *fi = (struct fuse_file_info *) args[4];

    int *ret = (int *) args[5];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = 0;
    sys_ret = pwrite(fi->fh, buf, *size, *offset);

    if (sys_ret < 0) {
        *ret = -errno;
    }
    else {
        *ret = sys_ret;
    }

     // Clean up the full path, it was allocated on the heap.
    free(full_path);

    DLOG("Returning code for write: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;        

}

int watdfs_truncate(int *argTypes, void **args) {

    char *short_path = (char *) args[0];

    off_t *newsize = (off_t *) args[1];

    int *ret = (int *) args[2];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = 0;
    sys_ret = truncate(full_path, *newsize);
    
    if (sys_ret < 0) {
        *ret = -errno;
    }

     // Clean up the full path, it was allocated on the heap.
    free(full_path);

    DLOG("Returning code for truncate: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}

int watdfs_fsync(int *argTypes, void **args) {

    char *short_path = (char *) args[0];

    struct fuse_file_info *fi = (struct fuse_file_info *) args[1];

    int *ret = (int *) args[2];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    *ret = 0;

    int sys_ret = 0;
    sys_ret = fsync(fi->fh);

    if (sys_ret < 0) {
        *ret = -errno;
    }

     // Clean up the full path, it was allocated on the heap.
    free(full_path);

    DLOG("Returning code for fsync: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}

int watdfs_lock(int *argTypes, void **args) {

    char *short_path = (char *) args[0];

    rw_lock_mode_t *mode = (rw_lock_mode_t *) args[1];

    int *ret = (int *) args[2];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    int sys_ret = 0;
    sys_ret = rw_lock_lock(open_files->get_lock(full_path), *mode);

    *ret = sys_ret;

     // Clean up the full path, it was allocated on the heap.
    free(full_path);

    DLOG("Returning code for lock: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}

int watdfs_unlock(int *argTypes, void **args) {

    char *short_path = (char *) args[0];

    rw_lock_mode_t *mode = (rw_lock_mode_t *) args[1];

    int *ret = (int *) args[2];

    // Get the local file name, so we call our helper function which appends
    // the server_persist_dir to the given path.
    char *full_path = get_full_path(short_path);

    int sys_ret = 0;
    sys_ret = rw_lock_unlock(open_files->get_lock(full_path), *mode);

    *ret = sys_ret;

     // Clean up the full path, it was allocated on the heap.
    free(full_path);

    DLOG("Returning code for unlock: %d", *ret);
    // The RPC call succeeded, so return 0.
    return 0;
}

// The main function of the server.
int main(int argc, char *argv[]) {
    // argv[1] should contain the directory where you should store data on the
    // server. If it is not present it is an error, that we cannot recover from.
    if (argc != 2) {
        // In general, you shouldn't print to stderr or stdout, but it may be
        // helpful here for debugging. Important: Make sure you turn off logging
        // prior to submission!
        // See watdfs_client.cpp for more details
        // # ifdef PRINT_ERR
        // std::cerr << "Usage:" << argv[0] << " server_persist_dir";
        // #endif
        return -1;
    }
    // Store the directory in a global variable.
    server_persist_dir = argv[1];
    
    // Init open files store
    open_files = new server_mutex;

    // TODO: Initialize the rpc library by calling `rpcServerInit`.
    // Important: `rpcServerInit` prints the 'export SERVER_ADDRESS' and
    // 'export SERVER_PORT' lines. Make sure you *do not* print anything
    // to *stdout* before calling `rpcServerInit`.
    //DLOG("Initializing server...");
    int rpcInitCode = rpcServerInit();
    DLOG("Initializing server...");

    int ret = 0;
    // If there is an error with `rpcServerInit`, it maybe useful to have
    // debug-printing here, and then you should return.
    if (rpcInitCode != 0) {
        // rpc server init failed
        DLOG("Failed to initialize RPC Server ");
        return rpcInitCode;
    }

    // TODO: Register your functions with the RPC library.
    // Note: The braces are used to limit the scope of `argTypes`, so that you can
    // reuse the variable for multiple registrations. Another way could be to
    // remove the braces and use `argTypes0`, `argTypes1`, etc.
    {
        // There are 3 args for the function (see watdfs_client.cpp for more
        // detail).
        int argTypes[4];
        // First is the path.
        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        // The second argument is the statbuf.
        argTypes[1] =
            (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        // The third argument is the retcode.
        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        // Finally we fill in the null terminator.
        argTypes[3] = 0;

        // We need to register the function with the types and the name.
        ret = rpcRegister((char *)"getattr", argTypes, watdfs_getattr);
        if (ret < 0) {
            DLOG("get attr failed");
            return ret;
        }
        DLOG("get attr succeeded");
    }

    // for mknode
    {
        int argTypes[5];

        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = (1u << ARG_INPUT) | (ARG_INT << 16u);

        argTypes[2] =  (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[3] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[4] = 0;

        ret = rpcRegister((char *)"mknod", argTypes, watdfs_mknod);
        if (ret < 0) {
            DLOG("mknod failed");
            return ret;
        }
        DLOG("mknod succeeded");
    }

    // for ultimensat
    {

        int argTypes[4];

        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        ret = rpcRegister((char *) "ultimensat", argTypes, watdfs_ultimensat);

        if (ret < 0) {
            DLOG("ultimensat failed");
	        return ret;
        }
        DLOG("ultimensat succeeded");
    }

    // for open
    {
        int argTypes[4];

        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = (1u << ARG_INPUT) | (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        ret = rpcRegister((char *)"open", argTypes, watdfs_open);
        if (ret < 0) {
            DLOG("open failed");
            return ret;
        }
        DLOG("open succeeded");
    }

    // for release 
    {
        int argTypes[4];

        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
            
        argTypes[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        // We need to register the function with the types and the name.
        ret = rpcRegister((char *) "release", argTypes, watdfs_release);
        if (ret < 0) {
            DLOG("release failed");
            return ret;
        }
        DLOG("release succeeded");
    }

    // for read 
    {
        int argTypes[7];

        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[4] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;
        
        argTypes[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[6] = 0;

        // We need to register the function with the types and the name.
        ret = rpcRegister((char *) "read", argTypes, watdfs_read);
        if (ret < 0) {
            DLOG("read failed");
            return ret;
        }
        DLOG("read succeeded");
    }

    {
        int argTypes[7];

        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[4] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[6] = 0;

        // We need to register the function with the types and the name.
        ret = rpcRegister((char *) "write", argTypes, watdfs_write);
        if (ret < 0) {
            DLOG("write failed");
            return ret;
        }
        DLOG("write succeeded");
    }

    // for truncate
    {
        int argTypes[4];

        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = (1u << ARG_INPUT) | (ARG_LONG << 16u);

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        // We need to register the function with the types and the name.
        ret = rpcRegister((char *) "truncate", argTypes, watdfs_truncate);
        if (ret < 0) {
            DLOG("truncate failed");
            return ret;
        }
        DLOG("truncate succeeded");
    }

    // for fysnc 
    {
        int argTypes[4];

        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        // We need to register the function with the types and the name.
        ret = rpcRegister((char *) "fsync", argTypes, watdfs_fsync);
         if (ret < 0) {
            DLOG("fsync failed");
            return ret;
        }
        DLOG("fsync succeeded");
    }

    // for lock
    {
        int argTypes[4];

        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = (1u << ARG_INPUT) |  (ARG_INT << 16u);

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        // We need to register the function with the types and the name.
        ret = rpcRegister((char *) "lock", argTypes, watdfs_lock);
        if (ret < 0) {
            DLOG("lock failed");
            return ret;
        }
        DLOG("lock succeeded");
    }

    // for unlock
    {
        int argTypes[4];

        argTypes[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | 1u;

        argTypes[1] = (1u << ARG_INPUT) |  (ARG_INT << 16u);

        argTypes[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

        argTypes[3] = 0;

        // We need to register the function with the types and the name.
        ret = rpcRegister((char *) "unlock", argTypes, watdfs_unlock);
        if (ret < 0) {
            DLOG("unlock failed");
            return ret;
        }
        DLOG("unlock succeeded");
    }

    // Hand over control to the RPC library by calling `rpcExecute`.
    int executionStatusCode = rpcExecute();

    // rpcExecute could fail, so you may want to have debug-printing here, and
    // then you should return.
    if (executionStatusCode != 0) {
        DLOG("Failed to execute command rpcExecute() ");
        return executionStatusCode;
    }

    return ret;
}
