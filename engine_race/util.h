#if !defined(UTIL_H)
#define UTIL_H
#include <dirent.h>
#include <string.h>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <mylib.h>

// helper functions copied from engine_example
inline bool FileExists(const std::string &path) {
    return access(path.c_str(), F_OK) == 0;
}
inline int GetFileLength(const std::string &file) {
    struct stat stat_buf;
    int rc = stat(file.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}
inline int FileAppend(int fd, const std::string &value) {
    if (fd < 0) {
        return -1;
    }
    size_t value_len = value.size();
    log_trace("FileAppend:fd=%d,len=%lu",fd,value_len);
    const char *pos = value.data();
    while (value_len > 0) {
        ssize_t r = write(fd, pos, value_len);
        if (r < 0) {
            if (errno == EINTR) {
                continue; // Retry
            }
            perror("file write fail");
            return -1;
        }
        pos += r;
        value_len -= r;
    }
    return 0;
}
inline int FileAppend(int fd, const char *data, size_t length) {
    if (fd < 0) {
        return -1;
    }
    log_trace("in FileAppend: fd=%d, length=%lu", fd, length);
    while (length > 0) {
        ssize_t r = write(fd, data, length);
        if (r < 0) {
            if (errno == EINTR) {
                continue; // Retry
            }
            return -1;
        }
        data += r;
        length -= r;
    }
    return 0;
}
#endif // UTIL_H
