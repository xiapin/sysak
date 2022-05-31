#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#define KVERSION 64
#define MAX_SUBCMD_ARGS 512
#define MAX_DEPEND_LEN 128
#define MAX_NAME_LEN 64
#define MAX_WORK_PATH_LEN 512
#define ERR_NOSUBTOOL 2
#define ERR_MISSARG 3

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
char modin[MAX_DEPEND_LEN];
char modun[MAX_DEPEND_LEN];
char run_depend[MAX_DEPEND_LEN]= {0};
char tools_path[MAX_WORK_PATH_LEN];
char tools_exec[MAX_WORK_PATH_LEN] = {0};
char sysak_rule[MAX_WORK_PATH_LEN];
char module_path[MAX_WORK_PATH_LEN];
char sysak_work_path[MAX_WORK_PATH_LEN];
char sysak_other_rule[MAX_WORK_PATH_LEN];
bool pre_module = false;
bool post_module = false;

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

static void kern_release(void)
{
    struct utsname name;

    if (uname (&name) == -1){
        printf("cannot get system version\n");
        return;
    }
    strncpy(kern_version, name.release, sizeof(name.release));
}

static void mod_ctrl(bool enable)
{
    FILE *modlist_fp;
    char exec_mod[4*KVERSION];
    bool has_ko = false;
    char modinfo[MAX_SUBCMD_ARGS];

    if (access(system_modules,0) != 0)
        return;

    modlist_fp = fopen(system_modules, "r");
    if (!modlist_fp){
        printf("open %s failed\n", system_modules);
		return;
    }
    while(fgets(modinfo, sizeof(modinfo), modlist_fp))
    {
        if (strstr(modinfo,"sysak")){
            has_ko = true;
            break;
        }

    }
    fclose(modlist_fp); 

    if (enable && !has_ko) {
        snprintf(exec_mod, sizeof(exec_mod), "insmod %s%s%s", module_path, kern_version, module);
        system(exec_mod);

    }
    else if (!enable && has_ko) {
        snprintf(exec_mod, sizeof(exec_mod), "rmmod %s%s%s", module_path, kern_version, module);
        system(exec_mod);
    }
}
static void add_python_depend(char *depend,char *cmd)
{
    if (!strcmp(depend, "all")){
        snprintf(tools_exec, sizeof(tools_exec), "python2 %s", cmd);
    }else if (!strcmp(depend, "python3")){
        snprintf(tools_exec, sizeof(tools_exec), "python3 %s", cmd);
    }else if(!strcmp(depend, "python2")){
        snprintf(tools_exec, sizeof(tools_exec), "python2 %s", cmd);
    }
}

static int exectue(int argc, char *argv[])
{
    int i;
    int ret = 0;
    char subcmd_name[MAX_NAME_LEN+MAX_SUBCMD_ARGS];
    char subcmd_args[MAX_SUBCMD_ARGS];
    char subcmd_exec_final[MAX_NAME_LEN+MAX_SUBCMD_ARGS];

    if (pre_module)
        mod_ctrl(true);

    snprintf(subcmd_name, sizeof(subcmd_name), "%s%s", tools_path, argv[1]);

    if (access(subcmd_name,0) != 0)
        snprintf(subcmd_name, sizeof(subcmd_name), "%s%s%s", tools_path, kern_version, argv[1]);

    for(i = 2; i <= (argc-1); i++){
        snprintf(subcmd_args, sizeof(subcmd_args), " \"%s\"", argv[i]);
        strcat(subcmd_name,subcmd_args);
    }

    if (run_depend[0])
        add_python_depend(run_depend, subcmd_name);
    else
        strncpy(tools_exec,subcmd_name,strlen(subcmd_name));

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

static bool tool_lookup(char *path, char *tool)
{
    FILE *fp;
    char buf[MAX_NAME_LEN + MAX_SUBCMD_ARGS];

    if (access(path,0) != 0)
        return false;

    fp = fopen(path, "r");
    if (!fp){
        printf("open %s failed\n", path);
		return false;
    }
    while(fgets(buf, sizeof(buf), fp))
    {
        char tools_name[MAX_NAME_LEN];

        sscanf(buf,"%*[^:]:%[^:]",tools_name);
        if (strcmp(tools_name, tool)){
            continue;
        }
        sscanf(buf,"%*[^:]:prev{%[^}]};post{%[^}]};python-dep{%[^}]}", modin, modun, run_depend);
        if (!run_depend[0])
            sscanf(buf,"%*[^:]:python-dep{%[^}]}", run_depend);
        return true;
    }
    fclose(fp);
    return false;
}

static int subcmd_parse(int argc, char *argv[])
{
    int i;
    
    if (!tool_lookup(sysak_other_rule, argv[1]) && 
            !tool_lookup(sysak_rule, argv[1])){
        printf("no components, you should get first\n");
        return -ERR_NOSUBTOOL;
    }

    if (strstr(modin, "default") != NULL|| strstr(modun, "default") != NULL){
        pre_module = true;
        post_module = true;
        goto exec;
    }

    for(i = 2; i <= (argc-1); i++)
    {
        if (strstr(modin, argv[i])){
            pre_module = true;
            break;
        }
        else if (strstr(modun, argv[i])){
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

    if (argc < 2){
        usage();
        return -ERR_MISSARG;
    }

    if (!strcmp(argv[1],"list")){
        if (argc == 3 && !strcmp(argv[2],"-a"))
            show_all = true;

        subcmd_list(show_all);
        return 0;
    }

    if (!strcmp(argv[1],"help")){
        usage();
        return 0;
    }
    return subcmd_parse(argc, argv);
}
char *dirpath(char *fullpath)
{
    char* substr = strrchr(fullpath,'/');
    char *ptr = fullpath;

    while(strcmp(substr,ptr))
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
