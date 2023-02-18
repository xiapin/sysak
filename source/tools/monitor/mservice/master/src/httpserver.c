#include "tsar.h"

#define SERVER_PORT 9200
#define ERROR_MSG "HTTP/1.1 404 Not Found\r\n"

enum {
	REQUEST_METRIC_ROOT,
	REQUEST_METRIC_CGROUP,
	REQUEST_METRIC_CGROUP_ALL,
	REQUEST_METRIC_ROOT_RAW,
	REQUEST_METRIC_CGROUP_RAW,
	REQUEST_METRIC_CGROUP_ALL_RAW,
	REQUEST_MAX
};

static int get_request(const char *buf, char *sub_req, int len)
{
	char req_str[LEN_256];

	sscanf(buf, "GET /%255s HTTP", req_str);
	if (strcmp(req_str, "metrics") == 0 || strcmp(req_str, "metrics/") == 0) {
		return REQUEST_METRIC_ROOT;
	}
	else if (strcmp(req_str, "metrics/cgroups") == 0 || strcmp(req_str, "metrics/cgroups/") == 0) {
		return REQUEST_METRIC_CGROUP_ALL;
	}
	else if (strncmp(req_str, "metrics/cgroups/", 15) == 0) {
		strncpy(sub_req, req_str + 15, len - 1);
		return REQUEST_METRIC_CGROUP;
	}
	else if (strcmp(req_str, "metrics/raw") == 0 || strcmp(req_str, "metrics/raw/") == 0) {
		return REQUEST_METRIC_ROOT_RAW;
	}
	else if (strcmp(req_str, "metrics/raw/cgroups") == 0 || strcmp(req_str, "metrics/raw/cgroups/") == 0) {
		return REQUEST_METRIC_CGROUP_ALL_RAW;
	}
	else if (strncmp(req_str, "metrics/raw/cgroups/", 19) == 0) {
		strncpy(sub_req, req_str + 19, len - 1);
		return REQUEST_METRIC_CGROUP_RAW;
	}
	return REQUEST_MAX;
}

int output_http(int sk, int req, const char*sub_req)
{
	int         i, j, k, n = 0;
	char        detail[LEN_1M] = {0};
	struct      module *mod;
	static char line[LEN_10M] = {0};
	char http_header[LEN_64];
	char opt_line[LEN_64];
	char *precord, *psub;
	U_64 *raw_array;
	double    *st_array;
	char label[32] = {0};

	line[0] = 0;

	for (i = 0; i < statis.total_mod_num; i++) {
		mod = mods[i];
		if (req != REQUEST_METRIC_ROOT) {
			if ((req == REQUEST_METRIC_CGROUP_ALL
				||req == REQUEST_METRIC_CGROUP
				||req == REQUEST_METRIC_CGROUP_ALL_RAW
				||req == REQUEST_METRIC_CGROUP_RAW )
				&& strcmp(mod->name, "mod_cgroup"))
				continue;
		}
		if (mod->enable && strlen(mod->record)) {
			precord = mod->record;
			j = 0;
			for (j = 0; j < mod->n_item; j++) {
				psub = strstr(precord, "=");
				if (psub) {
					int ignore = 0;
					*psub = 0;
					/*check if we want*/
					if ((req == REQUEST_METRIC_CGROUP || req == REQUEST_METRIC_CGROUP_RAW)
						 && strcmp(precord, sub_req))
						ignore = 1;
					snprintf(opt_line, LEN_64, "%s_", mod->opt_line+2);
					snprintf(label, sizeof(label), "{%s=\"%s\"}",mod->lable, precord);
					precord = strstr(psub + 1, ";");
					if (precord)
						precord = precord + 1;
					if (ignore)
						continue;
                                } else {
					memset(label, 0, sizeof(label));
                                        snprintf(opt_line, LEN_64, "%s_", mod->opt_line+2);
                                }
				if (req > REQUEST_METRIC_CGROUP_ALL && req < REQUEST_MAX)
					raw_array = &mod->cur_array[j * mod->n_col];
				else
                                	st_array = &mod->st_array[j * mod->n_col];
                                for (k = 0; k < mod->n_col; k++) {
					if (HIDE_BIT == mod->info[k].summary_bit)
						continue;

					if (req > REQUEST_METRIC_CGROUP_ALL && req < REQUEST_MAX)
                                        	n = snprintf(detail, LEN_1M, "%s%s%s %6llu\n",
							opt_line, trim(mod->info[k].hdr, LEN_128), label, raw_array[k]);
					else
                                        	n = snprintf(detail, LEN_1M, "%s%s%s %6.2f\n",
							opt_line, trim(mod->info[k].hdr, LEN_128), label, st_array[k]);
                                        if (n >= LEN_1M - 1) {
                                                do_debug(LOG_FATAL, "mod %s lenth is overflow %d\n", mod->name, n);
                                        }
                                        /* one for \n one for \0 */
                                        if (strlen(line) + strlen(detail) >= LEN_10M - 2) {
                                                do_debug(LOG_FATAL, "tsar.data line lenth is overflow line %d detail %d\n", strlen(line), strlen(detail));
                                        }
                                        strcat(line, detail);
                                }
                        }
                }
	}

	if (strlen(line) == 0)
		return -1;

	strcat(line, "\n");

	sprintf(http_header, "HTTP/1.1 200 OK\r\nContent-Length: %d \r\n\r\n",
			(int)strlen(line));
	write(sk, http_header, strlen(http_header));
	write(sk, line, strlen(line));
	return 0;
}


