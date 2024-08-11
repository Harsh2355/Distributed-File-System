#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int rpc_getattr(void *userdata, const char *path, struct stat *statbuf);

int rpc_mknod(void *userdata, const char *path, mode_t mode, dev_t dev);

int rpc_open(void *userdata, const char *path, struct fuse_file_info *fi);

int rpc_release(void *userdata, const char *path, struct fuse_file_info *fi);

int rpc_read(void *userdata, const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

int rpc_write(void *userdata, const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

int rpc_truncate(void *userdata, const char *path, off_t newsize);

int rpc_fsync(void *userdata, const char *path, struct fuse_file_info *fi);

int rpc_utimensat(void *userdata, const char *path, const struct timespec ts[2]);

