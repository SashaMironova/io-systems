#include <linux/module.h> 
#include <linux/kernel.h> 
#include <linux/fs.h>
#include <linux/uaccess.h>        
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>

#define SUCCESS 0
#define DEVICE_NAME "var2"
#define DEVICE_CLASS_NAME "var2class"
#define DEVICE_FILE_NAME "var2"
#define PROC_FILE_NAME "var2"
#define BUF_LEN 80

/* 
 * Устройство уже открыто? Используется для
 * предотвращения конкурирующих запросов к устройству
 */
static int Device_Open = 0;

/* 
 * Ответ устройства на запрос
 */
static char Message[BUF_LEN];

/* 
 * Позиция в буфере.
 * Используется в том случае, если сообщение оказывется длиннее
 * чем размер буфера. 
 */
static char *Message_Ptr;

static int result = 0;

static struct proc_dir_entry* entry;

static dev_t first;
static struct cdev c_dev; 
static struct class *cl;


/* 
 * Вызывается когда процесс пытается открыть файл устройства
 */
static int device_open(struct inode *inode, struct file *file)
{
    //printk(KERN_INFO "device_open(%p)\n", file);

	/* 
	 * В каждый конкретный момент времени только один процесс может открыть файл устройства 
	 */
	if (Device_Open)
		return -EBUSY;

	Device_Open++;
	
	/*
	 * Инициализация сообщения
	 */
	Message_Ptr = Message;
	try_module_get(THIS_MODULE);
	return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
	//printk(KERN_INFO "device_release(%p,%p)\n", inode, file);

	/* 
	 * Теперь мы готовы принять запрос от другого процесса
	 */
	Device_Open--;

	module_put(THIS_MODULE);
	return SUCCESS;
}

/* 
 * Вызывается когда процесс, открывший файл устройства
 * пытается считать из него данные.
 */
static ssize_t device_read(struct file *file, /* см. include/linux/fs.h   */
            char __user * buffer,             /* буфер для сообщения */
            size_t length,                    /* размер буфера       */
            loff_t * offset)
{
    printk(KERN_INFO "device_read(%p)\n", file);
    printk(KERN_INFO "last result %d\n", result);
	return 0;
}

/* 
 * Вызывается при попытке записи в файл устройства
 */
static ssize_t device_write(struct file *file, const char __user * buffer, size_t length, loff_t * offset)
{
    int first_operand = -10;
    int second_operand = -10;

    char first, second, third;

	printk(KERN_INFO "device_write(%p)", file);


    //if (length != 3)
    //    return 0;

	if (copy_from_user(&first, buffer, 1) != 0)
	{
		return -EFAULT;
	}
	if (copy_from_user(&second, buffer+1, 1) != 0)
	{
		return -EFAULT;
	}
	if (copy_from_user(&third, buffer+2, 1) != 0)
	{
		return -EFAULT;
	}

    /*get_user(first, 0);
    get_user(second, 1);
    get_user(third, 2);*/

    first_operand = first - '0';
    second_operand = third - '0';

    switch (second) {
        case '+':
            result = first_operand+second_operand;
           break;
        case '-':
            result = first_operand-second_operand;
           break;
        case '*':
            result = first_operand*second_operand;
           break;
        case '/':
			if (second_operand != 0)
				result = first_operand/second_operand;
           break;
    }

	/* 
	* Вернуть количество принятых байт
	*/
	return 3;
}

static ssize_t proc_write(struct file *file, const char __user * ubuf, size_t count, loff_t* ppos) 
{
	printk(KERN_DEBUG "Attempt to write proc file");
	return 0;
}

static ssize_t proc_read(struct file *file, char __user * ubuf, size_t count, loff_t* ppos) 
{
    char string[20]; 
    size_t len = sprintf(string, "%d\n", result);
	if (*ppos > 0 || count < len)
	{
		return 0;
	}
	if (copy_to_user(ubuf, string, len) != 0)
	{
		return -EFAULT;
	}
	*ppos = len;
	return len;
}

/* Объявления */

/* 
 * Свзяь команд и функций
 */
struct file_operations devFops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release,    /* оно же close */
};

struct file_operations procFops = {
	.owner = THIS_MODULE,
	.read = proc_read,
	.write = proc_write,
};

/* 
 * Инициализация модуля
 */
int init_module()
{
    entry = proc_create(PROC_FILE_NAME, 0444, NULL, &procFops);
	printk(KERN_INFO "%s: proc file is created\n", PROC_FILE_NAME);


    if (alloc_chrdev_region(&first, 0, 1, DEVICE_FILE_NAME) < 0)
	  {
		return -1;
	  }
    if ((cl = class_create(THIS_MODULE, DEVICE_CLASS_NAME)) == NULL)
	  {
		unregister_chrdev_region(first, 1);
		return -1;
	  }
    if (device_create(cl, NULL, first, NULL, DEVICE_FILE_NAME) == NULL)
	  {
		class_destroy(cl);
		unregister_chrdev_region(first, 1);
		return -1;
	  }
    cdev_init(&c_dev, &devFops);
    if (cdev_add(&c_dev, first, 1) == -1)
	  {
		device_destroy(cl, first);
		class_destroy(cl);
		unregister_chrdev_region(first, 1);
		return -1;
	  }

	printk("%s The major device number is %d.\n",
			 "Registeration is a success", first);
	return 0;
}

/* 
 * Завершение работы модуля - дерегистрация файла в /proc 
 */
void cleanup_module()
{
    proc_remove(entry);    

	/* 
	 * Дерегистрация устройства
	 */
	cdev_del(&c_dev);
    device_destroy(cl, first);
    class_destroy(cl);
    unregister_chrdev_region(first, 1);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author");
MODULE_DESCRIPTION("The first kernel module");
MODULE_VERSION("0.1");
