#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#define KVERSION 64
#define MAX_SUBCMD_ARGS 512
#define MAX_DEPEND_LEN 128
#define MAX_NAME_LEN 64
#define MAX_WORK_PATH_LEN 512
#define ERR_NOSUBTOOL 2
#define ERR_MISSARG 3
#define LINE_BUFF_LEN 1024

#define bool int
#define true 1
#define false 0

enum TOOL_TYPE {
    USER_TOOL,
    EXPERT_TOOL,
    MONITOR_TOOL,
    MAX_TOOL_TYPE
};

struct tool_info {
    char module[MAX_NAME_LEN];
    char name[MAX_NAME_LEN];
    char helpinfo[MAX_SUBCMD_ARGS];
};

struct tool_list_node {
    struct tool_list_node *next;
    struct tool_info tool;
};

struct tool_list {
    char *type_name;
    struct tool_list_node *head;
};

char *module = "/sysak.ko";
char *log_path="/var/log/sysak";
char *system_modules = "/proc/modules";
char *bin_path = "/usr/local/sbin";

char kern_version[KVERSION];
char machine[KVERSION];
char prev_dep[MAX_DEPEND_LEN];
char post_dep[MAX_DEPEND_LEN];
char run_depend[MAX_DEPEND_LEN]= {0};
char tools_path[MAX_WORK_PATH_LEN];
char tools_exec[MAX_WORK_PATH_LEN] = {0};
char sysak_rule[MAX_WORK_PATH_LEN];
char module_path[MAX_WORK_PATH_LEN];
char module_tag[MAX_NAME_LEN];
char sysak_work_path[MAX_WORK_PATH_LEN];
char sysak_other_rule[MAX_WORK_PATH_LEN];
char sysak_compoents_server[MAX_WORK_PATH_LEN];
char download_cmd[MAX_WORK_PATH_LEN];
bool pre_module = false;
bool post_module = false;
bool btf_depend = false;

static struct tool_list tool_lists[MAX_TOOL_TYPE]={
    {"sysak tools for user self-help analysis", NULL},
    {"sysak tools for system detail info", NULL},
    {"sysak monitor service", NULL}
};

static void usage(void)
{
    fprintf(stdout,
                "Usage: sysak [cmd] [subcmd [cmdargs]]\n"
                "       cmd:\n"
                "              list [-a], show subcmds\n"
                "              help, help information for specify subcmd\n"
                "       subcmd: see the result of list\n");
}

static void strim(char *str)
{
    char *pstr = str, *start = NULL;
    bool has_space = false;

    while (pstr && *pstr) {
       if (isspace(*pstr)) {
           *pstr = 0;
           has_space = true;
       }
       else if (!start) {
           start = pstr;
       }
       pstr++;
    }

    if (start && start != str)
       strcpy(str, start);
}

static void kern_release(void)
{
    struct utsname name;

    if (uname(&name) == -1) {
        printf("cannot get system version\n");
        return;
    }
    strncpy(kern_version, name.release, sizeof(name.release));
    strncpy(machine, name.machine, sizeof(name.machine));
}

static int mod_ctrl(bool enable)
{
    FILE *modlist_fp;
    char exec_mod[4*KVERSION];
    bool has_ko = false;
    char modinfo[MAX_SUBCMD_ARGS];
    int ret = -1;

    if (access(system_modules,0) != 0)
        return ret;

    modlist_fp = fopen(system_modules, "r");
    if (!modlist_fp) {
        printf("open %s failed\n", system_modules);
        return ret;
    }
    while(fgets(modinfo, sizeof(modinfo), modlist_fp))
    {
        if (strstr(modinfo, "sysak")) {
            has_ko = true;
            break;
        }

    }
    fclose(modlist_fp);

    if (enable && !has_ko) {
        snprintf(exec_mod, sizeof(exec_mod), "insmod %s%s%s", module_path, kern_version, module);
        ret = system(exec_mod);

    }
    else if (!enable && has_ko) {
        snprintf(exec_mod, sizeof(exec_mod), "rmmod %s%s%s", module_path, kern_version, module);
        ret = system(exec_mod);
    }

    return ret;
}

static bool get_server_addr(void)
{
    char filename[MAX_WORK_PATH_LEN];
    FILE *fp;

    sprintf(filename, "%s/sysak_server.conf", tools_path);
    fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "no sysak server config file\n");
        return false;
    }

    fgets(sysak_compoents_server, sizeof(sysak_compoents_server), fp);
    fclose(fp);

    strim(sysak_compoents_server);
    if (strlen(sysak_compoents_server) == 0) {
        fprintf(stderr, "no sysak server setting\n");
        return false;
    }

    return true;
}