static int prev_collect_time;
static int handle_metric(int sk, int req, const char*sub_req)
{
	int ret;

	pthread_mutex_lock(&module_record_mutex);
	prev_collect_time = statis.cur_time;
	statis.cur_time = time(NULL);
	if (statis.cur_time - prev_collect_time > 60 || statis.cur_time <= prev_collect_time) {
		/*read twice for metrics which need compute diff*/
		collect_record();
		collect_record_stat();
		conf.print_interval = 1;
		sleep(1);
	} else {
		conf.print_interval = statis.cur_time - prev_collect_time;
	}

	collect_record();
	collect_record_stat();

	ret = output_http(sk, req, sub_req);
	pthread_mutex_unlock(&module_record_mutex);

	return ret;
}

static void handle_request(int sk)
{
	char buff[1024] = {0};
	char sub_req[LEN_256] = {0};
	int req;

	if (read(sk, buff, sizeof(buff)) <= 0)
		goto error;

	req = get_request(buff, sub_req, LEN_256);
	switch (req) {
		case REQUEST_METRIC_ROOT:
		case REQUEST_METRIC_CGROUP:
		case REQUEST_METRIC_CGROUP_ALL:
		case REQUEST_METRIC_ROOT_RAW:
		case REQUEST_METRIC_CGROUP_RAW:
		case REQUEST_METRIC_CGROUP_ALL_RAW:
			break;
		default:
			goto error;
	}

	if (handle_metric(sk, req, sub_req)) {
error:
		write(sk, ERROR_MSG, strlen(ERROR_MSG));
	}
}

int http_server(void)
{
	struct sockaddr_in srvaddr;
	int sk, res, sockopt = 1;

	sk = socket(AF_INET, SOCK_STREAM, 0);
	if(sk < 0) {
		printf("create socket error!\n");
		return -1;
	}

	srvaddr.sin_family = AF_INET;
	if (conf.server_port != '\0')
		srvaddr.sin_port = htons(conf.server_port);
	else
		srvaddr.sin_port = htons(SERVER_PORT);
	srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(int));

	res = bind(sk, (struct sockaddr *)&srvaddr, sizeof(srvaddr));
	if(res < 0) {
		printf("bind error!\n");
		close(sk);
		return -1;
	}

	listen(sk, 10);

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t len = sizeof(cli_addr);
		int csk;

		csk = accept(sk, (struct sockaddr *)&cli_addr, &len);
		if(csk < 0) {
			printf("accept error!\n");
			close(sk);
			return -1;
		}
		handle_request(csk);
	        close(csk);
    }

    close(sk);

    return 0;
}
