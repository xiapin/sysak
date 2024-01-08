//
// Created by 廖肇燕 on 2022/12/13.
//

#include "foxTSDB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#define FOX_MAGIC 0x0f030f03
#define MICRO_UNIT (1000 * 1000UL)

// for lseek mode choose.
#ifndef FOX_LSEEK_MODE
#define FOX_LSEEK_MODE 64
#endif

#if (FOX_LSEEK_MODE==64)
#define FOX_LSEEK lseek64
#else
#define FOX_LSEEK lseek
#endif

struct fox_index {
    fox_time_t t_us;
    fox_off_t off;
    int table_len;
    int data_len;
};
#define FOX_INDEX_SIZE sizeof(struct fox_index)

fox_time_t get_us(void) {
    fox_time_t res = 0;
    struct timeval tv;

    if (gettimeofday(&tv, NULL) == 0) {
        res = tv.tv_sec * MICRO_UNIT + tv.tv_usec;
    }
    return res;
}

static void tm2date(struct tm * ptm, struct foxDate * p) {
    p->year = ptm->tm_year;
    p->mon  = ptm->tm_mon;
    p->mday = ptm->tm_mday;
    p->hour = ptm->tm_hour;
    p->min  = ptm->tm_min;
    p->sec  = ptm->tm_sec;
}

static void date2tm(struct foxDate * p, struct tm * ptm) {
    ptm->tm_year = p->year;
    ptm->tm_mon  = p->mon;
    ptm->tm_mday = p->mday;
    ptm->tm_hour = p->hour;
    ptm->tm_min  = p->min;
    ptm->tm_sec  = p->sec;
    ptm->tm_isdst = 0;
}

int get_date_from_us(fox_time_t us, struct foxDate * p) {
    struct tm * ptm;

    time_t t = us / MICRO_UNIT;
    ptm = gmtime(&t);
    tm2date(ptm, p);
    return 0;
}

int get_date(struct foxDate * p) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        struct tm * ptm = gmtime(&tv.tv_sec);
        tm2date(ptm, p);
        return 0;
    }
    return 1;
}

fox_time_t make_stamp(struct foxDate * p) {
    struct tm tmt;

    date2tm(p, &tmt);
    time_t t = mktime(&tmt);
    return t * MICRO_UNIT;
}

static fox_time_t fox_read_init_now(struct fox_manager* pman) {
    struct foxDate date;

    date.year = pman->year;
    date.mon = pman->mon;
    date.mday = pman->mday;
    date.hour = date.min = date.sec = 0;

    return make_stamp(&date);
}

static fox_time_t fox_read_day_max(struct fox_manager* pman) {
    struct foxDate date;

    date.year = pman->year;
    date.mon = pman->mon;
    date.mday = pman->mday;
    date.hour = date.min = date.sec = 0;

    return make_stamp(&date) + 24 * 60 * 60 * MICRO_UNIT;
}

int check_foxdate(struct foxDate* d1, struct foxDate* d2) {
    return  (d1->year == d2->year) && \
            (d1->mon == d2->mon) && \
            (d1->mday == d2->mday);
}

int check_pman_date(struct fox_manager* pman, struct foxDate* pdate) {
    return  (pman->year == pdate->year) && \
            (pman->mon == pdate->mon) && \
            (pman->mday == pdate->mday);
}

// tell file size.
static size_t fd_size(int fd) {
    int ret;
    struct stat file_info;

    ret = fstat(fd, &file_info);
    if (ret < 0) {
        fprintf(stderr, "stat file failed. %d, %s\n", errno, strerror(errno));
    }
    return (!ret) ? file_info.st_size : -EACCES;
}

// create .foxi  file name
static void pack_iname(char* pname, struct foxDate * p) {
    snprintf(pname, FNAME_SIZE, "%04d%02d%02d.foxi", p->year, p->mon, p->mday);
}

// create .fox file name
static void pack_fname(char* pname, struct foxDate * p) {
    snprintf(pname, FNAME_SIZE, "%04d%02d%02d.fox", p->year, p->mon, p->mday);
}