static bool get_module_tag(void)
{
    FILE *fp;
    char filename[MAX_WORK_PATH_LEN];
    char buf[LINE_BUFF_LEN];
    char *pstr;

    sprintf(filename, "%s%s/.sysak.rules", module_path, kern_version);
    fp = fopen(filename, "r");
    if (!fp) {
        printf("open %s failed\n", filename);
        return false;
    }

    while(fgets(buf, sizeof(buf), fp)) {
        pstr = strstr(buf, "sysak_module_tag=");
        if (pstr) {
            pstr += strlen("sysak_module_tag=");
            strcpy(module_tag, pstr);
            strim(module_tag);
            if (strlen(module_tag) == 0)
               return false;
            return true;
        }
    }

    return false;
}

static int down_install_ext_tools(const char *tool)
{
    int ret;
    FILE *fp;
    char filename[MAX_WORK_PATH_LEN];
    char buf[LINE_BUFF_LEN];
    char rule[LINE_BUFF_LEN];
    char *pstr;

    sprintf(download_cmd, "wget %s/sysak/ext_tools/%s/%s/rule -P %s",
           sysak_compoents_server, machine, tool, tools_path);
    printf("%s ... \n", download_cmd);
    ret = system(download_cmd);
    if (ret < 0)
        return ret;

    sprintf(filename, "%s/rule", tools_path);
    fp = fopen(filename, "r");
    if (!fp)
        return -1;

    while(fgets(buf, sizeof(buf), fp)) {
        pstr = strstr(buf, "target=");
        if (pstr) {
            strcpy(filename, pstr + strlen("target="));
            continue;
        }

        pstr = strstr(buf, "info=");
        if (pstr) {
            strcpy(rule, pstr + strlen("info="));
            continue;
        }
    }
    fclose(fp);

    sprintf(download_cmd, "wget %s/sysak/ext_tools/%s/%s/%s -P %s",
            sysak_compoents_server, machine, tool, filename, tools_path);
    printf("%s ... \n", download_cmd);
    ret = system(download_cmd);
    if (ret < 0)
        return ret;

    /* extract files, only for zip now*/
    if (strstr(filename, ".zip")) {
        sprintf(buf, "unzip %s/%s -d %s\n", tools_path, filename, tools_path);
        printf("%s ... \n", buf);
        ret = system(buf);
        if (ret < 0)
            return ret;
    }

    fp = fopen(sysak_rule, "a");
    if (!fp)
        return -1;

    fputs(rule, fp);
    fclose(fp);
    return 0;
}

static int down_install(const char *compoent_name)
{
    if (!get_server_addr())
        return -1;

    if (strcmp(compoent_name, "sysak_modules") == 0) {
        if (!get_module_tag())
            return -1;
        sprintf(download_cmd, "wget %s/sysak/sysak_modules/%s/%s/sysak.ko -P %s/%s",
                sysak_compoents_server, machine, module_tag, module_path, kern_version);
        printf("%s ... \n", download_cmd);
        return system(download_cmd);
    } else if (strcmp(compoent_name, "btf") == 0) {
        sprintf(download_cmd, "wget %s/btf/%s/vmlinux-%s -P %s/%s",
                sysak_compoents_server, kern_version, tools_path, kern_version);
        printf("%s ... \n", download_cmd);
        return system(download_cmd);
    } else {
        return down_install_ext_tools(compoent_name);
    }
}

static int check_or_install_compoents(const char *name)
{
    char compents_path[MAX_WORK_PATH_LEN];
    const char *promt = "has not been installed, do you want to auto download and install ? Enter Y/N:";
    char user_input;
    int ret = 0;

    if (strcmp(name, "sysak_modules") == 0)
        sprintf(compents_path, "%s%s%s", module_path, kern_version, module);
    else if (strcmp(name, "btf") == 0)
        sprintf(compents_path, "%s%s/vmlinux-%s", tools_path, kern_version, kern_version);

    if (access(compents_path, 0) != 0) {
        printf("%s %s", name, promt);
        scanf("%c", &user_input);
        if (user_input == 'y' || user_input == 'Y') {
            ret = down_install(name);
            if (ret < 0 || access(compents_path, 0) != 0)
               ret = -EEXIST;
        } else {
            ret = -EEXIST;
        }
    }

    return ret;
}

static int do_prev_depend(void)
{
    if (pre_module) {
        if (!check_or_install_compoents("sysak_modules"))
            return mod_ctrl(true);
        printf("sysak_modules not installed, exit ...\n");
        return -1;
    }

    if (btf_depend)
        return check_or_install_compoents("btf");

    return 0;
}

