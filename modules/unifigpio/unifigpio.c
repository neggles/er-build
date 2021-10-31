#include <linux/init.h> /* initializes the module */
#include <linux/kernel.h> /* doing kernel things, shockingly */
#include <linux/module.h> /* specifically, a kernel module */
#include <linux/moduleparam.h> /* module has parameters */
#include <linux/printk.h> /* prints info to the kernel log */
#include <linux/errno.h> /* error codes */
#include <linux/proc_fs.h> /* creates a procfs dir and some procfs files */
#include <linux/fs.h>
#include <linux/seq_file.h> /* uses seq_file operations */
#include <linux/uaccess.h> /* copies data to/from userspace */
#include <linux/jiffies.h> /* wants to know what time is */
#include <linux/timer.h> /* so it can set up a timer */
#include <linux/gpio.h> /* do some gpio operations */
//#include <linux/version.h> /* for version compat in future. */


//  Define the module metadata.
#define MODULE_NAME "unifigpio"
MODULE_AUTHOR("Andrew Powers-Holmes");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("UniFi GPIO compat module");
MODULE_VERSION("0.1");

// name strings
static const char *proc_dir_name = "gpio";
static const char *proc_led_pattern_name = "led_pattern";
static const char *proc_led_tempo_name = "led_tempo";
static const char *proc_lcm_tempo_name = "lcm_tempo";

// LED pattern array
#define LED_PATTERN_MAX 128
static int led_pattern[LED_PATTERN_MAX] = { 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
					    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
					    0x30, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x22, 0x58, 0 };
static int led_pattern_len = 1;
module_param_array(led_pattern, int, &led_pattern_len, 0000);
MODULE_PARM_DESC(proc_led_pattern_name, "LED pattern (array of ints)");

// LED tempo number
static int led_tempo = 120;
module_param(led_tempo, int, 0000);
MODULE_PARM_DESC(proc_led_tempo_name, "LED tempo (int)");

// LCM tempo value, controls GPIO 27
static int lcm_tempo = 2;
module_param(lcm_tempo, int, 0000);
MODULE_PARM_DESC(proc_lcm_tempo_name, "LCM tempo (int)");


// LED timer
static struct timer_list led_timer;
static int led_pattern_index = 0;
static int led_status = 0;

// pin numbers
static const unsigned int gpio_led_blue = 29;
static const unsigned int gpio_led_white = 28;
static const unsigned int gpio_lcm_reset = 27;
static const unsigned int gpio_lcm_boot = 26;

// GPIO values
static bool led_blue = 1;
static bool led_white = 1;
static bool lcm_reset = 1;
static bool lcm_boot = 0;

// procfs directory structs
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_led_pattern;
static struct proc_dir_entry *proc_led_tempo;
static struct proc_dir_entry *proc_lcm_tempo;


// prototypes for our functions later on
static void gpio_update_led(void);
// static void gpio_set_led(unsigned int new_val);
static void gpio_update_lcm(unsigned int new_tempo);

static int led_pattern_show(struct seq_file *m, void *v);
static int led_pattern_open(struct inode *inode, struct file *file);
static ssize_t led_pattern_write(struct file *file, const char __user *usr_buf, size_t count, loff_t *ppos);

static int led_tempo_show(struct seq_file *m, void *v);
static int led_tempo_open(struct inode *inode, struct file *file);
static ssize_t led_tempo_write(struct file *file, const char __user *usr_buf, size_t count, loff_t *ppos);

static int lcm_tempo_show(struct seq_file *m, void *v);
static int lcm_tempo_open(struct inode *inode, struct file *file);
static ssize_t lcm_tempo_write(struct file *file, const char __user *usr_buf, size_t count, loff_t *ppos);


// file operations structs
static struct file_operations led_pattern_fops = {
	.owner = THIS_MODULE,
	.open = led_pattern_open,
	.llseek = seq_lseek,
	.read = seq_read,
	.write = led_pattern_write,
	.release = single_release,
};

