#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_CONF_FILE_PATH      "/usr/local/sysak/monctl.conf"
#define LEN_1024    1024
#define LEN_512 512
#define LEN_32  32
#define W_SPACE     " \t\r\n"
#define MAX_MOD_NUM 32

struct module {
    char name[LEN_32];
    int enable;
};

struct command {
    char name[LEN_32];
    char cmd[LEN_1024];
};

struct module *mod_list[MAX_MOD_NUM] = {0};
struct command *cmd_store[MAX_MOD_NUM] = {0};
int total_mod_num = 0;

static void usage(void)
{
	fprintf(stdout,
	        "Usage: sysak monctl \n"
            "Options:\n"
			"    -e         run monctl\n"
            "    -h			help info\n"
            "Usage step:\n"
            "1. enable mod in /usr/log/sysak/sysak_monctl.conf\n"
            "2. run sysak monctl, example: sysak monctl -e &\n");
	exit(EXIT_FAILURE);
}

static void parse_mod(char *mod_name)
{
    int i;
    struct module *mod;
    char *token;
    char *tool_name;

    tool_name = strstr(mod_name,"_");
    if (tool_name == '\0')
        return;
    printf("tool_name is %s\n",tool_name);


    token = strtok(NULL, W_SPACE);
    printf("token is %s\n",token);
    if (token && strcasecmp(token, "on") && strcasecmp(token, "enable")) {
        return;
    }

    printf("total_mod_num is %d\n",total_mod_num);
    /* check if the mod load already */
    for ( i = 0; i < total_mod_num; i++ )
    {
        mod = mod_list[i];
        printf("name is %s,i is %d\n",mod->name,i);
        if (!strcmp(mod->name, tool_name)) {
            return;
        }
    }

    mod = mod_list[total_mod_num++] = malloc(sizeof(struct module));
    if (mod == NULL) {
        printf("Failed to alloc memory for mod %s\n", mod_name);
        return;
    }
    memset(mod, '\0', sizeof(struct module));

    strncpy(mod->name, tool_name, LEN_32);
    mod->enable = 1;
}

static void parse_cmd(const char *cmd_name)
{
    int i;
    struct module *mod;
    struct command *cmd;
    char *token;
    char *tool_name;

    tool_name = strstr(cmd_name,"_");
    if (tool_name == '\0')
        return;

    for ( i = 0; i < total_mod_num; i++ )
    {
        mod = mod_list[i];
        printf("mod->name is %s,tool_name is %s\n",mod->name, tool_name);
        if (!strcmp(mod->name, tool_name)) {
            cmd = cmd_store[i] = malloc(sizeof(struct command));
            if (cmd == NULL){
                printf("Failed to alloc memory for cmd %s\n", cmd_name);
                return;
            }
            memset(cmd, '\0', sizeof(struct command));
            strncpy(cmd->name, tool_name, LEN_32);
            strncpy(cmd->cmd, cmd_name + strlen(cmd_name) + 1, LEN_512 - 1);
            cmd->cmd[LEN_512 - 1] = 0;
            printf("cmd is %s\n",cmd->cmd);
        }
    }
}

/* parse every config line */
static int parse_line(char *buff)
{
    char   *token;
    int     i;

    if ((token = strtok(buff, W_SPACE)) == NULL) {
        /* ignore empty lines */
        (void) 0;

    } else if (token[0] == '#') {
        /* ignore comment lines */
        (void) 0;

    } else if (strstr(token, "mod_")) {
        parse_mod(token);

    } else if (strstr(token, "cmd_")) {
        parse_cmd(token);

    } else {
        return 0;
    }
    return 1;
}

static void process_input_line(char *config_input_line, int len, const char *file_name)
{
    char *token;

    if ((token = strchr(config_input_line, '\n'))) {
        *token = '\0';
    }
    if ((token = strchr(config_input_line, '\r'))) {
        *token = '\0';
    }

    if (config_input_line[0] == '#') {
        goto final;
    } else if (config_input_line[0] == '\0') {
        goto final;
    }

    if (!parse_line(config_input_line)) {
        printf("parse_config_file: unknown keyword in '%s' at file %s\n",
                 config_input_line, file_name);
    }

final:
    memset(config_input_line, '\0', len);
}

static void parse_config_file(const char *file_name)
{
    FILE *fp;
    char config_input_line[LEN_1024] = {0};

    if (!(fp = fopen(file_name, "r"))) {
        printf("Unable to open configuration file: %s", file_name);
    }

    while (fgets(config_input_line, LEN_1024, fp)) {
        process_input_line(config_input_line, LEN_1024, file_name);
    }
    if (fclose(fp) < 0) {
        printf("fclose error:%s", strerror(errno));
    }
}

static void exec_command(void)
{
    int i;
    struct command *cmd;

    for ( i = 0; i < total_mod_num; i++ )
    {
        cmd = cmd_store[i];
        system(cmd->cmd);
    }
}

int main(int argc, char **argv)
{
    int opt;
	while ((opt = getopt(argc, argv, "he")) != -1) {
		switch (opt) {
			case 'h':
                usage();
                break;
            case 'e':
                parse_config_file(DEFAULT_CONF_FILE_PATH);
                exec_command();
                break;
        }
	}
    return 0;
}
