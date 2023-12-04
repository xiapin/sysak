#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define KVERSION 64
#define MAX_SUBCMD_ARGS 512
#define MAX_DEPEND_LEN 128
#define DEFAULT_LEN 128
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
    BCLINUX_TOOL,
    MAX_TOOL_TYPE
};

struct diagnose_info
{
    char default_cmd[MAX_SUBCMD_ARGS];
    char cmd[MAX_SUBCMD_ARGS];
    char name[MAX_NAME_LEN];
    char helpinfo[MAX_SUBCMD_ARGS];
};

struct diagnose_tool
{
    struct diagnose_info tool;
    struct diagnose_tool *next;
};

struct diagnose_menu_node
{
    struct diagnose_menu_node *next;
    struct diagnose_tool *diagnose_tool_head;
    char diagnose_menu_name[MAX_NAME_LEN];
};

struct tool_info
{
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
char *log_path = "/var/log/sysak";
char *system_modules = "/proc/modules";
char *sysak_root_path = "/usr/local/sysak";
char *python_bin = "/usr/bin/python";
char *python2_bin = "/usr/bin/python2";
char *python3_bin = "/usr/bin/python3";

char kern_version[KVERSION];
char machine[KVERSION];
char prev_dep[MAX_DEPEND_LEN];
char post_dep[MAX_DEPEND_LEN];
char run_depend[MAX_DEPEND_LEN] = {0};
char tools_path[MAX_WORK_PATH_LEN];
char tools_exec[MAX_WORK_PATH_LEN] = {0};
char sysak_rule[MAX_WORK_PATH_LEN];
char module_path[MAX_WORK_PATH_LEN];
char module_tag[MAX_NAME_LEN];
char sysak_work_path[MAX_WORK_PATH_LEN];
char sysak_other_rule[MAX_WORK_PATH_LEN];
char sysak_components_server[MAX_WORK_PATH_LEN];
char sysak_oss_server[MAX_WORK_PATH_LEN];
char download_cmd[MAX_WORK_PATH_LEN];
char region[MAX_NAME_LEN] = {0};
char sysak_diagnose_rule[MAX_WORK_PATH_LEN];

bool pre_module = false;
bool post_module = false;
bool btf_depend = false;
bool auto_get_components = false;
bool oss_get_components = false;
bool only_download = false;
pid_t child_pid;

static struct tool_list tool_lists[MAX_TOOL_TYPE]={
    {"sysak tools for user self-help analysis", NULL},
    {"sysak tools for system detail info", NULL},
    {"sysak monitor service", NULL},
    {"sysak tools for bclinux", NULL}
};

static struct diagnose_menu_node *diag_list_head, *diag_menu_choose;
static struct diagnose_tool *diag_tool_choose = NULL;

static void usage(void)
{
    fprintf(stdout,
            "Usage: sysak [cmd] [subcmd [cmdargs]]\n"
            "       cmd:\n"
            "              list [-a],   show subcmds\n"
            "              -h/help,     help informati on for specify subcmd\n"
            "              -g,          auto download btf and components from anolis mirrors\n"
            "              -oss,        auto download btf and components from oss\n"
            "              -d,          only download btf and components. example: sysak -oss -d\n"
            "              -c,          show diagnosis center\n"
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

    if (enable) {
        if(has_ko) {
            ret = 0;
        }
        else {
            snprintf(exec_mod, sizeof(exec_mod), "insmod %s%s%s", module_path, kern_version, module);
            ret = system(exec_mod);
        }
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

    sprintf(filename, "%s/sysak_server.conf", sysak_root_path);
    fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "no sysak server config file\n");
        return false;
    }

    fgets(sysak_components_server, sizeof(sysak_components_server), fp);
    fgets(sysak_oss_server, sizeof(sysak_oss_server), fp);
    fclose(fp);

    strim(sysak_components_server);
    strim(sysak_oss_server);
    if (strlen(sysak_components_server) == 0 || strlen(sysak_oss_server) == 0) {
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

    sprintf(filename, "%s/.sysak.rules", module_path);
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

static bool get_region(char *region)
{
    FILE *res;
    const char *region_cmd = "curl -s --connect-timeout 2 http://100.100.100.200/latest/meta-data/region-id 2>&1";

    res = popen(region_cmd, "r");
    if (res == NULL) {
        printf("get region id failed\n");
        return -1;
    }

    if (feof(res)) {
        printf("cmd line end\n");
        return 0;
    }
    fread(region, 1,512, res);
    pclose(res);
    return 0;
}

static int down_install_ext_tools(const char *tool)
{
    int ret;
    FILE *fp;
    char filename[MAX_WORK_PATH_LEN];
    char buf[LINE_BUFF_LEN];
    char rule[LINE_BUFF_LEN];
    char *pstr;

    sprintf(download_cmd, "wget %s/sysak/ext_tools/%s/%s/rule -P %s &>/dev/null",
            sysak_components_server, machine, tool, tools_path);
    //printf("%s ... \n", download_cmd);
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

    strim(filename);

    if (strlen(filename) == 0) {
        printf("invalid ext tool\n");
        return -1;
    }

    sprintf(download_cmd, "wget %s/sysak/ext_tools/%s/%s/%s -P %s &>/dev/null",
            sysak_components_server, machine, tool, filename, tools_path);
    //printf("%s ... \n", download_cmd);
    ret = system(download_cmd);
    if (ret < 0)
        return ret;

    /* extract files, only for zip and tar now*/
    if (strstr(filename, ".zip")) {
        sprintf(buf, "unzip %s/%s -d %s\n", tools_path, filename, tools_path);
        //printf("%s ... \n", buf);
        ret = system(buf);
        if (ret < 0)
            return ret;
    } else if (strstr(filename, ".tar")) {
        sprintf(buf, "tar xf %s/%s -C %s\n", tools_path, filename, tools_path);
        //printf("%s ... \n", buf);
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

static int down_install(const char *component_name)
{
    char ko_path[MAX_WORK_PATH_LEN];
    char ko_file[MAX_WORK_PATH_LEN];
    char btf_file[MAX_WORK_PATH_LEN];
    int retry_cnt = 0;
    int ret = 0;

    sprintf(ko_path, "%s/%s", module_path, kern_version);
    sprintf(ko_file, "%s/%s/%s", module_path, kern_version, "sysak.ko");
    sprintf(btf_file, "%s/vmlinux-%s", tools_path, kern_version);

    if (!get_server_addr())
        return -1;

    if (strcmp(component_name, "sysak_modules") == 0) {
        if (!get_module_tag())
            return -1;

        if (access(ko_path,0) != 0)
            mkdir(ko_path, 0755 );

        //sprintf(download_cmd, "wget %s/sysak/sysak_modules/%s/%s/sysak.ko -P %s/%s 1&>/dev/null",
        //        sysak_components_server, machine, module_tag, module_path, kern_version);
        if (oss_get_components){
retry_ko_oss:
            sprintf(download_cmd, "wget -T 5 -t 2 -q -O %s/%s/sysak.ko %s-%s.oss-%s-internal.aliyuncs.com/home/hive/sysak/modules/%s/sysak-%s.ko",
                    module_path, kern_version, sysak_oss_server, &region[0], &region[0], machine, kern_version);
        }
        else
retry_ko:
            sprintf(download_cmd, "wget -T 5 -t 2 -q %s/sysak/modules/%s/sysak-%s.ko -O %s/%s/sysak.ko &>/dev/null",
                    sysak_components_server, machine, kern_version, module_path, kern_version);
        //printf("%s ... \n", download_cmd);

        ret = system(download_cmd);
        if (access(ko_file,0) == 0)
            ret = 0;
        else if (retry_cnt == 0){
            if (oss_get_components){
                retry_cnt++;
                goto retry_ko;
            }
            else {
                retry_cnt++;
                goto retry_ko_oss;
            }
        }
        return ret;

    } else if (strcmp(component_name, "btf") == 0) {
	    //sprintf(download_cmd, "wget %s/coolbpf/btf/%s/vmlinux-%s -P %s/%s 1&>/dev/null",
        //       sysak_components_server, machine, kern_version, tools_path, kern_version);
        if (oss_get_components){
retry_btf_oss:
            sprintf(download_cmd, "wget -T 5 -t 2 -q -P %s %s-%s.oss-%s-internal.aliyuncs.com/home/hive/btf/%s/vmlinux-%s",
                    tools_path, sysak_oss_server, &region[0], &region[0], machine, kern_version);
        }
        else
retry_btf:
            sprintf(download_cmd, "wget -T 5 -t 2 -q %s/coolbpf/btf/%s/vmlinux-%s -P %s &>/dev/null",
                    sysak_components_server, machine, kern_version, tools_path);
        //printf("%s ... \n", download_cmd);
        ret = system(download_cmd);
        if (access(btf_file,0) == 0)
            ret = 0;
        else if (retry_cnt == 0){
            if (oss_get_components){
                retry_cnt++;
                goto retry_btf;
            }
            else {
                retry_cnt++;
                goto retry_btf_oss;
            }
        }
        return ret;
    } else {
        return down_install_ext_tools(component_name);
    }
}

static int pre_down_install(const char *module, const char *btf, const char *compents)
{
    bool download = false;
    int ret = 0;
    char user_input = ' ';
    char *btf_name = "btf";
    char *module_name = "sysak_modules";
    const char *promt = "has not been installed, do you want to auto download and install ? Enter Y/N:";

    if (auto_get_components){
        download = true;
    }else{
        if (module && btf)
            printf("%s and %s %s", module_name, btf_name, promt);
        else if (module)
            printf("%s %s", module_name, promt);
        else if (btf)
            printf("%s %s", btf_name, promt);
        else
            printf("%s %s", compents, promt);
        scanf("%c", &user_input);

        if (user_input == 'y' || user_input == 'Y')
            download = true;
    }

    if (download) {
        if (module)
            ret = down_install(module_name);
        if (btf)
            ret = down_install(btf_name);
        if (compents)
            ret = down_install(compents);
        if (ret < 0)
            ret = -EEXIST;
    } else {
        ret = -EEXIST;
    }
    return ret;
}

static int check_or_install_components(const char *name)
{
    char compents_path[MAX_WORK_PATH_LEN];
    char ko_path[MAX_WORK_PATH_LEN];
    char btf_path[MAX_WORK_PATH_LEN];
    char *need_module = NULL, *need_btf = NULL;
    bool need_compents = false;
    int ret = 0;
    bool download = false;

    need_module = strstr(name, "sysak_modules");
    need_btf = strstr(name, "btf");

    if (need_module || need_btf) {
        if (need_module)
            sprintf(ko_path, "%s%s%s", module_path, kern_version, module);
        if (need_btf)
            sprintf(btf_path, "%s/vmlinux-%s", tools_path, kern_version);
    } else {
        sprintf(compents_path, "%s%s", tools_path, name);
        need_compents = true;
    }

    if (access(ko_path, 0) == 0)
        need_module = NULL;
    if (access(btf_path, 0) == 0)
        need_btf = NULL;

    get_region(region);

    if (need_module && need_btf){
        ret = pre_down_install(need_module, need_btf, NULL);
    } else if (need_module) {
        ret = pre_down_install(need_module, NULL, NULL);
    } else if (need_btf){
        ret = pre_down_install(NULL, need_btf, NULL);
    } else if (need_compents){
        ret = pre_down_install(NULL, NULL, name);
    }
    return ret;
}

static int do_prev_depend(void)
{
    if (pre_module && btf_depend)
        return check_or_install_components("sysak_modules and btf");

    if (pre_module) {
        if (!check_or_install_components("sysak_modules")){
            if (!only_download){
                return mod_ctrl(true);
            }
        } else{
            printf("sysak_modules not installed, exit ...\n");
        }
        return -1;
    }

    if (btf_depend)
        return check_or_install_components("btf");

    return 0;
}

static void add_python_depend(char *depend,char *cmd)
{
    if (!strcmp(depend, "all")) {
        if (!access(python_bin,0))
            snprintf(tools_exec, sizeof(tools_exec), "python %s", cmd);
        else if (!access(python2_bin,0))
            snprintf(tools_exec, sizeof(tools_exec), "python2 %s", cmd);
        else if (!access(python3_bin,0))
            snprintf(tools_exec, sizeof(tools_exec), "python3 %s", cmd);
        else
            printf("please install python!\n");
    } else if (!strcmp(depend, "python3")) {
        snprintf(tools_exec, sizeof(tools_exec), "python3 %s", cmd);
    } else if(!strcmp(depend, "python2")) {
        snprintf(tools_exec, sizeof(tools_exec), "python2 %s", cmd);
    }
}

static void sig_handler(int sig, siginfo_t *info, void *act)
{
    if (child_pid > 0 )
        kill(-child_pid, sig);
    if (post_module && !only_download)
        mod_ctrl(false);
}

static int register_sig_handler(pid_t pid)
{
    struct sigaction act;
    int ret = -1;

    sigemptyset(&act.sa_mask);
    act.sa_flags=SA_SIGINFO;
    act.sa_sigaction=sig_handler;

    if (sigaction(SIGINT, &act, NULL) == 0)
        ret = 0;
    if (sigaction(SIGQUIT, &act, NULL) == 0)
        ret = 0;
    if (sigaction(SIGTERM, &act, NULL) == 0)
        ret = 0;
    if (!ret)
        child_pid = pid;

    return 0;
}

static int my_system(char *cmd)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        return -1;
    } else if (pid == 0) {
        execl("/usr/bin/sh", "sh", "-c", cmd, NULL);
        exit(0);
    }

    register_sig_handler(pid);
    waitpid(pid, &status, 0);
    return 0;
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

    if (only_download)
        return 0;

    snprintf(subcmd_name, sizeof(subcmd_name), "%s%s", tools_path, argv[0]);

    if (access(subcmd_name, 0) != 0)
        snprintf(subcmd_name, sizeof(subcmd_name), "%s%s%s", tools_path, kern_version, argv[0]);

    for (i = 1; i <= (argc - 1); i++) {
        snprintf(subcmd_args, sizeof(subcmd_args), " \"%s\"", argv[i]);
        strcat(subcmd_name,subcmd_args);
    }

    if (run_depend[0])
        add_python_depend(run_depend, subcmd_name);
    else
        strncpy(tools_exec, subcmd_name, strlen(subcmd_name));

    snprintf(subcmd_exec_final, sizeof(subcmd_exec_final), "%s;%s", sysak_work_path, tools_exec);
    ret = my_system(subcmd_exec_final);
    if (ret < 0)
        return ret;

    if (post_module)
        mod_ctrl(false);

    return ret;
}

static void print_each_tool(bool all)
{
    int i, max_idx = all ? BCLINUX_TOOL : USER_TOOL;
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

static int char_num(char *str, char c)
{
    char *pos;
    int num = 0, i = 0;
    int len = strlen(str);

    for (i; i < len ; i++){
        if (str[i] == c)
            num++;
    }
    return num;
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
            if (char_num(tools_class_module, '/') >= 2){
                free(node);
                continue;
            }
            list = &tool_lists[EXPERT_TOOL];
            sscanf(tools_class_module, "detect/%[^:]", node->tool.module);
        } else if (strncmp(tools_class_module, "monitor", 7) == 0) {
            list = &tool_lists[MONITOR_TOOL];
        } else if (strncmp(tools_class_module, "bclinux", 7) == 0) {
            list = &tool_lists[BCLINUX_TOOL];
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

static int build_diagnose_cmd_info_from_file(FILE *fp)
{
    struct diagnose_menu_node *menu_node;
    struct diagnose_tool *node;
    char buf[MAX_NAME_LEN + MAX_SUBCMD_ARGS], *default_str, *cmd_str;
    char tools_name[MAX_NAME_LEN], menu_name[MAX_NAME_LEN];
    char tools_class_module[MAX_NAME_LEN];
    int ret = 0, flag = 0, menu_num = 0;

    // diag_list_head

    while (fgets(buf, sizeof(buf), fp))
    {
        sscanf(buf, "%[^/]/%[^:]", menu_name, tools_name);

        if (!menu_name)
            continue;

        menu_node = diag_list_head;
        flag = 0;
        while (menu_node && flag == 0)
        {
            if (strcmp(menu_name, menu_node->diagnose_menu_name) == 0)
            {
                flag = 1;
                break;
            }
            menu_node = menu_node->next;
        }

        if (!flag)
        {
            // create new menu
            menu_node = malloc(sizeof(struct diagnose_menu_node));
            memset(menu_node, 0, sizeof(struct diagnose_menu_node));
            strcpy(menu_node->diagnose_menu_name, menu_name);
            menu_node->next = diag_list_head;
            diag_list_head = menu_node;
        }

        node = malloc(sizeof(struct diagnose_tool));
        if (!node)
        {
            fclose(fp);
            return -1;
        }
        memset(node, 0, sizeof(struct diagnose_tool));

        strcpy(node->tool.name, tools_name);
        default_str = strstr(buf, ":default{");
        cmd_str = strstr(buf, ":cmd{");

        if (default_str)
            sscanf(default_str, ":default{%[^}]}", node->tool.default_cmd);
        if (cmd_str)
            sscanf(cmd_str, ":cmd{%[^}]}", node->tool.cmd);

        if (menu_node->diagnose_tool_head == NULL)
        {
            menu_node->diagnose_tool_head = node;
        }
        else
        {
            node->next = menu_node->diagnose_tool_head;
            menu_node->diagnose_tool_head = node;
        }
        ret++;
    }

    fclose(fp);
    return ret;
}

static int build_diagnose_cmd_info()
{
    FILE *fp;
    int ret = 0;

    if (access(sysak_diagnose_rule, 0) != 0)
    {
        printf(".sysak.diag.config not found!\n");
        return 0;
    }

    fp = fopen(sysak_diagnose_rule, "r");
    if (fp)
        ret += build_diagnose_cmd_info_from_file(fp);

    return ret;
}

static void show_diagnose_menu()
{
    struct diagnose_menu_node *menu_node = diag_list_head;
    int num = 0;
    printf("\n");
    while (menu_node)
    {
        num++;
        printf("%d %s\n", num, menu_node->diagnose_menu_name);
        menu_node = menu_node->next;
    }
}

static void show_diagnose_tool_menu()
{
    struct diagnose_tool *node = diag_menu_choose->diagnose_tool_head;
    int num = 0;
    printf("\n");
    while (node)
    {
        num++;
        printf("  %d  %s\n", num, node->tool.name);
        node = node->next;
    }
}

static int choose_diagnose_menu()
{
    int num;
    printf("\nchoose a menu number:");
    scanf("%d", &num);
    return num;
}

static int choose_diagnose_tool_menu()
{
    int num;
    printf("\nchoose a tool number:");
    scanf("%d", &num);
    return num;
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
        char class_name[MAX_NAME_LEN];

        pstr = buf;
        sscanf(buf,"%[^:]:%[^:]", class_name, tools_name);
    if (strstr(class_name,"combine"))
        if (strstr(class_name,"/"))
                continue;

        if (strcmp(tools_name, tool)) {
            continue;
        }
        pstr = strstr(buf, ":prev{");
        if (pstr)
            sscanf(pstr, ":prev{%[^}]};post{%[^}]}", prev_dep, post_dep, run_depend);
        pstr = strstr(buf, ":python-dep{");
        if (pstr)
            sscanf(pstr, ":python-dep{%[^}]}", run_depend);

        pstr = strstr(buf, "bpf");
        if (pstr)
            btf_depend = true;

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
        if (check_or_install_components(tool) < 0)
            return false;
    }

    if (!tool_rule_parse(sysak_other_rule, tool) &&
        !tool_rule_parse(sysak_rule, tool))
        return false;

    return true;
}

int copy_file(char *dest_file, char *src_file)
{
    int cnt = 0;
    FILE *fp1 = fopen(dest_file,"w");
    FILE *fp2 = fopen(src_file,"r");

    if(fp1 == NULL) {
        printf("%s:fopen failed!", dest_file);
        return -1;
    }
    if(fp2 == NULL) {
        printf("%s: fopen failed!", src_file);
        return -1;
    }

    char buffer = fgetc(fp2);

    while(!feof(fp2)) {
        cnt++;
        fputc(buffer,fp1);
        buffer = fgetc(fp2);
    }
    fclose(fp1);
    fclose(fp2);
    return cnt;
}

int has_string(char *dest_file, char *substring)
{
	FILE *fp;
	int	line=0;
	char file_str[DEFAULT_LEN];

	fp=fopen(dest_file,"r");
	if(fp==NULL)
	{
		printf("open error\n");
		return -1;
	}

	while(fgets(file_str,sizeof(file_str),fp))
	{
		line++;
		if(strstr(file_str,substring))
		{
			return 1;
		}
	}
	fclose(fp);
	return 0;
}

void btf_support_check(void){
    char config[DEFAULT_LEN];
    char local_btf[DEFAULT_LEN] = "/sys/kernel/btf/vmlinux";
    char tool_btf[DEFAULT_LEN];
    char *config_name = "CONFIG_BPF=y";

    if (access(local_btf,0) == 0){
        snprintf(tool_btf, sizeof(tool_btf), "%s/vmlinux-%s", tools_path, kern_version);
        if (copy_file(tool_btf, local_btf) > 0){
            oss_get_components = auto_get_components =false;
            btf_depend = false;
            return;
        }
    }

    snprintf(config, sizeof(config), "/boot/config-%s", kern_version);
    if (!has_string(config, config_name))
        btf_depend = false;
}

static int subcmd_parse(int argc, char *argv[])
{
    int i;

    if (!tool_lookup(argv[0]) && strcmp(argv[0], "-d")) {
        printf("no components, you should get first\n");
        return -ERR_NOSUBTOOL;
    }

    if (!strcmp(argv[0], "-d")) {
        only_download = true;
        pre_module = true;
	    btf_depend = true;
	    goto exec;
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

    for (i = 1; i <= (argc-1); i++)
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
    btf_support_check();
    return exectue(argc, argv);
}

static void get_menu_node(int menu_num)
{
    diag_menu_choose = diag_list_head;
    int num = 1;
    while (diag_menu_choose && num < menu_num)
    {
        num++;
        diag_menu_choose = diag_menu_choose->next;
    }
    if (diag_menu_choose)
        printf("\n%s:", diag_menu_choose->diagnose_menu_name);
    else
        printf("please choose a valid menu number!\n");
}

static void get_diagnose_tool_node(int tool_num)
{
    diag_tool_choose = diag_menu_choose->diagnose_tool_head;
    int num = 1;
    while (diag_tool_choose && num < tool_num)
    {
        num++;
        diag_tool_choose = diag_tool_choose->next;
    }
    if (!diag_tool_choose)
        printf("please choose a valid tool number!\n");
}

static void show_diagnose_usage()
{
    printf("\nusage of %s:\n", diag_tool_choose->tool.name);
    // printf("strlen=%d",strlen(diag_tool_choose->tool.cmd));
    if (strlen(diag_tool_choose->tool.cmd))
    {
        printf("\n%16s %s\n", " ", diag_tool_choose->tool.cmd);
        printf("\ndefault cmd:\n%16s%s \n\n", " ", diag_tool_choose->tool.default_cmd);
    }
    else
        printf("\n%16s %s\n\n", " ", diag_tool_choose->tool.default_cmd);
}

static int diagnose_page()
{
    if (build_diagnose_cmd_info() == 0)
    {
        printf("diagnose tool not found");
        return 0;
    }
    show_diagnose_menu();
    int menu_num, tool_num;
    while (diag_menu_choose == NULL)
    {
        menu_num = choose_diagnose_menu();
        get_menu_node(menu_num);
    }
    show_diagnose_tool_menu();
    while (diag_tool_choose == NULL)
    {
        tool_num = choose_diagnose_tool_menu();
        get_diagnose_tool_node(tool_num);
    }
    show_diagnose_usage();

    return 0;
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

    if (!strcmp(argv[1], "-c"))
    {
        diagnose_page();
        return 0;
    }

    if (!strcmp(argv[1], "help") || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
    {
        usage();
        return 0;
    }

    if (!strcmp(argv[1], "-g")) {
        if (argc < 3) {
            usage();
            return -ERR_MISSARG;
        }
        auto_get_components = true;

        if (!strcmp(argv[2], "-oss")){
            oss_get_components = true;
            argc = argc - 3;
            argv = &argv[3];
        } else {
            argc = argc - 2;
            argv = &argv[2];
        }
    } else if (!strcmp(argv[1], "-oss")) {
        if (argc < 3) {
            usage();
            return -ERR_MISSARG;
        }
        oss_get_components = true;
        auto_get_components = true;
        if (!strcmp(argv[2], "-g")){
            argc = argc - 3;
            argv = &argv[3];
        } else {
            argc = argc - 2;
            argv = &argv[2];
        }
    } else {
        argc = argc - 1;
        argv = &argv[1];
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
    char components_path[MAX_WORK_PATH_LEN];
    char tmp[MAX_WORK_PATH_LEN];
    char *current_path;

    realpath(argv[0],tmp);
    current_path = dirpath(tmp);

    snprintf(components_path, sizeof(components_path), "%s%s",
             current_path, "/.sysak_components");

    if (access(components_path,0) != 0)
        snprintf(components_path, sizeof(tools_path), "%s%s",
                 sysak_root_path, "/.sysak_components");

    snprintf(tools_path, sizeof(tools_path), "%s%s",
             components_path, "/tools/");
    snprintf(module_path, sizeof(module_path), "%s%s",
             components_path, "/lib/");
    snprintf(sysak_rule, sizeof(sysak_rule), "%s%s",
             components_path, "/tools/.sysak.rules");
    snprintf(sysak_other_rule, sizeof(sysak_other_rule), "%s%s%s%s",
             components_path, "/tools/", kern_version, "/.sysak.rules");
    snprintf(sysak_diagnose_rule, sizeof(sysak_diagnose_rule), "%s", "/etc/sysak/.sysak.diag.config");
    snprintf(sysak_work_path, sizeof(sysak_work_path), "%s%s",
             "export SYSAK_WORK_PATH=", components_path);
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