static struct file_operations led_tempo_fops = {
	.owner = THIS_MODULE,
	.open = led_tempo_open,
	.llseek = seq_lseek,
	.read = seq_read,
	.write = led_tempo_write,
	.release = single_release,
};

static struct file_operations lcm_tempo_fops = {
	.owner = THIS_MODULE,
	.open = lcm_tempo_open,
	.llseek = seq_lseek,
	.read = seq_read,
	.write = lcm_tempo_write,
	.release = single_release,
};

// and thus begins the actual code!
static int __init unifigpio_init(void)
{
	pr_info("%s: init\n", MODULE_NAME);

	// create led timer
	setup_timer(&led_timer, gpio_update_led, 0);
	mod_timer(&led_timer, jiffies + msecs_to_jiffies(6000 / led_tempo));
	pr_info("%s: LED timer created", MODULE_NAME);

	// create procfs directory
	proc_dir = proc_mkdir(proc_dir_name, NULL);
	if (!proc_dir) {
		pr_err("%s: failed to create /proc/%s\n", MODULE_NAME, proc_dir_name);
		return -ENOMEM;
	}

	proc_led_pattern = proc_create(proc_led_pattern_name, 0644, proc_dir, &led_pattern_fops);
	if (!proc_led_pattern) {
		pr_err("%s: failed to create /proc/%s/%s\n", MODULE_NAME, proc_dir_name,
		       proc_led_pattern_name);
		return -ENOMEM;
	}

	proc_led_tempo = proc_create(proc_led_tempo_name, 0644, proc_dir, &led_tempo_fops);
	if (!proc_led_tempo) {
		pr_err("%s: failed to create /proc/%s/%s\n", MODULE_NAME, proc_dir_name, proc_led_tempo_name);
		return -ENOMEM;
	}

	proc_lcm_tempo = proc_create(proc_lcm_tempo_name, 0644, proc_dir, &lcm_tempo_fops);
	if (!proc_lcm_tempo) {
		pr_err("%s: failed to create /proc/%s/%s\n", MODULE_NAME, proc_dir_name, proc_lcm_tempo_name);
		return -ENOMEM;
	}
	// success!
	pr_info("%s: procfs files created", MODULE_NAME);

	// now set up the GPIOs
	gpio_request(gpio_led_blue, "logo-blue");
	gpio_direction_output(gpio_led_blue, led_blue);

	gpio_request(gpio_led_white, "logo-white");
	gpio_direction_output(gpio_led_white, led_white);

	gpio_request(gpio_lcm_reset, "lcm-reset");
	gpio_direction_output(gpio_lcm_reset, lcm_reset);

	gpio_request(gpio_lcm_reset, "lcm-boot");
	gpio_direction_output(gpio_lcm_boot, lcm_boot);

	pr_info("%s: init complete\n", MODULE_NAME);
	return 0;
}

// exit/cleanup func
static void __exit unifigpio_exit(void)
{
	pr_info("%s: unloading...\n", MODULE_NAME);

	// remove procfs directory
	remove_proc_entry(proc_led_pattern_name, proc_dir);
	remove_proc_entry(proc_led_tempo_name, proc_dir);
	remove_proc_entry(proc_lcm_tempo_name, proc_dir);
	remove_proc_entry(proc_dir_name, NULL);

	// release GPIOs
	gpio_free(gpio_led_blue);
	gpio_free(gpio_led_white);
	gpio_free(gpio_lcm_reset);
	gpio_free(gpio_lcm_boot);

	// delete timer
	del_timer(&led_timer);

	pr_info("%s: unload complete\n", MODULE_NAME);
	return;
}

// LED timer callback
static void gpio_update_led(void)
{
	uint ledval = led_pattern[led_pattern_index];
	if (led_status != ledval) {
		led_status = ledval;
		gpio_set_value(gpio_led_blue, led_status & 0b01);
		gpio_set_value(gpio_led_white, (led_status & 0b10) >> 1);
	}
	if (1 < led_pattern_len) {
		led_pattern_index = (led_pattern_index + 1) % led_pattern_len;
		mod_timer(&led_timer, jiffies + msecs_to_jiffies(6000 / led_tempo));
	}

	return;
}

