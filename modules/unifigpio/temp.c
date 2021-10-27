#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

int len, temp;

char *msg;
char *dirname = "gpio";
struct proc_dir_entry *proc_dir;
int read_proc(struct file *filp, char *buf, size_t count, loff_t *offp)
{
	if (count > temp) {
		count = temp;
	}
	temp = temp - count;
	copy_to_user(buf, msg, count);
	if (count == 0)
		temp = len;

	return count;
}

int write_proc(struct file *filp, const char *buf, size_t count, loff_t *offp)
{
	copy_from_user(msg, buf, count);
	len = count;
	temp = len;
	return count;
}

struct file_operations proc_fops = { read: read_proc, write: write_proc };
void create_new_proc_entry()
{
	proc_dir = proc_mkdir(dirname, NULL);
	proc_create("hello", 0, proc_dir, &proc_fops);
	msg = kmalloc(GFP_KERNEL, 10 * sizeof(char));
}

int proc_init(void)
{
	create_new_proc_entry();
	return 0;
}

void proc_cleanup(void)
{
	remove_proc_entry("temp1", NULL);
}

MODULE_LICENSE("GPL");
module_init(proc_init);
module_exit(proc_cleanup);