static void add_python_depend(char *depend,char *cmd)
{
    if (!strcmp(depend, "all")) {
        snprintf(tools_exec, sizeof(tools_exec), "python2 %s", cmd);
    } else if (!strcmp(depend, "python3")) {
        snprintf(tools_exec, sizeof(tools_exec), "python3 %s", cmd);
    } else if(!strcmp(depend, "python2")) {
        snprintf(tools_exec, sizeof(tools_exec), "python2 %s", cmd);
    }
}

static int exectue(int argc, char *argv[])
{
    char subcmd_name[MAX_NAME_LEN+MAX_SUBCMD_ARGS];
    char subcmd_args[MAX_SUBCMD_ARGS];
    char subcmd_exec_final[MAX_NAME_LEN+MAX_SUBCMD_ARGS];
    int ret = 0;
    int i = 0;

    if (do_prev_depend() < 0)
        return -1;

    snprintf(subcmd_name, sizeof(subcmd_name), "%s%s", tools_path, argv[1]);

    if (access(subcmd_name, 0) != 0)
        snprintf(subcmd_name, sizeof(subcmd_name), "%s%s%s", tools_path, kern_version, argv[1]);

    for (i = 2; i <= (argc - 1); i++) {
        snprintf(subcmd_args, sizeof(subcmd_args), " \"%s\"", argv[i]);
        strcat(subcmd_name,subcmd_args);
    }

    if (run_depend[0])
        add_python_depend(run_depend, subcmd_name);
    else
        strncpy(tools_exec, subcmd_name, strlen(subcmd_name));

    snprintf(subcmd_exec_final, sizeof(subcmd_exec_final), "%s;%s", sysak_work_path, tools_exec);
    ret = system(subcmd_exec_final);

    if (post_module)
        mod_ctrl(false);

    return ret;
}

static void print_each_tool(bool all)
{
    int i, max_idx = all ? MONITOR_TOOL : USER_TOOL;
    struct tool_list_node *curr;
    char last_module[MAX_NAME_LEN] = {0};

    for (i = 0; i <= max_idx; i++) {
        if (!tool_lists[i].head)
            continue;

        printf("%s\n", tool_lists[i].type_name);
        curr = tool_lists[i].head;
        while (curr) {
            if(curr->tool.module[0] && strcmp(curr->tool.module, last_module)) {
                strncpy(last_module, curr->tool.module, MAX_NAME_LEN);
                printf("  %s:\n", curr->tool.module);
            }
            printf("    %-16s    %s\n", curr->tool.name, curr->tool.helpinfo);
            curr = curr->next;
        }
        printf("\n");
    }

    if (!all)
        printf("If you want to known the detail about the system, please use 'sysak list -a'.\n");
}

static int build_subcmd_info_from_file(FILE *fp, bool all)
{
    struct tool_list *list;
    struct tool_list_node *node;
    char buf[MAX_NAME_LEN + MAX_SUBCMD_ARGS], *sptr;
    char tools_name[MAX_NAME_LEN];
    char tools_class_module[MAX_NAME_LEN];
    int ret = 0;

    while(fgets(buf, sizeof(buf), fp))
    {
        sscanf(buf,"%[^:]:%[^:]",tools_class_module, tools_name);
        if (!all) {
            if (strcmp(tools_class_module, "combine"))
                continue;
        }

        node = malloc(sizeof(struct tool_list_node));
        if (!node) {
            fclose(fp);
            return -1;
        }

        memset(node, 0, sizeof(struct tool_list_node));
        if (strcmp(tools_class_module, "combine") == 0) {
            list = &tool_lists[USER_TOOL];
        } else if (strncmp(tools_class_module, "detect", 6) == 0) {
            list = &tool_lists[EXPERT_TOOL];
            sscanf(tools_class_module, "detect/%[^:]", node->tool.module);
        } else if (strncmp(tools_class_module, "monitor", 7) == 0) {
            list = &tool_lists[MONITOR_TOOL];
        } else {
            free(node);
            continue;
        }

        strcpy(node->tool.name, tools_name);
        sptr = strstr(buf, ":help{");
        if (sptr)
            sscanf(sptr, ":help{%[^}]}",node->tool.helpinfo);
        node->next = list->head;
        list->head = node;
        ret++;
    }

    fclose(fp);
    return ret;
}

static int build_subcmd_info(bool all)
{
    FILE *fp;
    int ret = 0;

    if (access(sysak_rule,0) != 0 && access(sysak_other_rule,0) != 0)
        return 0;

    fp = fopen(sysak_rule, "r");
    if (fp)
        ret += build_subcmd_info_from_file(fp, all);

    fp = fopen(sysak_other_rule, "r");
    if (fp)
        ret += build_subcmd_info_from_file(fp, all);

    return ret;
}

static void subcmd_list(bool show_all)
{
    if (build_subcmd_info(show_all) <= 0)
       return;

    print_each_tool(show_all);
}