//open .foxi to write data index info
static int fox_open_w_fdi(struct foxDate * p, char * iname) {
    int fd;
    int ret;
    fox_off_t off;

    pack_iname(iname, p);

    fd = open(iname, O_RDWR | O_APPEND | O_CREAT);
    if (fd < 0) {
        fprintf(stderr, "open %s error, return %d, %s\n", iname, errno, strerror(errno));
        ret = -ENOENT;
        goto endOpen;
    }

    off = FOX_LSEEK(fd, 0, SEEK_END);
    if (off < 0) {
        fprintf(stderr, "fox_open_w_fdi lseek %s error, return %d, %s\n", iname, errno, strerror(errno));
        ret = -errno;
        goto endSeek;
    }

    return fd;
    endSeek:
    close(fd);
    endOpen:
    return ret;
}

//open .fox to write data index info
static int fox_open_w_fd(struct foxDate * p, char * fname) {
    int fd;
    int ret;
    fox_off_t off;

    pack_fname(fname, p);
    fd = open(fname, O_RDWR | O_APPEND | O_CREAT);
    if (fd < 0) {
        fprintf(stderr, "open %s error, return %d, %s", fname, errno, strerror(errno));
        ret = -ENOENT;
        goto endOpen;
    }

    off = FOX_LSEEK(fd, 0, SEEK_END);
    if (off < 0) {
        fprintf(stderr, "fox_open_w_fd lseek %s error, return %d, %s\n", fname, errno, strerror(errno));
        ret = -errno;
        goto endSeek;
    }

    return fd;
    endSeek:
    close(fd);
    endOpen:
    return ret;
}

// export for setup write
int fox_setup_write(struct fox_manager* pman, struct foxDate * p, fox_time_t now) {
    int ret = 0;
    size_t pos;

    pman->fdi = fox_open_w_fdi(p, pman->iname);   // open fdi for data index.
    if (pman->fdi < 0) {
        ret = pman->fdi;
        goto endOpeni;
    }

    pman->fd = fox_open_w_fd(p, pman->fname);    // open fd for data.
    if (pman->fd < 0) {
        ret = pman->fd;
        goto endOpen;
    }

    pos = fd_size(pman->fd);
    if (pos < 0) {
        ret = pos;
        goto endSize;
    }

    pman->year = p->year;
    pman->mon  = p->mon;
    pman->mday = p->mday;
    pman->now = now;
    pman->w_off = pos;

    return ret;

    endSize:
    close(pman->fd);
    endOpen:
    pman->fd = 0;
    close(pman->fdi);
    endOpeni:
    pman->fdi = 0;
    return ret;
}

static int fox_write_data(struct fox_manager* pman, fox_time_t us,
        const char* tData, int tLen,
        const char* data, int len) {
    int ret = 0;
    int fd = pman->fd;
    int fdi = pman->fdi;
    struct fox_index index;

    index.t_us = us;
    index.off  = pman->w_off;
    index.table_len  = tLen;
    index.data_len   = len;

    //write foxi
    ret = write(fdi, &index, FOX_INDEX_SIZE);
    if (ret < 0) {
        fprintf(stderr, "write foxi file failed. %d, %s\n", errno, strerror(errno));
        goto endWrite;
    }

    //write fox
    ret = write(fd, tData, tLen );  // for table
    if (ret < 0) {
        fprintf(stderr, "write data file table failed. %d, %s\n", errno, strerror(errno));
        goto endWrite;
    }
    ret = write(fd, data, len );  // for stream
    if (ret < 0) {
        fprintf(stderr, "write file failed. %d, %s\n", errno, strerror(errno));
        goto endWrite;
    }

    pman->now = us;
    pman->w_off += tLen + len;
    return  ret;
    endWrite:
    return ret;
}

