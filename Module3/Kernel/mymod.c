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

//Define offsets for FiFo Start and End => H/w start and end
#define FIFO_START 0x1020
#define FIFO_END 0x1024

//Define FIFO_ENTRIES as there are 1024 according to notes
#define FIFO_ENTRIES 1024

//Defines to keep track of graphic and DMA on/off
#define GRAPHICS_ON 0
#define DMA_MAPPED 0

MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Clemson Tigers");

struct fifo_entry{
    unsigned int cmd;
    unsigned int value;
}fifo_entry;

struct fifo{
    unsigned long p_base;
    struct fifo_entry *k_base;
    unsigned int head;
    unsigned int tail_cache;
}fifo;

struct kyouko3 {
  unsigned long p_control_base;
  unsigned long p_ram_card_base;
  unsigned int *k_control_base;
  unsigned int *k_ram_card_base;
  struct pci_dev *kyouko3_pci_dev;
  struct fifo kyouko3_fifo;
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

void FIFO_WRITE(unsigned int reg, unsigned int val)
{
  kyouko3.kyouko3_fifo.k_base[kyouko3.kyouko3_fifo.head].cmd = reg;
  kyouko3.kyouko3_fifo.k_base[kyouko3.kyouko3_fifo.head].value = val;
  
  kyouko3.kyouko3_fifo.head++;
  
  if(kyouko3.kyouko3_fifo.head >= FIFO_ENTRIES)
  {
      kyouko3.kyouko3_fifo.head = 0;
  }
}

int kyouko3_open(struct inode *inode, struct file *fp)
{
  unsigned int RAM_SIZE;
  
  kyouko3.k_control_base = ioremap(kyouko3.p_control_base, CONTROL_SIZE);
  
  RAM_SIZE = K_READ_REG(DEVICE_RAM);
  RAM_SIZE = RAM_SIZE*1024*1024;

  kyouko3.k_ram_card_base = ioremap(kyouko3.p_ram_card_base, RAM_SIZE);
  
  kyouko3.kyouko3_fifo.k_base = pci_alloc_consistent(kyouko3.kyouko3_pci_dev, 8192u, &kyouko3.kyouko3_fifo.p_base);
  K_WRITE_REG(FIFO_START, kyouko3.kyouko3_fifo.p_base);
  K_WRITE_REG(FIFO_END, kyouko3.kyouko3_fifo.p_base + 8192u);
  
  kyouko3.kyouko3_fifo.head = 0;
  kyouko3.kyouko3_fifo.tail_cache = 0;
  
  //TODO: Not handled graphics and DMA flags
  
  printk(KERN_ALERT "[KERNEL] Successfully opened device\n");
  return 0;
}

int kyouko3_release(struct inode *inode, struct file *fp)
{
  pci_free_consistent(kyouko3.kyouko3_pci_dev, 8192u, kyouko3.kyouko3_fifo.k_base, kyouko3.kyouko3_fifo.p_base);  
  
  iounmap(kyouko3.k_ram_card_base);  
  iounmap(kyouko3.k_control_base);
  
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

