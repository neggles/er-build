//  Note:
//  This code is based on Derek Molloy's excellent tutorial on creating Linux
//  Kernel Modules:
//    http://derekmolloy.ie/writing-a-linux-kernel-module-part-2-a-character-device/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

//  Define the module metadata.
#define MODULE_NAME "unifigpio"
#define PROC_DIR_NAME "gpio"
#define PROC_LED_PATTERN_NAME "led_pattern"
#define PROC_LED_TEMPO_NAME "led_tempo"
#define PROC_LCM_TEMPO_NAME "lcm_tempo"
#define PROC_POE_PASSTHROUGH_NAME "poe_passthrough"

MODULE_AUTHOR("Andrew Powers-Holmes");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("UniFi Security Gateway GPIO module");
MODULE_VERSION("0.1");

//  Define module parameters.
static char *name = "Bilbo";
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "The name to display in /var/log/kern.log");

// LED timer
static struct timer_list led_timer;

// procfs directory struct
static struct proc_dir_entry proc_dir;

// LED pattern
static struct proc_dir_entry proc_led_pattern;
static struct file_operations proc_led_pattern_file_fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

// LED tempo
static struct proc_dir_entry proc_led_tempo;
static struct file_operations proc_led_tempo_file_fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

// LCM tempo
static struct proc_dir_entry proc_lcm_tempo;
static struct file_operations proc_lcm_tempo_file_fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

// PoE Passthrough
static struct proc_dir_entry proc_poe_passthrough;
static struct file_operations proc_poe_passthrough_file_fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};


//  The device number, automatically set. The message buffer and current message
//  size. The number of device opens and the device class struct pointers.
static int majorNumber;
static char message[256] = { 0 };
static short messageSize;
static int numberOpens = 0;
static struct class *unifigpioClass = NULL;
static struct device *unifigpioDevice = NULL;
static DEFINE_MUTEX(ioMutex);

// prototypes for functions
static void gpio_update_led(void);

//  Prototypes for our device functions.
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

//   Our main 'unifigpio' function...
static char unifigpio(char input);

//  Create the file operations instance for our driver.
static struct file_operations fops = {
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

static int __init mod_init(void)
{
	pr_info("%s: module loaded at 0x%p\n", MODULE_NAME, mod_init);

	//  Create a mutex to guard io operations.
	mutex_init(&ioMutex);

	//  Register the device, allocating a major number.
	majorNumber = register_chrdev(0 /* i.e. allocate a major number for me */, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		pr_alert("%s: failed to register a major number\n", MODULE_NAME);
		return majorNumber;
	}
	pr_info("%s: registered correctly with major number %d\n", MODULE_NAME, majorNumber);

	//  Create the device class.
	unifigpioClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(unifigpioClass)) {
		//  Cleanup resources and fail.
		unregister_chrdev(majorNumber, DEVICE_NAME);
		pr_alert("%s: failed to register device class\n", MODULE_NAME);

		//  Get the error code from the pointer.
		return PTR_ERR(unifigpioClass);
	}
	pr_info("%s: device class registered correctly\n", MODULE_NAME);

	//  Create the device.
	unifigpioDevice = device_create(unifigpioClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if (IS_ERR(unifigpioDevice)) {
		class_destroy(unifigpioClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		pr_alert("%s: failed to create the device\n", DEVICE_NAME);
		return PTR_ERR(unifigpioDevice);
	}
	pr_info("%s: device class created correctly\n", DEVICE_NAME);

	//  Success!
	return 0;
}

static int __init init_module(void) {
	pr_info("%s: module loaded at 0x%p\n", MODULE_NAME, init_module);

	// init the LED timer
	timer_setup(&led_timer, gpio_update_led, 0);
	mod_timer(&led_timer, jiffies + msecs_to_jiffies(6000));
	pr_debug("%s: LED timer created", MODULE_NAME);

	// create procfs dirs
	proc_dir = proc_mkdir(PROC_DIR_NAME, 0);
	if (proc_dir == 0) {
		pr_crit("%s: failed to create /proc/%s\n", MODULE_NAME, PROC_DIR_NAME);
	} else {
		pr_debug("%s: create /proc/%s/%s\n", PROC_DIR_NAME, PROC_LED_PATTERN_NAME);

		pr_debug("%s: create /proc/%s/%s\n", PROC_DIR_NAME, PROC_LED_TEMPO_NAME);

		pr_debug("%s: create /proc/%s/%s\n", PROC_DIR_NAME, PROC_LCM_TEMPO_NAME);

		pr_debug("%s: create /proc/%s/%s\n", PROC_DIR_NAME, PROC_POE_PASSTHROUGH_NAME);
	}

	return 0;
}

static void __exit cleanup_module(void) {
	pr_info("%s: unloading...\n", MODULE_NAME);
    if (proc_dir != 0) {
        if (proc_led_pattern != 0) {
            remove_proc_entry("led_pattern", proc_dir);
        }
        if (proc_led_tempo != NULL) {
            remove_proc_entry("led_tempo", proc_dir);
        }
        if (proc_poe_passthrough != 0) {
            remove_proc_entry("poe_passthrough",proc_dir);
        }
    }
    remove_proc_entry(PROC_DIR_NAME,0);
    del_timer(led_timer);
    return;
}

static void __exit mod_exit(void)
{
	pr_info("%s: unloading...\n", MODULE_NAME);
	device_destroy(unifigpioClass, MKDEV(majorNumber, 0));
	class_unregister(unifigpioClass);
	class_destroy(unifigpioClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	mutex_destroy(&ioMutex);
	pr_info("%s: device unregistered\n", MODULE_NAME);
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep)
{
	//  Try and lock the mutex.
	if (!mutex_trylock(&ioMutex)) {
		pr_alert("%s: device in use by another process", MODULE_NAME);
		return -EBUSY;
	}

	numberOpens++;
	pr_info("%s: device has been opened %d time(s)\n", MODULE_NAME, numberOpens);
	return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	int error_count = 0;
	int i = 0;
	char unifigpioMessage[256] = { 0 };

	//   Create the unifigpio content.
	for (; i < messageSize; i++) {
		unifigpioMessage[i] = unifigpio(message[i]);
	}

	// copy_to_user has the format ( * to, *from, size) and returns 0 on success
	error_count = copy_to_user(buffer, unifigpioMessage, messageSize);

	if (error_count == 0) {
		pr_info("%s: sent %d characters to the user\n", MODULE_NAME, messageSize);
		//    Clear the position, return 0.
		messageSize = 0;
		return 0;
	} else {
		pr_err("%s: failed to send %d characters to the user\n", MODULE_NAME, error_count);
		//    Failed -- return a bad address message (i.e. -14)
		return -EFAULT;
	}
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
	//  Write the string into our message memory. Record the length.
	copy_from_user(message, buffer, len);
	messageSize = len;
	pr_info("%s: received %zu characters from the user\n", MODULE_NAME, len);
	return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep)
{
	mutex_unlock(&ioMutex);
	pr_info("%s: device successfully closed\n", MODULE_NAME);
	return 0;
}

static char unifigpio(char input)
{
	if ((input >= 'a' && input <= 'm') || (input >= 'A' && input <= 'M')) {
		return input + 13;
	}
	if ((input >= 'n' && input <= 'z') || (input >= 'N' && input <= 'Z')) {
		return input - 13;
	}
	return input;
}

module_init(init_module);
module_exit(cleanup_module);