static bool tool_rule_parse(char *path, char *tool)
{
    char *pstr = NULL;
    FILE *fp = NULL;
    char buf[MAX_NAME_LEN + MAX_SUBCMD_ARGS];

    if (access(path,0) != 0)
        return false;

    fp = fopen(path, "r");
    if (!fp) {
        printf("open %s failed\n", path);
        return false;
    }

    while(fgets(buf, sizeof(buf), fp))
    {
        char tools_name[MAX_NAME_LEN];

        pstr = buf;
        sscanf(buf,"%*[^:]:%[^:]",tools_name);
        if (strcmp(tools_name, tool)) {
            continue;
        }
        pstr = strstr(buf, ":prev{");
        if (pstr)
            sscanf(pstr, ":prev{%[^}]};post{%[^}]}", prev_dep, post_dep, run_depend);
        pstr = strstr(buf, ":python-dep{");
        if (pstr)
            sscanf(pstr, ":python-dep{%[^}]}", run_depend);

        fclose(fp);
        return true;
    }

    fclose(fp);
    return false;
}

static bool tool_lookup(char *tool)
{
    char tool_exec_file[MAX_WORK_PATH_LEN];

    snprintf(tool_exec_file, sizeof(tool_exec_file), "%s%s%s", tools_path, kern_version, tool);
    if (access(tool_exec_file, 0) != 0)
        snprintf(tool_exec_file, sizeof(tool_exec_file), "%s%s", tools_path, tool);

    if (access(tool_exec_file, 0) != 0) {
        if (down_install(tool) < 0)
            return false;
    }

    if (access(tool_exec_file, 0) != 0)
        return false;

    if (!tool_rule_parse(sysak_other_rule, tool) &&
            !tool_rule_parse(sysak_rule, tool))
        return false;

    return true;
}

static int subcmd_parse(int argc, char *argv[])
{
    int i;

    if (!tool_lookup(argv[1])) {
        printf("no components, you should get first\n");
        return -ERR_NOSUBTOOL;
    }

    if (strstr(prev_dep, "btf") != NULL) {
        btf_depend = true;
        goto exec;
    }

    if (strstr(prev_dep, "default") != NULL|| strstr(post_dep, "default") != NULL) {
        pre_module = true;
        post_module = true;
        goto exec;
    }

    for (i = 2; i <= (argc-1); i++)
    {
        if (strstr(prev_dep, argv[i])) {
            pre_module = true;
            break;
        }
        else if (strstr(post_dep, argv[i])) {
            post_module = true;
            break;
        }
    }
exec:
    return exectue(argc, argv);
}

static int parse_arg(int argc, char *argv[])
{
    bool show_all = false;

    if (argc < 2) {
        usage();
        return -ERR_MISSARG;
    }

    if (!strcmp(argv[1], "list")) {
        if (argc == 3 && !strcmp(argv[2], "-a"))
            show_all = true;

        subcmd_list(show_all);
        return 0;
    }

    if (!strcmp(argv[1], "help")) {
        usage();
        return 0;
    }
    return subcmd_parse(argc, argv);
}

char *dirpath(char *fullpath)
{
    char* substr = strrchr(fullpath, '/');
    char *ptr = fullpath;

    while(strcmp(substr, ptr))
        ptr++;

    *(ptr)='\0';
    return fullpath;
}

static void set_path(char *argv[])
{
    char compoents_path[MAX_WORK_PATH_LEN];
    char tmp[MAX_WORK_PATH_LEN];
    char *current_path;

    realpath(argv[0],tmp);
    current_path = dirpath(tmp);

    snprintf(compoents_path, sizeof(compoents_path), "%s%s",
             current_path, "/.sysak_compoents");

    if (access(compoents_path,0) != 0)
        snprintf(compoents_path, sizeof(tools_path), "%s%s",
            bin_path, "/.sysak_compoents");

    snprintf(tools_path, sizeof(tools_path), "%s%s",
             compoents_path, "/tools/");
    snprintf(module_path, sizeof(module_path), "%s%s",
             compoents_path, "/lib/");
    snprintf(sysak_rule, sizeof(sysak_rule), "%s%s",
             compoents_path, "/tools/.sysak.rules");
    snprintf(sysak_other_rule, sizeof(sysak_other_rule), "%s%s%s%s",
             compoents_path, "/tools/",kern_version,"/.sysak.rules");
    snprintf(sysak_work_path, sizeof(sysak_work_path), "%s%s",
             "export SYSAK_WORK_PATH=", compoents_path);
}

int main(int argc, char *argv[])
{
    int ret = 0;

    if (access(log_path,0) != 0)
        mkdir(log_path, 0755 );

    kern_release();
    set_path(argv);

    ret = parse_arg(argc, argv);
    return ret;
}
