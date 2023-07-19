// SPDX-License-Identifier: MIT

/* Parse /proc/meminfo
 * Returned values are in kiB */

#include <errno.h>
#include <signal.h>
#include <stddef.h> // for size_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "globals.h"
#include "meminfo.h"
#include "msg.h"

/* Parse the contents of /proc/meminfo (in buf), return value of "name"
 * (example: "MemTotal:")
 * Returns -errno if the entry cannot be found. */
static long long get_entry(const char* name, const char* buf)
{
    char* hit = strstr(buf, name);
    if (hit == NULL) {
        return -ENODATA;
    }

    errno = 0;
    long long val = strtoll(hit + strlen(name), NULL, 10);
    if (errno != 0) {
        int strtoll_errno = errno;
        warn("%s: strtol() failed: %s", __func__, strerror(errno));
        return -strtoll_errno;
    }
    return val;
}

/* Like get_entry(), but exit if the value cannot be found */
static long long get_entry_fatal(const char* name, const char* buf)
{
    long long val = get_entry(name, buf);
    if (val < 0) {
        warn("%s: fatal error, dumping buffer for later diagnosis:\n%s", __func__, buf);
        fatal(104, "could not find entry '%s' in /proc/meminfo: %s\n", name, strerror((int)-val));
    }
    return val;
}

/* If the kernel does not provide MemAvailable (introduced in Linux 3.14),
 * approximate it using other data we can get */
static long long available_guesstimate(const char* buf)
{
    long long Cached = get_entry_fatal("Cached:", buf);
    long long MemFree = get_entry_fatal("MemFree:", buf);
    long long Buffers = get_entry_fatal("Buffers:", buf);
    long long Shmem = get_entry_fatal("Shmem:", buf);

    return MemFree + Cached + Buffers - Shmem;
}

/* Parse /proc/meminfo.
 * This function either returns valid data or kills the process
 * with a fatal error.
 */
meminfo_t parse_meminfo()
{
    // Note that we do not need to close static FDs that we ensure to
    // `fopen()` maximally once.
    static FILE* fd;
    static int guesstimate_warned = 0;
    // On Linux 5.3, "wc -c /proc/meminfo" counts 1391 bytes.
    // 8192 should be enough for the foreseeable future.
    char buf[8192] = { 0 };
    meminfo_t m = { 0 };

    if (fd == NULL) {
        char buf[PATH_LEN] = { 0 };
        snprintf(buf, sizeof(buf), "%s/%s", procdir_path, "meminfo");
        fd = fopen(buf, "r");
    }
    if (fd == NULL) {
        fatal(102, "could not open /proc/meminfo: %s\n", strerror(errno));
    }
    rewind(fd);

    size_t len = fread(buf, 1, sizeof(buf) - 1, fd);
    if (ferror(fd)) {
        fatal(103, "could not read /proc/meminfo: %s\n", strerror(errno));
    }
    if (len == 0) {
        fatal(103, "could not read /proc/meminfo: 0 bytes returned\n");
    }

    m.MemTotalKiB = get_entry_fatal("MemTotal:", buf);
    m.MemFreeKiB = get_entry_fatal("MemFree:", buf);
    m.SwapTotalKiB = get_entry_fatal("SwapTotal:", buf);
    m.AnonPagesKiB = get_entry_fatal("AnonPages:", buf);
    m.SwapFreeKiB = get_entry_fatal("SwapFree:", buf);
    m.MemFileCacheKiB = get_entry_fatal("Active(file):", buf);
    m.MemFileCacheKiB += get_entry_fatal("Inactive(file):", buf);

    m.MemAvailableKiB = get_entry("MemAvailable:", buf);
    if (m.MemAvailableKiB < 0) {
        m.MemAvailableKiB = available_guesstimate(buf);
        if (guesstimate_warned == 0) {
            fprintf(stderr, "Warning: Your kernel does not provide MemAvailable data (needs 3.14+)\n"
                            "         Falling back to guesstimate\n");
            guesstimate_warned = 1;
        }
    }

    // Calculated values
    m.UserMemTotalKiB = m.MemAvailableKiB + m.AnonPagesKiB;

    // Calculate percentages
    m.MemAvailablePercent = (double)m.MemAvailableKiB * 100 / (double)m.UserMemTotalKiB;
    if (m.SwapTotalKiB > 0) {
        m.SwapFreePercent = (double)m.SwapFreeKiB * 100 / (double)m.SwapTotalKiB;
    } else {
        m.SwapFreePercent = 0;
    }

    return m;
}

bool is_alive(int pid)
{
    // whole process group (-g flag)?
    if (pid < 0) {
        // signal 0 does nothing, but we do get an error when the process
        // group does not exist.
        int res = kill(pid, 0);
        if (res == 0) {
            return true;
        }
        return false;
    }

    char buf[PATH_LEN] = { 0 };
    // Read /proc/[pid]/stat
    snprintf(buf, sizeof(buf), "%s/%d/stat", procdir_path, pid);
    FILE* f = fopen(buf, "r");
    if (f == NULL) {
        // Process is gone - good.
        return false;
    }

    // File content looks like this:
    // 10751 (cat) R 2663 10751 2663[...]
    // File may be bigger than 256 bytes, but we only need the first 20 or so.
    memset(buf, 0, sizeof(buf));
    size_t len = fread(buf, 1, sizeof(buf), f);
    bool read_error = ferror(f) || len == 0;
    fclose(f);
    if (read_error) {
        warn("%s: fread failed: %s\n", __func__, strerror(errno));
        return false;
    }

    // Find last ")" by searching from the end
    int i = sizeof(buf) - 1;
    for (; i >= 0; i--) {
        if (buf[i] == ')')
            break;
    }
    if (i <= 0 || i + 2 >= (int)sizeof(buf)) {
        warn("%s: could not find closing bracket\n", __func__);
        return false;
    }
    char state = buf[i + 2];

    debug("process state: %c\n", state);
    if (state == 'Z') {
        // A zombie process does not use any memory. Consider it dead.
        return false;
    }
    return true;
}

