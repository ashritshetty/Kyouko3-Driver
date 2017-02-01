#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define KYOUKO_MAJOR 500
#define KYOUKO_MINOR 127

MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Clemson Tigers");

int kyouko3_open(struct inode *inode, struct file *fp)
{
  printk(KERN_ALERT "[KERNEL] Successfully opened device");
  return 0;
}

int kyouko3_release(struct inode *inode, struct file *fp)
{
  printk(KERN_ALERT "[KERNEL] Successfully released device");
  return 0;
}

struct file_operations kyouko3_fops = {
  .open = kyouko3_open,
  .release = kyouko3_release,
  .owner = THIS_MODULE
};

struct cdev kyouko3_cdev;

int my_init_function(void)
{
  int ret = -1;
  cdev_init(&kyouko3_cdev, &kyouko3_fops);
  kyouko3_cdev.owner = THIS_MODULE;
  ret = cdev_add(&kyouko3_cdev, MKDEV(KYOUKO_MAJOR, KYOUKO_MINOR), 1);
  if (ret == 0)
  {
    printk(KERN_ALERT "[KERNEL] Successfully initialized");
  }
  return ret;
}

void my_exit_function(void)
{
  cdev_del(&kyouko3_cdev);
  printk(KERN_ALERT "[KERNEL] Successfully exiting");
}

module_init(my_init_function);
module_exit(my_exit_function);