int fox_write(struct fox_manager* pman, struct foxDate* pdate, fox_time_t us,
              const char* tData, int tLen,
              const char* data, int len) {
    int res = 0;

    if (!check_pman_date(pman, pdate)) {  // new day?
        fox_del_man(pman);   // free old
        res = fox_setup_write(pman, pdate, us);
        if (res < 0) {
            fprintf(stderr, "create new file failed.\n");
            goto endCreateFile;
        }
        pman->new_day = 1;
    } else {
        pman->new_day = 0;
    }

    if (pman->now <= us) {  // time should monotonically increasing
        res = fox_write_data(pman, us, tData, tLen, data, len);
    }
    return res;

    endCreateFile:
    return res;
}

static int fox_index_get(int fdi, fox_off_t index, struct fox_index *pindex) {
    int ret = 0;
    size_t size;
    fox_off_t pos = index * FOX_INDEX_SIZE;

    ret = FOX_LSEEK(fdi, pos, SEEK_SET);
    if (ret < 0) {
        fprintf(stderr, "seek file failed. %d, %s\n", errno, strerror(errno));
        ret = -EACCES;
        goto endSeek;
    }

    size = read(fdi, pindex, FOX_INDEX_SIZE);
    if (size == FOX_INDEX_SIZE) {
        return size;
    } else if (size > 0) {
        fprintf(stderr, "read index file return %ld.\n", size);
        ret = -EACCES;
        goto endRead;
    } else {
        fprintf(stderr, "read index file failed. %d, %s\n", errno, strerror(errno));
        ret = -EACCES;
        goto endRead;
    }

    return ret;
    endRead:
    endSeek:
    return ret;
}

static void fox_cursor_save(struct fox_manager* pman, struct fox_index *pindex, fox_off_t find) {
    pman->r_index = find;

    pman->r_next = pman->r_index + 1;
    if (pman->r_next > pman->cells - 1) {  // r_next should less than pman->cells - 1 and 0;
        pman->r_next = pman->cells ? pman->cells - 1 : 0 ;
    }
    pman->now = pindex->t_us;
    pman->data_pos = pindex->off;
    pman->table_len = pindex->table_len;
    pman->data_len = pindex->data_len;
}

//cursor left
static int fox_cursor_left(int fdi, struct fox_manager* pman, fox_time_t now) {
    int ret = 0;
    fox_off_t start = 0, end = pman->r_index;
    fox_off_t mid = start;
    struct fox_index index = {0, 0, 0, 0};
    fox_time_t us = 0;

    while (start < end) {
        mid = start + (end - start) / 2;

        ret = fox_index_get(fdi, mid, &index);
        us= index.t_us;
        if (ret < 0) {
            fprintf(stderr, "fox index %ld get failed. %d, %s\n", mid, errno, strerror(errno));
            ret = -EACCES;
            goto endSeek;
        }

        if (now < us) {    // now is little, upper half
            end = mid;
        } else if (now > us) {  // now is large, lower region
            start = mid + 1;
        } else {   // equal
            break;
        }
    }
    fox_cursor_save(pman, &index, mid);
    return 0;
    endSeek:
    return ret;
}

static int fox_cursor_right(int fdi, struct fox_manager* pman, fox_time_t now) {
    int ret = 0;
    fox_off_t start = pman->r_index, end = pman->cells ? pman->cells - 1 : 0;
    fox_off_t mid = start;
    struct fox_index index = {0, 0, 0, 0};
    fox_time_t us = 0;

    while (start < end) {
        mid = start + (end - start) / 2;

        ret = fox_index_get(fdi, mid, &index);
        us = index.t_us;
        if (ret < 0) {
            fprintf(stderr, "fox index %ld get failed. %d, %s\n", mid, errno, strerror(errno));
            ret = -EACCES;
            goto endSeek;
        }

        if (now < us) {    // now is little, upper half
            end = mid;
        } else if (now > us) {  // now is large, lower region
            start = mid + 1;
        } else {   // equal
            break;
        }
    }
    fox_cursor_save(pman, &index, mid);
    return 0;
    endSeek:
    return ret;
}

