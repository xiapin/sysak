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

#define FNAME_SIZE 16
#define FOX_MAGIC   0xf030
#define MICRO_UNIT (1000 * 1000UL)

#define FOX_VALUE_FLAG  (1 << 0ULL)
#define FOX_LOG_FLAG    (1 << 1ULL)

struct fox_head{
    unsigned int prev;
    unsigned int next;
    fox_time_t t_us;
    unsigned short magic;
    unsigned short flag;
};

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
    ptm->tm_isdst = -1;
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

static size_t fd_size(int fd) {
    int ret;
    struct stat file_info;

    ret = fstat(fd, &file_info);
    if (ret < 0) {
        fprintf(stderr, "stat file failed. %d, %s\n", errno, strerror(errno));
    }
    return (!ret) ? file_info.st_size : -EACCES;
}

static void pack_fname(char* pname, struct foxDate * p) {
    snprintf(pname, FNAME_SIZE, "%04d%02d%02d.fox", p->year, p->mon, p->mday);
}

static int fox_read_head(int fd, struct fox_head* phead) {
    size_t size;

    size = read(fd, phead, sizeof (struct fox_head));

    if (size == sizeof (struct fox_head)) {
        if (phead->magic == FOX_MAGIC) {
            return size;
        } else {
            fprintf(stderr, "bad magic: 0x%x, hope: 0x%x\n", phead->magic, FOX_MAGIC);
            return -EINVAL;
        }
    } else if (size == 0) {
        return 0;
    } else {
        fprintf(stderr, "read file failed. %d, %s\n", errno, strerror(errno));
        return -EACCES;
    }
}

static int fox_check_head(int fd, struct fox_manager* pman) {
    int ret;
    struct fox_head head;
    off_t off = 0;

    head.prev = 0;
    while (1) {
        off = lseek(fd, off, SEEK_SET);
        if (off < 0) {
            fprintf(stderr, "seek file failed. %d, %s\n", errno, strerror(errno));
            ret = -EACCES;
            goto endSeek;
        }

        ret = fox_read_head(fd, &head);
        if (ret > 0) {
            off = head.next;
        } else if (ret == 0) {
            pman->last_pos = head.prev;
            pman->pos = off;
            break;
        } else {
            fprintf(stderr, "write file failed. pos: 0x%llx\n", off);
            goto endHead;
        }
    }

    return ret;
    endHead:
    endSeek:
    return ret;
}

static int fox_check(int fd, struct fox_manager* pman) {
    int ret = 0;
    int retLock = 0;

    ret = lockf(fd, F_LOCK, 0);
    if (ret < 0) {
        fprintf(stderr, "lock file failed. %d, %s\n", errno, strerror(errno));
        goto endLock;
    }

    ret = fox_check_head(fd, pman);
    if (ret < 0) {
        goto endCheck;
    }

    ret = lockf(fd, F_ULOCK, 0);
    if (ret < 0) {
        fprintf(stderr, "lock file failed. %d, %s\n", errno, strerror(errno));
        goto endUnLock;
    }

    return ret;

    endCheck:
    retLock = lockf(fd, F_ULOCK, 0);
    if (retLock < 0) {
        fprintf(stderr, "lock file failed. %d, %s\n", errno, strerror(errno));
        ret = retLock;
        goto endUnLock;
    }
    return ret;
    endUnLock:
    endLock:
    return ret;
}

