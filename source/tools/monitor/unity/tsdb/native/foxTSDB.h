//
// Created by 廖肇燕 on 2022/12/13.
//

#ifndef TINYINFO_FOXTSDB_H
#define TINYINFO_FOXTSDB_H

#include <unistd.h>

typedef unsigned long fox_time_t;

struct foxDate {
    short year;
    char mon;
    char mday;
    char hour;
    char min;
    char sec;
};

struct fox_manager {
    fox_time_t now;
    off_t pos;          //now offset;
    off_t last_pos;     // last pos
    size_t fsize;        // file size.
    int fd;

    short year;
    char mon;
    char mday;
};

fox_time_t get_us(void);
int get_date_from_us(fox_time_t us, struct foxDate * p);
int get_date(struct foxDate * p);
fox_time_t make_stamp(struct foxDate  * p);
int check_foxdate(struct foxDate* d1, struct foxDate* d2);

int fox_setup_write(struct fox_manager* pman, struct foxDate * p, fox_time_t now);
int fox_write(struct fox_manager* pman, struct foxDate* pdate, fox_time_t us,
             const char* data, int len);
int fox_setup_read(struct fox_manager* pman, struct foxDate * p, fox_time_t now);
int fox_cur_move(struct fox_manager* pman, fox_time_t now);
int fox_read_resize(struct fox_manager* pman);
int fox_read(struct fox_manager* pman, fox_time_t stop, char **pp, fox_time_t *us);
void fox_free_buffer(char **pp);
void fox_del_man(struct fox_manager* pman);

#endif //TINYINFO_FOXTSDB_H