static int fox_cursor(int fdi, struct fox_manager* pman, fox_time_t now) {
    int ret = 0;

    if (pman->now > now) {
        ret = fox_cursor_left(fdi, pman, now);
        if (ret < 0) {
            goto endCursor;
        }
    } else if (pman->now < now) {
        ret = fox_cursor_right(fdi, pman, now);
        if (ret < 0) {
            goto endCursor;
        }
    }

    return ret;
    endCursor:
    return ret;
}

int fox_cur_move(struct fox_manager* pman, fox_time_t now) {
    int ret = fox_cursor(pman->fdi, pman, now);
    return ret;
}

static int fox_cursor_next(struct fox_manager* pman, struct fox_index* pindex) {
    int ret = 0;

    ret = fox_index_get(pman->fdi, pman->r_next, pindex);
    if (ret < 0) {
        goto endIndex;
    }

    fox_cursor_save(pman, pindex, pman->r_next);
    return ret;
    endIndex:
    return ret;
}

int fox_read_resize(struct fox_manager* pman) {
    int ret = 0;

    size_t isize = fd_size(pman->fdi);  // fresh new file size.
    if (isize < 0) {
        ret = isize;
        goto endSize;
    }
    pman->isize = isize;
    pman->cells = isize / FOX_INDEX_SIZE;
    if (pman->r_index == pman->r_next) {  // end of file? check new next.
        if (pman->r_next < pman->cells - 1) {
            pman->r_next = pman->r_index + 1;
        }
    }

    return ret;
    endSize:
    return ret;
}

//open .foxi to write data index info
static int fox_open_r_fdi(struct foxDate * p, int* pfd, char *iname) {
    int fd;
    int ret = 0;

    pack_iname(iname, p);
    fd = open(iname, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {  // not such file.
            ret = errno;
            goto endOpen;
        }
        fprintf(stderr, "open %s error, return %d, %s", iname, errno, strerror(errno));
        ret = -errno;
        goto endOpen;
    }

    *pfd = fd;
    return ret;
    endOpen:
    return ret;
}

//open .fox to write data index info
static int fox_open_r_fd(struct foxDate * p, int *pfd, char* fname) {
    int fd;
    int ret = 0;

    pack_fname(fname, p);
    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {  // not such file.
            ret = errno;
            goto endOpen;
        }
        fprintf(stderr, "open %s error, return %d, %s", fname, errno, strerror(errno));
        ret = -ENOENT;
        goto endOpen;
    }

    *pfd = fd;
    return ret;
    endOpen:
    return ret;
}

int fox_setup_read(struct fox_manager* pman, struct foxDate * p, fox_time_t now) {
    int ret = 0;
    size_t isize = 0;

    ret = fox_open_r_fdi(p, &pman->fdi, pman->iname);    // for index file
    if (ret != 0) {
        goto endOpeni;
    }

    ret = fox_open_r_fd(p, &pman->fd, pman->fname);     // for data file
    if (ret != 0) {
        ret = pman->fd;
        goto endOpen;
    }

    isize = fd_size(pman->fdi);
    if (isize < 0) {
        ret = isize;
        goto endSize;
    }

    pman->year = p->year;
    pman->mon = p->mon;
    pman->mday = p->mday;
    pman->now = fox_read_init_now(pman);  // at begin of a day default.
    pman->isize = isize;
    pman->cells = isize / FOX_INDEX_SIZE;
    pman->r_index = 0;                    // at begin of the file.
    pman->r_next = pman->cells ? pman->r_index + 1 : 0;     // for empty file, next is 0

    ret = fox_cursor(pman->fdi, pman, now);
    if (ret < 0) {
        goto endCursor;
    }

    return ret;
    endCursor:
    endSize:
    close(pman->fd);
    endOpen:
    pman->fd = 0;
    close(pman->fdi);
    endOpeni:
    pman->fdi = 0;
    return ret;
}

static int fox_read_stream(int fd, char* stream, int size) {
    int ret;

    ret = read(fd, stream, size);
    if (ret < 0) {
        fprintf(stderr, "read file failed. %d, %s\n", errno, strerror(errno));
        goto endRead;
    }
    return ret;

    endRead:
    return ret;
}

