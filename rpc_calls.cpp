#include "rpc_calls.h"
#include "debug.h"
#include "rpc.h"
using namespace std;

// GET FILE ATTRIBUTES
int rpc_getattr(void *userdata, const char *path, struct stat *statbuf) {
    // SET UP THE RPC CALL
    DLOG("rpc_getattr called for '%s'", path);
    
    // getattr has 3 arguments.
    int ARG_COUNT = 3;

    // Allocate space for the output arguments.
    void **args = new void*[ARG_COUNT];

    // Allocate the space for arg types, and one extra space for the null
    // array element.
    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    // Fill in the arguments
    // The first argument is the path, it is an input only argument, and a char
    // array. The length of the array is the length of the path.
    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    // For arrays the argument is the array pointer, not a pointer to a pointer.
    args[0] = (void *)path;

    // The second argument is the stat structure. This argument is an output
    // only argument, and we treat it as a char array. The length of the array
    // is the size of the stat structure, which we can determine with sizeof.
    arg_types[1] = (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) |
                   (uint) sizeof(struct stat); // statbuf
    args[1] = (void *)statbuf;

    // The third argument is the return code, an output only argument, which is
    // an integer.
    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);

    // The return code is not an array, so we need to hand args[2] an int*.
    // The int* could be the address of an integer located on the stack, or use
    // a heap allocated integer, in which case it should be freed.
    int returnCode = 0;
    args[2] = (void *)&returnCode;

    // Finally, the last position of the arg types is 0. There is no
    // corresponding arg.
    arg_types[3] = 0;

    // MAKE THE RPC CALL
    int rpc_ret = rpcCall((char *)"getattr", arg_types, args);

    // HANDLE THE RETURN
    // The integer value rpc_getattr will return.
    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("getattr rpc failed with error '%d'", rpc_ret);
        // Something went wrong with the rpcCall, return a sensible return
        // value. In this case lets return, -EINVAL
        fxn_ret = -EINVAL;
    } else {
        // Our RPC call succeeded. However, it's possible that the return code
        // from the server is not 0, that is it may be -errno. Therefore, we
        // should set our function return value to the retcode from the server.

        // set the function return value to the return code from the server.
        fxn_ret = returnCode;
    }

    if (fxn_ret < 0) {
        // If the return code of rpc_getattr is negative (an error), then 
        // we need to make sure that the stat structure is filled with 0s. Otherwise,
        // FUSE will be confused by the contradicting return values.
        memset(statbuf, 0, sizeof(struct stat));
    }

    // Clean up the memory we have allocated.
    delete []args;

    // Finally return the value we got from the server.
    return fxn_ret;
}

// CREATE, OPEN AND CLOSE
int rpc_mknod(void *userdata, const char *path, mode_t mode, dev_t dev) {
    // Called to create a file.

    // SET UP THE RPC CALL
    DLOG("rpc_mknode called for '%s'", path);
    
    // getattr has 3 arguments.
    int ARG_COUNT = 4;

    // Allocate space for the output arguments.
    void **args = new void*[ARG_COUNT];

    // Allocate the space for arg types, and one extra space for the null
    // array element.
    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    // fill arg_types and args
    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *) path;

    arg_types[1] = (1u << ARG_INPUT) | (ARG_INT << 16u);
    args[1] = (void *) &mode;

    arg_types[2] =  (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[2] = (void *) &dev;

    arg_types[3] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int returnCode = 0;
    args[3] = (void *) &returnCode;

    arg_types[4] = 0;

    // MAKE THE RPC CALL
    int rpc_ret = rpcCall((char *)"mknod", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("mknode rpc failed with error '%d'", rpc_ret);
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = returnCode;
    }

    // Clean up the memory we have allocated.
    delete []args;

    // Finally return the value we got from the server.
    return fxn_ret;
}


int rpc_open(void *userdata, const char *path,
                    struct fuse_file_info *fi) {
    // Called during open.
    // You should fill in fi->fh.

    // SET UP THE RPC CALL
    DLOG("rpc_open called for '%s'", path);
    
    int ARG_COUNT = 3;

    void **args = new void*[ARG_COUNT];

    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    // Fill in the arguments
    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *) path;

    arg_types[1] = (1u << ARG_INPUT) | (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) sizeof(struct fuse_file_info);
    args[1] = (void *) fi;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int returnCode = 0;
    args[2] = (void *) &returnCode;

    arg_types[3] = 0;

    // MAKE THE RPC CALL
    int rpc_ret = rpcCall((char *)"open", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("open rpc failed with error '%d'", rpc_ret);
        fxn_ret = rpc_ret;
    } else {
        DLOG("Return Code for open: %d", returnCode);
        fxn_ret = returnCode;
    }

    // Clean up the memory we have allocated.
    delete []args;

    // Finally return the value we got from the server.
    return fxn_ret;
}