/* Read /proc/[pid]/[name] and convert to integer.
 * As the value may legitimately be < 0 (think oom_score_adj),
 * it is stored in the `out` pointer, and the return value is either
 * 0 (success) or -errno (failure).
 */
static int read_proc_file_integer(const int pid, const char* name, int* out)
{
    char path[PATH_LEN] = { 0 };
    snprintf(path, sizeof(path), "%s/%d/%s", procdir_path, pid, name);
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return -errno;
    }
    int matches = fscanf(f, "%d", out);
    fclose(f);
    if (matches != 1) {
        return -ENODATA;
    }
    return 0;
}

/* Read /proc/[pid]/oom_score.
 * Returns the value (>= 0) or -errno on error.
 */
int get_oom_score(const int pid)
{
    int out = 0;
    int res = read_proc_file_integer(pid, "oom_score", &out);
    if (res < 0) {
        return res;
    }
    return out;
}

/* Read /proc/[pid]/oom_score_adj.
 * As the value may legitimately be negative, the return value is
 * only used for error indication, and the value is stored in
 * the `out` pointer.
 * Returns 0 on success and -errno on error.
 */
int get_oom_score_adj(const int pid, int* out)
{
    return read_proc_file_integer(pid, "oom_score_adj", out);
}

/* Read /proc/[pid]/comm (process name truncated to 16 bytes).
 * Returns 0 on success and -errno on error.
 */
int get_comm(int pid, char* out, size_t outlen)
{
    char path[PATH_LEN] = { 0 };
    snprintf(path, sizeof(path), "%s/%d/comm", procdir_path, pid);
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return -errno;
    }
    size_t n = fread(out, 1, outlen - 1, f);
    if (ferror(f)) {
        int fread_errno = errno;
        perror("get_comm: fread() failed");
        fclose(f);
        return -fread_errno;
    }
    fclose(f);
    // Process name may be empty, but we should get at least a newline
    // Example for empty process name: perl -MPOSIX -e '$0=""; pause'
    if (n < 1) {
        return -ENODATA;
    }
    // Strip trailing newline
    out[n - 1] = 0;
    fix_truncated_utf8(out);
    return 0;
}

int get_cmdline(int pid, char *out, int out_len) {
    char cmdline_file[128] = {0};
    int fd;
    int i;
    ssize_t nread, total = 0;

    sprintf(cmdline_file, "%s/%d/cmdline", procdir_path, pid);

    fd = open(cmdline_file, O_RDONLY);
    if (fd == -1) {
        printf("Failed to open %s\n", cmdline_file);
        return -1; 
    }   

    while ((nread = read(fd, out + total, out_len - total - 1)) > 0) {
        total += nread;
    }   

    close(fd);

    if (total == -1) {
        printf("Failed to read %s\n", cmdline_file);
        return -1; 
    }   

    // Replace '\0' with spaces when arguments are not separated by '\0'
    for (i = 0; i < total; i++) {
        if (out[i] == '\0') {
            out[i] = ' ';
        }   
    }   

    out[total] = '\0';

    return 0;
}


// Get the effective uid (EUID) of `pid`.
// Returns the uid (>= 0) or -errno on error.
int get_uid(int pid)
{
    char path[PATH_LEN] = { 0 };
    snprintf(path, sizeof(path), "/proc/%d", pid);
    struct stat st = { 0 };
    int res = stat(path, &st);
    if (res < 0) {
        return -errno;
    }
    return (int)st.st_uid;
}

// Read VmRSS from /proc/[pid]/statm and convert to kiB.
// Returns the value (>= 0) or -errno on error.
long long get_vm_rss_kib(int pid)
{
    long long vm_rss_kib = -1;
    char path[PATH_LEN] = { 0 };

    // Read VmRSS from /proc/[pid]/statm (in pages)
    snprintf(path, sizeof(path), "%s/%d/statm", procdir_path, pid);
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return -errno;
    }
    int matches = fscanf(f, "%*u %lld", &vm_rss_kib);
    fclose(f);
    if (matches < 1) {
        return -ENODATA;
    }

    // Read and cache page size
    static long page_size;
    if (page_size == 0) {
        page_size = sysconf(_SC_PAGESIZE);
        if (page_size <= 0) {
            fatal(1, "could not read page size\n");
        }
    }

    // Convert to kiB
    vm_rss_kib = vm_rss_kib * page_size / 1024;
    return vm_rss_kib;
}

/* Print a status line like
 *   mem avail: 5259 MiB (67 %), swap free: 0 MiB (0 %)"
 * as an informational message to stdout (default), or
 * as a warning to stderr.
 */
void print_mem_stats(int __attribute__((format(printf, 1, 2))) (*out_func)(const char* fmt, ...), const meminfo_t m)
{
    out_func("mem avail: %5lld of %5lld MiB (" PRIPCT "), filecache: %5lld MiB, free: %5lld MiB\n",
        m.MemAvailableKiB / 1024,
        m.UserMemTotalKiB / 1024,
        m.MemAvailablePercent,
        m.MemFileCacheKiB/1024,
        m.MemFreeKiB/1024);
}
