**System Manual**

**Design Choices**

**Global Data (Client)**

*struct file\_info*: This structure represents information about a file, including:

- *client\_fi*: An integer representing the file descriptor on the client side.
- *server\_fi*: An integer representing the file descriptor on the server side.
- *flags*: An integer representing various flags or attributes associated with the file.
- *tc*: A time\_t variable representing the time of creation or modification of the file.

*struct files\_store*: This structure holds data related to managing files, including:

- *cur\_open\_files*: A map data structure associating file paths (as strings) with their corresponding *file\_info* structures, effectively storing information about currently open files.
- *cache\_interval*: A time\_t variable representing the interval for caching files.
- *path\_to\_cache*: A pointer to a constant character array (C-string) representing the path to the cache directory.

The client and server handles are stored on the client side to make sure that we don’t have to open a file each time there is a need to Download/Upload from the Server/Client. 

**Global Data (Server)**

*struct file\_mutex*: This structure represents information about a file's mutex, including:

- *mode*: An integer representing the mode of the file mutex.
- *num\_times\_opened*: An integer representing the number of times the file has been opened.
- *lock*: A pointer to a read-write lock (rw\_lock\_t) associated with the file.

*class server\_mutex*: This class manages file mutexes on the server-side, including:

- *files*: A map data structure associating file paths (as strings) with their corresponding file\_mutex structures, effectively storing information about file mutexes.

The class server mutex has many methods associated with operations on files data such adding/removing, getter/setters etc. The interface for this can be found in global.h

**Download From Server to Client**

These are the steps taken to implement this as seen in function download\_from\_server\_to\_client() in utils.cpp:

1. Firstly, we lock the file path in read mode to mark the file in transfer.
1. We retrieve file info from the server with a rpc call to getattr.
1. Next, we check if the file is already open at the client from the global data.
1. If file is not already open at the client, we open it. In case the local copy of the file does not exist, we create it using mknod and then open it.
1. After opening the local copy of the file, we make a rpc call to open in order to open the file at the server.
1. If the file was already open to begin with, then we skip steps 3 and 4. Instead we initialize variables to store the file handles for the file (both file handles for client and server) using the global data.
1. Make a rpc call to read using the server’s file handle and read all its data.
1. Write the data retrieved from the server to the local copy of the file stored at client.
1. Modify the metadata of the local copy of the file.
1. If the file was not already open to begin with and we did steps 3 and 4, then make an rpc call to release and a call to close. We do this to close the file at both client and server.
1. Unlock the file path and mark the file to not in transfer.
1. File has successfully downloaded.

**Upload From Client to Server**

These are the steps taken to implement this as seen in function upload\_from\_client\_to\_server() in utils.cpp:

1. Firstly, we lock the path in write mode to mark the file in transfer for writes.
1. We retrieve file information from the client using a stat call.
1. Next, we check if the file is already open at the client from the global data.
1. If file is not already open at the client, we first make an rpc call to open and open the file at the server. In case the file does not exist at the server, we create it using an rpc call to mknod and then make an rpc call to open again.
1. After opening the file at the server, we then open the local copy of the file at the client.
1. If the file was already open to begin with, then we skip steps 3 and 4. Instead we initialize variables to store the file handles for the file (both file handles for client and server) using the global data.
1. We then read the local copy of the file and read all it’s data.
1. We make a rpc call to truncate and truncate the size of file at the server to 0.
1. Make a rpc call to write; to copy over all the data from the local copy of the file to the file at the server.
1. We update the metadata of the file at the server by making an rpc call to utimensat
1. If the file was not already open to begin with and we did steps 3 and 4, then make an rpc call to release and a call to close. We do this to close the file at both client and server.
1. Unlock the file path and mark the file to not in transfer.
1. File has successfully been uploaded.

**Atomicity**

The two rpc calls have been implemented to indicate that a file is in transfer either to or from the server. In order to mark as in transfer for reads or writes I implemented:

*lock(path, mode)*

*unlock(path, mode)*

Download\_from\_server\_to\_client tries to acquire and hold a lock in *RW\_READ\_LOCK* mode by making an rpc call to lock and releases the lock after the download is complete by making an rpc call to unlock. Similarly, Upload\_from\_client\_to\_server tries to acquire and hold a lock in *RW\_WRITE\_LOCK* mode and then releases the lock after the upload is complete.

On the server side, each file that is opened has an associated lock. The rpc calls to lock and unlock use the lock associated with a file to acquire and release. 

This implementation ensures that download and upload are atomic in nature.

**Mutual Exclusion**

At the client we have a map in the global data where we keep track of all the open files. If we detect that a file that is already in the map is being opened, we return a -EMFILE error ensuring mutual exclusion.

At the server also we have a map in the global data where we keep track of all open files. We use this map to ensure that if a file is already open in write mode by a client, then the file can’t be opened in write mode by another client. However, any number of clients can open this file in read mode. This ensures mutual exclusion at the server.

**Cache Invalidation**

I have implemented a function is\_file\_fresh that checks the freshness. This is how it’s been implemented:

1. We can get tc (time cache entry was last validated) and the cache interval from global data, and check if current\_time – tc < cache\_interval. If it is then we return true.
1. If not, then we retrieve file metadata of the local copy of the file using a stat call. This is done to retrieve T\_client (last time client was modified)
1. We make an rpc call to getattr to retrieve file attributes from the file at the server. This is done to retrieve T\_server (last time server was modified)
1. Now we check if T\_client == T\_server. If it is then we modify tc to current time and return true. If not we return false

In addition to this I have also implemented function update\_TServer\_to\_TClient and update\_TClient\_to\_TServer that do as their name suggests. update\_TServer\_to\_TClient is called every time the server file is modified with upload and update\_TClient\_to\_TServer is called every time the client copy of the file is modified with Download.

**Areas that are not complete:**

Received full marks in public and release tests.

**Error Codes**

Some of the error codes that we return are:

1. -EMFILE (-24): Too many files open
1. -EACCESS (-13) : Can’t open file in write mode, if it is already open
1. BAD\_TYPES (-205): Can’t fsync a file that is open in read only mode

**Testing**

1. Open-Write-Read-Close test: This test opens a file with O\_RDWR & O\_CREAT flags, writes some text to it, then reads and prints the text, before closing the file. This test checks the workings of  watdfs function for gettattr, mknod, open, read, write, and release along with the download/upload model and cache invalidation.
1. Open-Write-Truncate-Fysnc-Utime-Close test: This test open a file with O\_RDWR flags and writes some text with length 100. We then truncate the file to 20 characters and then call fysnc. We then call utime function before closing the file. This test checks the workings of watdfs function for getattr, open, write, truncate, fsync, utimensat, release.
1. Concurrent Access Test: Create a server and two clients. Client 1 open a file in O\_RDWR mode and sleeps for 30 seconds. After client 1 opens, client 2 tries to open the same file in O\_WRONLY mode. The second client can’t open the file at gets an -EACCESS error.
1. Read-Many test: Create a server and two clients. Client 1 opens a file in O\_RDWR mode and sleeps for 30 seconds. Client 2 tries to open the same file in O\_RDONLY mode and is able to successfully do so.
1. Too many files open test: Try to open a file that is already open. Receive a -EMFILE error.
1. Fsync test:  Open file in read only mode, write some text to it, and then perform fsync operation. fsync should fail with a BAD\_TYPES error.