int rpc_release(void *userdata, const char *path,
                       struct fuse_file_info *fi) {
    // Called during close, but possibly asynchronously.
    
    int ARG_COUNT = 3;

    void **args = new void*[ARG_COUNT];

    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *) path;

    arg_types[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) sizeof(struct fuse_file_info);    
    args[1] = (void *) fi;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int returnCode = 0;
    args[2] = (void *) &returnCode;

    arg_types[3] = 0;

    // MAKE THE RPC CALL
    int rpc_ret = rpcCall((char *)"release", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("open rpc failed with error '%d'", rpc_ret);
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = returnCode;
    }

    // Clean up the memory we have allocated.
    delete []args;

    // Finally return the value we got from the server.
    return fxn_ret;    
}

// READ AND WRITE DATA
int rpc_read(void *userdata, const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    // Read size amount of data at offset of file into buf.

    // Remember that size may be greater than the maximum array size of the RPC
    // library.

    int returnCode = 0;
    int fxn_ret = 0;
    int count  = 0;
    off_t new_offset = offset;
    size_t packSize = MAX_ARRAY_LEN;

    while (size > packSize) {

        int ARG_COUNT  = 6;

        void **args = new void*[ARG_COUNT];

        int arg_types[ARG_COUNT + 1];

        // The path has string length (strlen) + 1 (for the null character).
        int pathlen = strlen(path) + 1;

        arg_types[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
        args[0] = (void *) path;

        arg_types[1] = (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) MAX_ARRAY_LEN;
        args[1] = (void *) buf;

        arg_types[2] =  (1u << ARG_INPUT) | (ARG_LONG << 16u);
        args[2] = (void *) &packSize;

        arg_types[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
        args[3] = (void *) &new_offset;

        arg_types[4] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) sizeof(struct fuse_file_info);
        args[4] = (void *) fi;

        arg_types[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        args[5] = (void *) &returnCode;

        arg_types[6] = 0;

        int rpc_ret = rpcCall((char *)"read", arg_types, args);

        if (rpc_ret < 0) {
            fxn_ret = -EINVAL;
            delete []args;
            return fxn_ret;
        }
        else if (returnCode < 0) {
            fxn_ret = returnCode;
            delete []args;
            return fxn_ret;
        }

        new_offset += returnCode;
        buf += returnCode;
        size -= returnCode;  
        count += 1;

        delete []args;
    }

    int ARG_COUNT  = 6;

    void **args = new void*[ARG_COUNT];

    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *) path;

    arg_types[1] = (1u << ARG_OUTPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) size;
    args[1] = (void *) buf;

    arg_types[2] =  (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[2] = (void *) &size;

    arg_types[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[3] = (void *) &new_offset;

    arg_types[4] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) sizeof(struct fuse_file_info);
    args[4] = (void *) fi;

    arg_types[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    args[5] = (void *) &returnCode;

    arg_types[6] = 0;

    int rpc_ret = rpcCall((char *)"read", arg_types, args);

    if (rpc_ret < 0) {
        fxn_ret = -EINVAL;
    }
    else if (returnCode < 0) {
        fxn_ret = returnCode;
    } 
    else {
        fxn_ret = (count * packSize) + returnCode;
    }

    delete []args;

    return fxn_ret;
}


int rpc_write(void *userdata, const char *path, const char *buf,
                     size_t size, off_t offset, struct fuse_file_info *fi) {
    // Write size amount of data at offset of file from buf.

    // Remember that size may be greater than the maximum array size of the RPC
    int returnCode = 0;
    int fxn_ret = 0;
    int count  = 0;
    off_t new_offset = offset;
    size_t packSize = MAX_ARRAY_LEN;

    while (size > packSize) {

        int ARG_COUNT  = 6;

        void **args = new void*[ARG_COUNT];

        int arg_types[ARG_COUNT + 1];

        // The path has string length (strlen) + 1 (for the null character).
        int pathlen = strlen(path) + 1;

        arg_types[0] =
            (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
        args[0] = (void *) path;

        arg_types[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) packSize;
        args[1] = (void *) buf;

        arg_types[2] =  (1u << ARG_INPUT) | (ARG_LONG << 16u);
        args[2] = (void *) &packSize;

        arg_types[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
        args[3] = (void *) &new_offset;

        arg_types[4] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) sizeof(struct fuse_file_info);
        args[4] = (void *) fi;

        arg_types[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
        args[5] = (void *) &returnCode;

        arg_types[6] = 0;

        int rpc_ret = rpcCall((char *)"write", arg_types, args);

        if (rpc_ret < 0) {
            fxn_ret = -EINVAL;
            delete []args;
            return fxn_ret;
        }
        else if (returnCode < 0) {
            fxn_ret = returnCode;
            delete []args;
            return fxn_ret;
        }

        new_offset += returnCode;
        buf += returnCode;
        size -= returnCode;  
        count += 1;

        delete []args;
    }

    int ARG_COUNT  = 6;

    void **args = new void*[ARG_COUNT];

    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *) path;

    arg_types[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) size;
    args[1] = (void *) buf;

    arg_types[2] =  (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[2] = (void *) &size;

    arg_types[3] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[3] = (void *) &new_offset;

    arg_types[4] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) sizeof(struct fuse_file_info);
    args[4] = (void *) fi;

    arg_types[5] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    args[5] = (void *) &returnCode;

    arg_types[6] = 0;

    int rpc_ret = rpcCall((char *)"write", arg_types, args);

    if (rpc_ret < 0) {
        fxn_ret = -EINVAL;
    }
    else if (returnCode < 0) {
        fxn_ret = returnCode;
    } 
    else {
        fxn_ret = (count * packSize) + returnCode;
    }

    delete []args;

    return fxn_ret;           
}


int rpc_truncate(void *userdata, const char *path, off_t newsize) {
    // Change the file size to newsize.
    
    int ARG_COUNT = 3;

    void **args = new void*[ARG_COUNT];

    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *) path;

    arg_types[1] = (1u << ARG_INPUT) | (ARG_LONG << 16u);
    args[1] = (void *) &newsize;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int returnCode = 0;
    args[2] = (void *) &returnCode;

    arg_types[3] = 0;

    int rpc_ret = rpcCall((char *)"truncate", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("open rpc failed with error '%d'", rpc_ret);
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = returnCode;
    }

    // Clean up the memory we have allocated.
    delete []args;

    // Finally return the value we got from the server.
    return fxn_ret;
}

int rpc_fsync(void *userdata, const char *path,
                     struct fuse_file_info *fi) {
    // Force a flush of file data.
    
    int ARG_COUNT = 3;

    void **args = new void*[ARG_COUNT];

    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    arg_types[0] =
        (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint) pathlen;
    args[0] = (void *) path;

    arg_types[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) |(uint) sizeof(struct fuse_file_info);
    args[1] = (void *) fi;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int returnCode = 0;
    args[2] = (void *) &returnCode;

    arg_types[3] = 0;

    int rpc_ret = rpcCall((char *)"fsync", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
        DLOG("open rpc failed with error '%d'", rpc_ret);
        fxn_ret = -EINVAL;
    } else {
        fxn_ret = returnCode;
    }

    // Clean up the memory we have allocated.
    delete []args;

    // Finally return the value we got from the server.
    return fxn_ret;
}

// CHANGE METADATA
int rpc_utimensat(void *userdata, const char *path,
                       const struct timespec ts[2]) {

    DLOG("rpc_ultimensat called for '%s'", path);

    int ARG_COUNT = 3;

    void **args = new void*[ARG_COUNT];

    int arg_types[ARG_COUNT + 1];

    // The path has string length (strlen) + 1 (for the null character).
    int pathlen = strlen(path) + 1;

    // Fill in the arguments
    arg_types[0] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) | (uint)pathlen;
    args[0] = (void *)path;

    arg_types[1] = (1u << ARG_INPUT) | (1u << ARG_ARRAY) | (ARG_CHAR << 16u) |
      (uint)sizeof(struct timespec) * 2;
    args[1] = (void *)ts;

    arg_types[2] = (1u << ARG_OUTPUT) | (ARG_INT << 16u);
    int returnCode = 0;
    args[2] = (void *)&returnCode;

    arg_types[3] = 0;

    // MAKE THE RPC CALL
    int rpc_ret = rpcCall((char *)"ultimensat", arg_types, args);

    int fxn_ret = 0;
    if (rpc_ret < 0) {
       fxn_ret = -EINVAL;
    } else {
       fxn_ret = returnCode;
    }

    delete []args;

    return fxn_ret;
}
