
#define pr_fmt(fmt) "[%s]-[%s]: " fmt, "weizhen.zt", __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/sched.h>

#define KSYM_NAME_LEN 16

static char symbol[KSYM_NAME_LEN] = "mutex_bock";
module_param_string(symbol, symbol, KSYM_NAME_LEN, 0644);

struct mutex m;
/*
ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
int (*open)(struct inode *, struct file *);
int (*release) (struct inode *, struct file *);
 */

ssize_t read_proc(struct file *f, char __user *u, size_t s, loff_t *off) {
    pr_info("Read proc file.\n");
    return 0;
}

ssize_t write_proc(struct file *f, const char __user *u, size_t s,
                   loff_t *off) {
    pr_info("Write proc file.\n");
    return 0;
}

int open_proc(struct inode *i, struct file *f) {
    pr_info("Open proc file.\n");
    mutex_lock(&m);
    pr_info("Lock mutex.\n");
    // long __hz = sysconf(_SC_CLK_TCK);
    schedule_timeout_uninterruptible(10 * HZ);
    mutex_unlock(&m);
    pr_info("Unock mutex.\n");
    return 0;
}

int release_proc(struct inode *i, struct file *f) {
    pr_info("Release proc file.\n");
    return 0;
}

static struct file_operations proc_fops = {.open = open_proc,
                                           .read = read_proc,
                                           .write = write_proc,
                                           .release = release_proc};

// struct proc_dir_entry *proc_create(const char *name, umode_t mode, struct
// proc_dir_entry *parent, const struct file_operations *proc_fops);

static struct proc_dir_entry *ent = 0;
static int __init mod_init(void) {
    char name[16] = "demo_mutex";
    ent = proc_create(name, 0666, 0, &proc_fops);
    if (!ent) {
        pr_info("Failed create proc file.\n");
    }
    mutex_init(&m);

    pr_info("Module init success.\n");
    return 0;
}

// static inline void proc_remove(struct proc_dir_entry *de)

static void __exit mod_exit(void) {
    if (ent) {
        proc_remove(ent);
        pr_info("Remove proc file.\n");
    }
    pr_info("Module exit success.\n");
}

module_init(mod_init) module_exit(mod_exit) MODULE_LICENSE("GPL");