// cursor move to next, < 0 bad, equal 0，>0 end.
int fox_next(struct fox_manager* pman, fox_time_t stop) {
    int ret = 0;

    if (pman->now <= stop && pman->r_index < pman->r_next) {  // r_index equal r_next  means is the end of file.
        struct fox_index index;

        ret = fox_cursor_next(pman, &index);  // the cursor will point to next position.
        if (ret < 0) {
            goto endNext;
        }
        ret = 0;

    } else {
        ret = 1;
    }
    return ret;

    endNext:
    return ret;
}

// read function
static int _fox_read_table(struct fox_manager* pman, char **pp, int *len) {
    int ret = 0;

    char *p = NULL;
    int size = 0;

    size = pman->table_len;
    ret = FOX_LSEEK(pman->fd, pman->data_pos, SEEK_SET);
    if (ret < 0) {
        fprintf(stderr, "lseek data fd %d pos: %lld, failed. %d, %s\n", pman->fd, pman->data_pos, errno, strerror(errno));
        ret = -EACCES;
        goto endSeek;
    }

    p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "malloc %d failed. %d, %s\n", size, errno, strerror(errno));
        ret = -ENOMEM;
        goto endMalloc;
    }

    ret = fox_read_stream(pman->fd, p, size);
    if (ret < 0) {
        free(p);
        goto endRead;
    }

    *pp  = p;
    *len = size;

    return ret;

    endRead:
    endMalloc:
    endSeek:
    *pp  = NULL;
    *len = 0;
    return ret;
}

int fox_read_table(struct fox_manager* pman, char **pp) {
    int ret;
    int len;

    ret = _fox_read_table(pman, pp, &len);
    if (ret < 0) {
        goto endWork;
    }
    return len;

    endWork:
    return ret;
}

// read function
static int _fox_read_data(struct fox_manager* pman, char **pp, int *len, fox_time_t *us) {
    int ret = 0;

    char *p = NULL;
    int size = 0;

    size = pman->data_len;
    ret = FOX_LSEEK(pman->fd, pman->data_pos + pman->table_len, SEEK_SET);
    if (ret < 0) {
        fprintf(stderr, "lseek data fd failed. %d, %s\n", errno, strerror(errno));
        ret = -EACCES;
        goto endSeek;
    }

    p = malloc(size);
    if (p == NULL) {
        fprintf(stderr, "malloc %d failed. %d, %s\n", size, errno, strerror(errno));
        ret = -ENOMEM;
        goto endMalloc;
    }

    ret = fox_read_stream(pman->fd, p, size);
    if (ret < 0) {
        free(p);
        goto endRead;
    }

    *pp  = p;
    *len = size;
    *us  = pman->now;
    return ret;

    endRead:
    endMalloc:
    endSeek:
    *pp  = NULL;
    *len = 0;
    return ret;
}

int fox_read_data(struct fox_manager* pman, char **pp, fox_time_t *us) {
    int ret = 0;
    int len = 0;

    ret = _fox_read_data(pman, pp, &len, us);
    if (ret < 0) {
        goto endWork;
    }
    return len;

    endWork:
    return ret;
}

void fox_free_buffer(char **pp) {
    char *p = *pp;
    free((void *)p);
}

void fox_del_man(struct fox_manager* pman) {
    if (pman->fd > 0) {
        posix_fadvise(pman->fd, 0, 0, POSIX_FADV_DONTNEED);
        close(pman->fd);
        pman->fd = 0;
    }
    if (pman->fdi > 0) {
        close(pman->fdi);
        pman->fdi = 0;
    }
}

int parse_sql(const char* sql, PgQueryParseResult* pRes) {
    *pRes = pg_query_parse(sql);
    return 0;
}

void parse_sql_free(PgQueryParseResult* pRes) {
    pg_query_free_parse_result(*pRes);
}

void parse_sql_exit() {
    pg_query_exit();
}