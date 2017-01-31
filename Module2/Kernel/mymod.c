#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/mm.h>
#include <asm/io.h>
#include <linux/delay.h>

#define KYOUKO_MAJOR 500
#define KYOUKO_MINOR 127
#define PCI_VENDOR_ID_CCORSI 0x1234
#define PCI_DEVICE_ID_CCORSI_KYOUKO3 0x1113
#define DEVICE_RAM 0x0020
#define CONTROL_SIZE 65536

MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Clemson Tigers");

struct kyouko3 {
  unsigned long p_control_base;
  unsigned long p_ram_card_base;
  uint *k_control_base;
  uint *k_ram_card_base;
  struct pci_dev *kyouko3_pci_dev;
}kyouko3;

struct pci_device_id kyouko3_dev_ids[] = {
  {PCI_DEVICE(PCI_VENDOR_ID_CCORSI, PCI_DEVICE_ID_CCORSI_KYOUKO3)},
  {0}
};

unsigned int K_READ_REG(unsigned int reg)
{
  unsigned int value;
  udelay(1);
  rmb();
  value = *(kyouko3.k_control_base+(reg>>2));
  return(value);
}

void K_WRITE_REG(unsigned int reg, unsigned int value)
{
  udelay(1);
  *(kyouko3.k_control_base+(reg>>2)) = value;
}

int kyouko3_open(struct inode *inode, struct file *fp)
{
  unsigned int RAM_SIZE;
  RAM_SIZE = K_READ_REG(DEVICE_RAM);
  RAM_SIZE = RAM_SIZE*1024*1024;
  kyouko3.k_control_base = ioremap(kyouko3.p_control_base, CONTROL_SIZE);
  kyouko3.k_ram_card_base = ioremap(kyouko3.p_ram_card_base, RAM_SIZE);
  printk(KERN_ALERT "[KERNEL] Successfully opened device\n");
  return 0;
}

int kyouko3_release(struct inode *inode, struct file *fp)
{
  iounmap(kyouko3.k_control_base);
  iounmap(kyouko3.k_ram_card_base);
  printk(KERN_ALERT "[KERNEL] Successfully released device\n");
  return 0;
}

int kyouko3_mmap(struct file *fp, struct vm_area_struct *vma)
{
  int ret;
  ret = io_remap_pfn_range(vma, vma->vm_start, kyouko3.p_control_base>>PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot);
  return ret;
}

int kyouko3_probe(struct pci_dev *pci_dev, const struct pci_device_id *pci_id)
{
  int ret;
  kyouko3.p_control_base = pci_resource_start(pci_dev, 1);
  kyouko3.p_ram_card_base = pci_resource_start(pci_dev, 2);
  kyouko3.kyouko3_pci_dev = pci_dev;
  ret = pci_enable_device(pci_dev);
  pci_set_master(pci_dev);
  return ret;
}

void kyouko3_remove(struct pci_dev *pci_dev)
{
  pci_disable_device(pci_dev);
  pci_clear_master(pci_dev);
}

struct pci_driver kyouko3_pci_drv = {
  .name = "KYOUKO3",
  .id_table = kyouko3_dev_ids,
  .probe = kyouko3_probe,
  .remove = kyouko3_remove
};

struct file_operations kyouko3_fops = {
  .open = kyouko3_open,
  .release = kyouko3_release,
  .mmap = kyouko3_mmap,
  .owner = THIS_MODULE
};

struct cdev kyouko3_cdev;

int my_init_function(void)
{
  int ret = -1;
  cdev_init(&kyouko3_cdev, &kyouko3_fops);
  kyouko3_cdev.owner = THIS_MODULE;
  ret = cdev_add(&kyouko3_cdev, MKDEV(KYOUKO_MAJOR, KYOUKO_MINOR), 1);
  ret = pci_register_driver(&kyouko3_pci_drv);
  if (ret == 0)
  {
    printk(KERN_ALERT "[KERNEL] Successfully initialized\n");
  }
  return ret;
}

void my_exit_function(void)
{
  pci_unregister_driver(&kyouko3_pci_drv);
  cdev_del(&kyouko3_cdev);
  printk(KERN_ALERT "[KERNEL] Successfully exiting\n");
}

module_init(my_init_function);
module_exit(my_exit_function);

