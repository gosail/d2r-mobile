/*
 * Fail-closed read/write of a versioned two-hireling sidecar inside the
 * UNCOMMITTED save worktree. The existing HLE whole-directory transaction
 * must publish this together with the original character .d2s.
 * Not active in the game until the native load/save bridges are verified.
 */
#ifndef DM_DUALMERC_DISK_H
#define DM_DUALMERC_DISK_H
#include "dm_dualmerc_state.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

static inline int dm_dualmerc_disk_name(const char *name) {
    if (!name) return 0;
    size_t n = strlen(name);
    if (!n || n > 96 || name[0] == '.') return 0;
    for (size_t i = 0; i < n; ++i)
        if ((unsigned char)name[i] < 0x20 || name[i] == '/' || name[i] == '\\' ||
            (name[i] == '.' && i + 1 < n && name[i+1] == '.')) return 0;
    return 1;
}
static inline int dm_dualmerc_disk_dir(const char *workdir, const char *name) {
    if (!workdir || !*workdir || !dm_dualmerc_disk_name(name)) {
        errno = EINVAL; return -1;
    }
    int root = open(workdir, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (root < 0) return -1;
    int dir = openat(root, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    int e = errno;
    close(root);
    if (dir < 0) errno = e;
    return dir;
}
static inline int dm_dualmerc_disk_write_all(int fd, const uint8_t *p, size_t n) {
    while (n) {
        ssize_t c = write(fd, p, n);
        if (c < 0 && errno == EINTR) continue;
        if (c <= 0) { if (!c) errno = EIO; return -1; }
        p += c; n -= (size_t)c;
    }
    return 0;
}
static inline int dm_dualmerc_disk_save(const char *workdir, const char *name,
                                        const dm_dualmerc_saved_state *state) {
    if (!state || !dm_dualmerc_state_valid(state)) { errno = EINVAL; return -1; }
    int dir = dm_dualmerc_disk_dir(workdir, name);
    if (dir < 0) return -1;
    uint8_t *buf = malloc(DM_DUALMERC_STATE_BYTES_MAX);
    size_t n = 0;
    if (!buf || dm_dualmerc_state_encode(state, buf, DM_DUALMERC_STATE_BYTES_MAX, &n)) {
        free(buf); close(dir); errno = ENOMEM; return -1;
    }
    char file[128], tmp[140];
    int a = snprintf(file, sizeof file, "%s.swmerc", name);
    int b = snprintf(tmp, sizeof tmp, ".%s.swmerc.tmp", name);
    if (a < 0 || a >= (int)sizeof file || b < 0 || b >= (int)sizeof tmp) {
        free(buf); close(dir); errno = ENAMETOOLONG; return -1;
    }
    /* A stale partial temp is never a committed save. */
    unlinkat(dir, tmp, 0);
    int fd = openat(dir, tmp, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC | O_NOFOLLOW, 0600);
    int err = 0;
    if (fd < 0) err = errno;
    else {
        if (dm_dualmerc_disk_write_all(fd, buf, n) || fsync(fd)) err = errno;
        if (close(fd) && !err) err = errno;
    }
    free(buf);
    if (!err && renameat(dir, tmp, dir, file)) err = errno;
    if (!err && fsync(dir)) err = errno;
    if (err) { unlinkat(dir, tmp, 0); close(dir); errno = err; return -1; }
    close(dir);
    return 0;
}
static inline int dm_dualmerc_disk_load(const char *workdir, const char *name,
                                        dm_dualmerc_saved_state *out) {
    if (!out) { errno = EINVAL; return -1; }
    int dir = dm_dualmerc_disk_dir(workdir, name);
    if (dir < 0) return -1;
    char file[128];
    if (snprintf(file, sizeof file, "%s.swmerc", name) >= (int)sizeof file) {
        close(dir); errno = ENAMETOOLONG; return -1;
    }
    int fd = openat(dir, file, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    int err = 0;
    if (fd < 0) { err = errno; close(dir); errno = err; return -1; }
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 92 ||
        st.st_size > DM_DUALMERC_STATE_BYTES_MAX) err = EINVAL;
    uint8_t *buf = err ? NULL : malloc((size_t)st.st_size);
    if (!err && !buf) err = ENOMEM;
    if (!err) {
        size_t n = (size_t)st.st_size, got = 0;
        while (got < n) {
            ssize_t r = read(fd, buf + got, n - got);
            if (r < 0 && errno == EINTR) continue;
            if (r <= 0) { err = r < 0 ? errno : EIO; break; }
            got += (size_t)r;
        }
        struct stat end;
        if (!err && (fstat(fd, &end) || end.st_size != st.st_size ||
                     end.st_mtime != st.st_mtime)) err = EIO;
        if (!err && dm_dualmerc_state_decode(buf, n, out)) err = EBADMSG;
    }
    free(buf);
    close(fd); close(dir);
    if (err) { errno = err; return -1; }
    return 0;
}
#endif