static void gpio_update_lcm(unsigned int new_tempo)
{
	if (lcm_tempo != new_tempo) {
		lcm_tempo = new_tempo;
	}

	switch (lcm_tempo) {
	case 0:
		gpio_set_value(gpio_lcm_boot, 1);
		gpio_set_value(gpio_lcm_reset, 1);
		break;
	case 1:
		gpio_set_value(gpio_lcm_boot, 1);
		gpio_set_value(gpio_lcm_reset, 0);
		break;
	case 2:
		gpio_set_value(gpio_lcm_boot, 0);
		gpio_set_value(gpio_lcm_reset, 0);
		break;
	default:
		return;
	};
}

// LED pattern ops
static int led_pattern_open(struct inode *inode, struct file *file)
{
	return single_open(file, led_pattern_show, NULL);
}
static int led_pattern_show(struct seq_file *m, void *v)
{
	if (led_pattern_len != 0) {
		int i;
		for (i = 0; i < led_pattern_len; i++) {
			seq_printf(m, "type%d: %1d\n", i, led_pattern[i]);
		}
	}
	seq_printf(m, "\n");
	return 0;
}
static ssize_t led_pattern_write(struct file *file, const char __user *usr_buf, size_t count, loff_t *ppos)
{
	char buf[512];
	int new_pattern[128];
	char *pos = buf;
	int n, idx = 0;

	if (*ppos > 0 || count > ARRAY_SIZE(buf))
		return -EINVAL;

	if (copy_from_user(buf, usr_buf, count))
		return -EFAULT;

	// make sure our string is terminated properly
	buf[count] = '\0';
	pr_info("%s: received new pattern string: %s\n", MODULE_NAME, buf);

	// parse the string

	while (sscanf(pos, " %d%n", &new_pattern[idx], &n) == 1) {
		pr_debug("idx=%5d val=%5d n=%5d\n", idx, new_pattern[idx], n);
		if (idx >= ARRAY_SIZE(new_pattern))
			break;
		idx++;
		pos += n;
	}
	pr_info("%s: parsed %d values from string\n", MODULE_NAME, idx);

	// copy the new pattern to the global array
	if (led_pattern != new_pattern) {
		led_pattern_len = idx;
		memcpy(led_pattern, new_pattern, sizeof(led_pattern));
	}

	// reset the pattern index and restart the animation
	led_pattern_index = 0;
	gpio_update_led();

	return count;
}


// LED tempo ops
static int led_tempo_open(struct inode *inode, struct file *file)
{
	return single_open(file, led_tempo_show, NULL);
}
static int led_tempo_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d (beats per minute)\n", led_tempo);
	return 0;
}
static ssize_t led_tempo_write(struct file *file, const char __user *usr_buf, size_t count, loff_t *ppos)
{
	int rv, val;

	rv = kstrtoint_from_user(usr_buf, count, 0, &val);
	if (rv)
		return rv;

	if (val < 0 || val > 254)
		return -EINVAL;

	if (led_tempo != val) {
		led_tempo = val;
		mod_timer(&led_timer, jiffies + msecs_to_jiffies(6000 / led_tempo));
	}

	return count;
}

// LCM tempo opts
static int lcm_tempo_open(struct inode *inode, struct file *file)
{
	return single_open(file, lcm_tempo_show, NULL);
}

static int lcm_tempo_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%X (BIT0: BOOT, BIT1: SWNRST)\n", lcm_tempo);
	return 0;
}
static ssize_t lcm_tempo_write(struct file *file, const char __user *usr_buf, size_t count, loff_t *ppos)
{
	int rv, val;

	rv = kstrtoint_from_user(usr_buf, count, 0, &val);
	if (rv)
		return rv;

	if (val != 0 && val != 1 && val != 2) {
		return -EINVAL;
	}

	gpio_update_lcm(val);

	return count;
}

module_init(unifigpio_init);
module_exit(unifigpio_exit);
