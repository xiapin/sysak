//
// Created by 廖肇燕 on 2022/12/13.
//

#ifndef TINYINFO_FOXTSDB_H
#define TINYINFO_FOXTSDB_H

#include <unistd.h>
#include "pg_query.h"

typedef signed long fox_time_t;
typedef size_t fox_off_t;

#define FNAME_SIZE 16

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
    fox_off_t w_off;        // now write offset;
    fox_off_t r_index;      // now index
    fox_off_t r_next;       // for next index
    fox_off_t cells;        // index max
    size_t isize;        //index file size;
    size_t fsize;        //data file size.
    fox_off_t data_pos;     // data position
    int table_len;          // data table size
    int data_len;           // data size

    int fd;
    int fdi;
    char iname[FNAME_SIZE];  // info file name,
    char fname[FNAME_SIZE];  // data file name,
    int new_day;

    short year;
    char mon;
    char mday;
};

struct PgQueryError_t {
    char* message; // exception message
    char* funcname; // source function of exception (e.g. SearchSysCache)
    char* filename; // source of exception (e.g. parse.l)
    int lineno; // source of exception (e.g. 104)
    int cursorpos; // char in query at which exception occurred
    char* context; // additional context (optional, can be NULL)
};

struct PgQueryParseResult_t {
    char* parse_tree;
    char* stderr_buffer;
    struct PgQueryError_t* error;
};

fox_time_t get_us(void);
int get_date_from_us(fox_time_t us, struct foxDate * p);
int get_date(struct foxDate * p);
fox_time_t make_stamp(struct foxDate  * p);
int check_foxdate(struct foxDate* d1, struct foxDate* d2);
int check_pman_date(struct fox_manager* pman, struct foxDate* pdate);

int fox_setup_write(struct fox_manager* pman, struct foxDate * p, fox_time_t now);
int fox_write(struct fox_manager* pman, struct foxDate* pdate, fox_time_t us,
             const char* tData, int tLen,
             const char* data, int len);
int fox_setup_read(struct fox_manager* pman, struct foxDate * p, fox_time_t now);
int fox_cur_move(struct fox_manager* pman, fox_time_t now);
int fox_read_resize(struct fox_manager* pman);
int fox_next(struct fox_manager* pman, fox_time_t stop);
int fox_read_table(struct fox_manager* pman, char **pp);
int fox_read_data(struct fox_manager* pman, char **pp, fox_time_t *us);
void fox_free_buffer(char **pp);
void fox_del_man(struct fox_manager* pman);

int parse_sql(const char* sql, PgQueryParseResult* pRes);
void parse_sql_free(PgQueryParseResult* pRes);
void parse_sql_exit();

#endif //TINYINFO_FOXTSDB_H