int fox_setup_write(struct fox_manager* pman, struct foxDate * p, fox_time_t now) {
    char fname[FNAME_SIZE];
    int ret = 0;

    pack_fname(fname, p);

    pman->fd = open(fname, O_RDWR|O_APPEND|O_CREAT);
    if (pman->fd < 0) {
        fprintf(stderr, "open %s error, return %d, %s", fname, errno, strerror(errno));
        ret = -ENOENT;
        goto endOpen;
    }

    ret = fox_check(pman->fd, pman);
    if (ret < 0) {
        goto endCheck;
    }

    pman->year = p->year;
    pman->mon  = p->mon;
    pman->mday = p->mday;
    pman->now = now;

    return ret;
    endCheck:
    close(pman->fd);
    pman->fd = 0;
    endOpen:
    return ret;
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

static int fox_write_data(struct fox_manager* pman, fox_time_t us, const char* data, int len) {
    int ret = 0;
    int fd = pman->fd;
    struct fox_head head;

    head.prev = pman->last_pos;
    head.next = pman->pos + sizeof (struct fox_head) + len;
    head.t_us = us;
    head.magic = FOX_MAGIC;
    head.flag = 0;

    ret = lockf(fd, F_LOCK, 0);
    if (ret < 0) {
        fprintf(stderr, "lock file failed. %d, %s\n", errno, strerror(errno));
        ret = -EACCES;
        goto endLock;
    }

    ret = write(fd, &head, sizeof (struct fox_head));
    if (ret < 0) {
        fprintf(stderr, "write file failed. %d, %s\n", errno, strerror(errno));
        goto endWrite;
    }
    ret = write(fd, data, len );
    if (ret < 0) {
        fprintf(stderr, "write file failed. %d, %s\n", errno, strerror(errno));
        goto endWrite;
    }

    ret = lockf(fd, F_ULOCK, 0);
    if (ret < 0) {
        fprintf(stderr, "lock file failed. %d, %s\n", errno, strerror(errno));
        ret = -EACCES;
        goto endUnLock;
    }

    pman->now = us;
    pman->last_pos = pman->pos;   // record last position
    pman->pos = head.next;
    return  ret;

    endWrite:
    ret = lockf(fd, F_ULOCK, 0);
    if (ret < 0) {
        fprintf(stderr, "lock file failed. %d, %s\n", errno, strerror(errno));
        ret = -EACCES;
        goto endUnLock;
    }
    return ret;
    endUnLock:
    endLock:
    return ret;
}

int fox_write(struct fox_manager* pman, struct foxDate* pdate, fox_time_t us,
          const char* data, int len) {
    int res = 0;

    if (!check_pman_date(pman, pdate)) {  // new day?
        close(pman->fd);   // close this file at first.
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
        res = fox_write_data(pman, us, data, len);
    }
    return  res;

    endCreateFile:
    return res;
}

static int fox_cursor_left(int fd, struct fox_manager* pman, fox_time_t now) {
    int ret;
    off_t pos;
    struct fox_head head;
    fox_time_t last_t_us = pman->now;

    if (pman->pos == pman->fsize) {
        pos = pman->last_pos;
    } else {
        pos = pman->pos;
    }

    while (1) {
        ret = lseek(fd, pos, SEEK_SET);
        if (ret < 0) {
            fprintf(stderr, "seek file failed. %d, %s\n", errno, strerror(errno));
            ret = -EACCES;
            goto endSeek;
        }

        ret = fox_read_head(fd, &head);;
        if (ret < 0) {
            goto endRead;
        }

        if (head.t_us < now) {
            pman->pos = head.next;
            pman->now = last_t_us;
            pman->last_pos = pos;
            break;
        }

        last_t_us = head.t_us;
        pos = head.prev;
        if (pos == 0) {     //begin of file
            pman->pos = pos;
            pman->now = last_t_us;
            pman->last_pos = pos;
            break;
        }
    }
    ret = 0;
    return ret;
    endSeek:
    endRead:
    return ret;
}

static int fox_cursor_right(int fd, struct fox_manager* pman, fox_time_t now) {
    int ret;
    off_t last_pos = pman->last_pos;
    off_t pos = pman->pos;
    struct fox_head head;

    while (1) {
        if (pos == pman->fsize) {
            pman->last_pos = last_pos;
            pman->pos = pos;
            pman->now = fox_read_day_max(pman);
            ret = 0;
            goto endEndoffile;
        } else if (pos > pman->fsize) {
            ret = -ERANGE;
            goto endRange;
        }

        ret = lseek(fd, pos, SEEK_SET);
        if (ret < 0) {
            fprintf(stderr, "seek file failed. %d, %s\n", errno, strerror(errno));
            ret = -EACCES;
            goto endSeek;
        }

        ret = fox_read_head(fd, &head);;
        if (ret < 0) {
            goto endRead;
        }

        if (head.t_us >= now) {  // just >
            pman->pos = pos;
            pman->now = head.t_us;
            pman->last_pos = last_pos;
            break;
        }
        last_pos = pos;
        pos = head.next;
    }
    ret = 0;
    return ret;
    endEndoffile:
    return ret;
    endRange:  //out of file range.
    return ret;
    endSeek:
    endRead:
    endSize:
    return ret;
}

static int fox_cursor_work(int fd, struct fox_manager* pman, fox_time_t now) {
    int ret = 0;

    if (pman->now > now) {
        ret = fox_cursor_left(fd, pman, now);
        if (ret < 0) {
            goto endCursor;
        }
    } else if (pman->now < now) {
        ret = fox_cursor_right(fd, pman, now);
        if (ret < 0) {
            goto endCursor;
        }
    }

    return ret;
    endCursor:
    return ret;
}

static int fox_cursor(int fd, struct fox_manager* pman, fox_time_t now) {
    int ret;

    ret = fox_cursor_work(fd, pman, now);
    if (ret < 0) {
        goto endLock;
    }

    return ret;
    endLock:
    return ret;
}

int fox_cur_move(struct fox_manager* pman, fox_time_t now) {
    int fd;

    fd = pman->fd;

    return fox_cursor(fd, pman, now);
}

static int fox_cur_next(struct fox_manager* pman, struct fox_head* phead) {
    int ret = 0;
    off_t pos = phead->next;
    struct fox_head head;
    int fd = pman->fd;

    pman->pos = pos;
    if (pos < pman->fsize) {
        ret = lseek(fd, pos, SEEK_SET);
        if (ret < 0) {
            fprintf(stderr, "seek file failed. %d, %s\n", errno, strerror(errno));
            goto endSeek;
        }

        ret = fox_read_head(fd, &head);;
        if (ret < 0) {
            goto endRead;
        }
        pman->now = head.t_us;
    } else {
        pman->now = fox_read_day_max(pman);
    }

    return ret;
    endSeek:
    endRead:
    return ret;
}

static int fox_cur_back(struct fox_manager* pman) {
    int ret = 0;
    off_t pos = pman->last_pos;
    struct fox_head head;
    int fd = pman->fd;

    pman->pos = pos;
    if (pos > 0) {
        ret = lseek(fd, pos, SEEK_SET);
        if (ret < 0) {
            fprintf(stderr, "seek file failed. %d, %s\n", errno, strerror(errno));
            goto endSeek;
        }

        ret = fox_read_head(fd, &head);;
        if (ret < 0) {
            goto endRead;
        }
        pman->now = head.t_us;
    } else {
        pman->now = fox_read_init_now(pman);
    }

    return ret;
    endSeek:
    endRead:
    return ret;
}

int fox_read_resize(struct fox_manager* pman) {
    int ret = 0;

    size_t fsize = fd_size(pman->fd);
    if (fsize < 0) {
        ret = fsize;
        goto endSize;
    }

    if (fsize > pman->fsize) {
        if (pman->pos == pman->fsize) {  // at the end of file.
            ret = fox_cur_back(pman);
            if (ret < 0) {
                goto endCur;
            }
        }
        pman->fsize = fsize;
    } else {
        struct foxDate d;
        d.year = pman->year;
        d.mon  = pman->mon;
        d.mday = pman->mday;
        d.hour = 0;
        d.min  = 0;
        d.sec  = 0;

        fox_time_t now = make_stamp(&d);

        ret = fox_setup_read(pman, &d, now);
        if (ret !=0 ) {
            ret = -EACCES;
            goto endSetup;
        }
    }
    return ret;
    endSetup:
    endSize:
    endCur:
    return ret;
}

int fox_setup_read(struct fox_manager* pman, struct foxDate * p, fox_time_t now) {
    char fname[FNAME_SIZE];
    int ret = 0;

    pack_fname(fname, p);
    pman->fd = open(fname, O_RDONLY);
    if (pman->fd < 0) {
        fprintf(stderr, "open %s failed, return %d, %s", fname, errno, strerror(errno));
        ret = 1;
        goto endOpen;
    }

    pman->year = p->year;
    pman->mon = p->mon;
    pman->mday = p->mday;
    pman->now = fox_read_init_now(pman);
    pman->pos = 0;
    pman->last_pos = 0;
    pman->fsize = fd_size(pman->fd);
    if (pman->fsize < 0) {
        ret = -EACCES;
        goto endSize;
    }

    ret = fox_cursor(pman->fd, pman, now);
    if (ret < 0) {
        goto endCursor;
    }

//    printf("setup %s, fd: %d, size: %ld, pos: %lld, pnow: %ld, now:%ld\n",
//           fname, pman->fd, pman->fsize, pman->pos, pman->now, now);
    return ret;
    endSize:
    endCursor:
    close(pman->fd);
    endOpen:
    pman->fd = -1;
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

static int fox_read_work(int fd, struct fox_manager* pman, fox_time_t stop,
        char **pp, int *len, fox_time_t *us) {
    int ret = 0;
    off_t pos = pman->pos;
    int size;

    if (pman->now <= stop) {
        struct fox_head head;
        char *p;

        ret = lseek(fd, pos, SEEK_SET);
        if (ret < 0) {
            fprintf(stderr, "seek file failed. %d, %s\n", errno, strerror(errno));
            ret = -EACCES;
            goto endSeek;
        }

        ret = fox_read_head(fd, &head);
        if (ret < 0) {
            goto endRead;
        }

        size = head.next - pos - sizeof (struct fox_head);
        if (size < 0) {
            goto endSize;
        }
        p = malloc(size);
        if (p == NULL) {
            fprintf(stderr, "malloc %d failed. %d, %s\n", size, errno, strerror(errno));
            ret = -ENOMEM;
            goto endMalloc;
        }

        ret = fox_read_stream(fd, p, size);
        if (ret < 0) {
            free(p);
            goto endRead2;
        }

        ret = fox_cur_next(pman, &head);  // the cursor will to next position.
        if (ret < 0) {
            free(p);
            goto endNext;
        }

        *us  = head.t_us;
        *pp  = p;
        *len = size;
    } else {
        *pp  = NULL;
        *len = 0;
    }
    return ret;

    endNext:
    endRead2:
    endSize:
    endMalloc:
    endRead:
    endSeek:
    *pp  = NULL;
    *len = 0;
    return ret;
}

int fox_read(struct fox_manager* pman, fox_time_t stop, char **pp, fox_time_t *us) {
    int ret;
    int len;
    int fd = pman->fd;

    ret = fox_read_work(fd, pman, stop, pp, &len, us);
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
        pman->fd = -1;
    }
}
