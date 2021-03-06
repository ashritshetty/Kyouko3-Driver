#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/mman.h>
#include <asm/current.h>
#include <linux/spinlock.h>
#include "kyouko3def.h"

MODULE_LICENSE("Proprietary");
MODULE_AUTHOR("Clemson Tigers");

DECLARE_WAIT_QUEUE_HEAD(dma_snooze);

struct kyouko3_frame{
  unsigned int cols;
  unsigned int rows;
  unsigned int rowPitch;
  unsigned int pixFormat;
  unsigned int startAddr;
};

struct kyouko3_frame frame = {
  .cols = 1024,
  .rows = 768,
  .rowPitch = 1024*4,
  .pixFormat = 0xf888,
  .startAddr = 0
};

struct kyouko3_encoder{
    unsigned int width;
    unsigned int height;
    unsigned int offsetX;
    unsigned int offsetY;
    unsigned int frame;
};

struct kyouko3_encoder encoder = {
    .width = 1024,
    .height = 768,
    .offsetX = 0,
    .offsetY = 0,
    .frame = 0
};

struct fifo_entry{
    unsigned int cmd;
    unsigned int value;
};

struct fifo{
    dma_addr_t p_base;
    struct fifo_entry *k_base;
    unsigned int head;
    unsigned int tail_cache;
};

struct dma_addr{
    dma_addr_t p_base;
    unsigned long* k_base;
    unsigned long u_base;
    unsigned int count;
};

struct kyouko3 {
  unsigned long p_control_base;
  unsigned long p_ram_card_base;

  unsigned int graphics_on;
  unsigned int dma_mapped;

  unsigned int *k_control_base;
  unsigned int *k_ram_card_base;
  struct pci_dev *kyouko3_pci_dev;

  struct fifo kyouko3_fifo;

  //This give the current mmap dma index value
  unsigned int curr_dma_mmap_index;
  
  //The index position to know current fill and drain
  unsigned int dma_fill;
  unsigned int dma_drain;
  
  //Suspend_state - This defines state of this the suspension
  //0 - Not suspended
  //1 - Being suspended but not yet suspended
  //2 - Suspended in START_DMA
  //3 - Suspended in UNBIND_DMA
  unsigned int suspend_state;
  
  //Defines if the DMA Queue is full or not
  unsigned int isQueueFull;
  
}kyouko3;

//Creating Pool of 8 DMA buf
struct dma_addr dma_buf[8] = {{0}};

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

  kyouko3.graphics_on = 0;
  kyouko3.dma_mapped = 0;

  kyouko3.dma_fill = 0;
  kyouko3.dma_drain = 0;
  
  kyouko3.suspend_state = 0;
  kyouko3.isQueueFull = 0;

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
  int ret = -1;
  unsigned int uid = current->cred->fsuid.val;	
  unsigned int offset;
  if (uid != 0){
      return ret;
  }
  offset = vma->vm_pgoff << PAGE_SHIFT;
  switch(offset){
    case CONTROL_OFF:
      ret = io_remap_pfn_range(vma, vma->vm_start, kyouko3.p_control_base>>PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot);
      printk(KERN_ALERT "[KERNEL] Control region mapped \n");
      break;
    case FB_OFF:
      ret = io_remap_pfn_range(vma, vma->vm_start, kyouko3.p_ram_card_base>>PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot);
      printk(KERN_ALERT "[KERNEL] Frame buffer mapped \n");
      break;
    default:
      ret = io_remap_pfn_range(vma, vma->vm_start, (dma_buf[kyouko3.curr_dma_mmap_index].p_base)>>PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot);
      printk(KERN_ALERT "[KERNEL] DMA buffer mapped \n");
      break;
  }
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

void kyouko3_fifo_flush(void)
{
  K_WRITE_REG(FIFO_HEAD, kyouko3.kyouko3_fifo.head);
  while(kyouko3.kyouko3_fifo.tail_cache != kyouko3.kyouko3_fifo.head)
  {
    kyouko3.kyouko3_fifo.tail_cache = K_READ_REG(FIFO_TAIL);
    schedule();
  }
}

void sync_kick_fifo(void)
{
    K_WRITE_REG(FIFO_HEAD, kyouko3.kyouko3_fifo.head);
}

void kyouko3_vmode(void)
{
    float red = 1.0;
    float blue = 1.0;
    float green = 1.0;
    float alpha = 0.0;
    //set Frame 0
    K_WRITE_REG(FRAME_COL, frame.cols);
    K_WRITE_REG(FRAME_ROW, frame.rows);
    K_WRITE_REG(FRAME_RPITCH, frame.rowPitch);
    K_WRITE_REG(FRAME_PIXFORMAT, frame.pixFormat);
    K_WRITE_REG(FRAME_STARTADDR, frame.startAddr);

    //set accelaration bit mask
    K_WRITE_REG(CONFIG_ACC, CONFIG_ACC_MASK);

    //set DAC/encoder
    K_WRITE_REG(ENC_WIDTH, encoder.width);
    K_WRITE_REG(ENC_HEIGHT, encoder.height);
    K_WRITE_REG(ENC_OFFSETX, encoder.offsetX);
    K_WRITE_REG(ENC_OFFSETY, encoder.offsetY);
    K_WRITE_REG(ENC_FRAME, encoder.frame);

    //write 0 to mode set register
    K_WRITE_REG(CONFIG_MODE_SET, CONFIG_MODE_SET_VAL);

    //msleep(n)
    msleep(10);

    //load clear color register using FIFO_WRITE
    FIFO_WRITE(CLEAR_COLOR, *(unsigned int *)&red);
    FIFO_WRITE(CLEAR_COLOR+0x0004, *(unsigned int *)&green);
    FIFO_WRITE(CLEAR_COLOR+0x0008, *(unsigned int *)&blue);
    FIFO_WRITE(CLEAR_COLOR+0x000c, *(unsigned int *)&alpha);

    //FIFO_WRITE 0x03 to clear buffer register
    FIFO_WRITE(FIFO_CLEAR_BUF, FIFO_CLEAR_BUF_VAL);

    //FIFO_WRITE 0x0 to flush register
    FIFO_WRITE(FIFO_FLUSH_REG, FIFO_FLUSH_REG_VAL);

    kyouko3_fifo_flush();
    kyouko3.graphics_on = 1;
}

void drainDMA(int count)
{
   FIFO_WRITE(DMA_BUF_ADDR_A, (dma_buf[kyouko3.dma_drain].p_base));
   FIFO_WRITE(DMA_BUF_CONF_A, count);
   sync_kick_fifo();
}

unsigned int getBufCnt(void)
{
    unsigned int bufCnt = (kyouko3.dma_fill >= kyouko3.dma_drain) ?
                           (kyouko3.dma_fill - kyouko3.dma_drain) :
                           (NUM_DMA_BUF - kyouko3.dma_drain + kyouko3.dma_fill);

    if(bufCnt == 0 && kyouko3.isQueueFull == 1)
    {
      bufCnt = NUM_DMA_BUF;
    }
                           
    return bufCnt;
}

irqreturn_t dma_intr(int irq, void *dev_id, struct pt_regs *regs)
{
  unsigned int iflags;
  DEFINE_SPINLOCK(mLock);
  unsigned long flags;
  
  iflags = K_READ_REG(INTR_STATUS);
  K_WRITE_REG(INTR_STATUS, (iflags & 0xf));
  
  if((iflags & 0x02) == 0)
    return IRQ_NONE;

  kyouko3.dma_drain = (kyouko3.dma_drain+1)%NUM_DMA_BUF;
  
  spin_lock_irqsave(&mLock, flags);
  if(kyouko3.isQueueFull == 1)  //
  {
      kyouko3.isQueueFull = 0;
      spin_unlock_irqrestore(&mLock, flags);
      if(kyouko3.suspend_state == 2){
        printk(KERN_ALERT "Wakeup call in state 2\n");  
        wake_up_interruptible(&dma_snooze);
      }
  }
  else
  {
    spin_unlock_irqrestore(&mLock, flags);
  }
  
  if(kyouko3.dma_fill != kyouko3.dma_drain)
  {
      drainDMA(dma_buf[kyouko3.dma_drain].count);
  }
  else if(kyouko3.suspend_state == 3){
      printk(KERN_ALERT "Wakeup call in state 3\n");  
      wake_up_interruptible(&dma_snooze);
  }

  return IRQ_HANDLED;
}

long kyouko3_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
  int ret, i;
  struct fifo_entry entry;
  switch(cmd){
      case FIFO_QUEUE:
        ret = copy_from_user(&entry, (struct fifo_entry*)arg, sizeof(struct fifo_entry));
        FIFO_WRITE(entry.cmd, entry.value);
        break;

      case FIFO_FLUSH:
        kyouko3_fifo_flush();
        break;

      case VMODE:
      {
        if((int)arg == GRAPHICS_ON)
        {
          kyouko3_vmode();
        }
        else if((int)arg == GRAPHICS_OFF)
        {
          kyouko3_fifo_flush();
          K_WRITE_REG(CONFIG_ACC, CONFIG_ACC_DEF);
          K_WRITE_REG(CONFIG_MODE_SET, CONFIG_MODE_SET_VAL);
          kyouko3.graphics_on = 0;
          msleep(10);
        }
        break;
      }

      case BIND_DMA:
          for(i = 0; i < NUM_DMA_BUF; ++i)
          {
              dma_buf[i].k_base = pci_alloc_consistent(kyouko3.kyouko3_pci_dev, DMA_BUF_SIZE, &dma_buf[i].p_base);
              kyouko3.curr_dma_mmap_index = i;
              dma_buf[i].u_base = vm_mmap(fp, 0, DMA_BUF_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, 0x10000000);
          }
         
          ret = copy_to_user((void __user*)arg, &(dma_buf[0].u_base), sizeof(unsigned long));

          //Enabled INTERRUPT HANDLER
          ret = pci_enable_msi(kyouko3.kyouko3_pci_dev);
          if(ret != 0)
          {
            printk(KERN_ALERT "[KERNEL] Error enabling message signaled interrupts\n");
            return -1;
          }
          ret = request_irq(kyouko3.kyouko3_pci_dev->irq, (irq_handler_t)dma_intr, IRQF_SHARED, "dma_intr", &kyouko3);
          if(ret != 0)
          {
            pci_disable_msi(kyouko3.kyouko3_pci_dev);
            printk(KERN_ALERT "[KERNEL] IRQ request failed\n");
            return -1;
          }
          K_WRITE_REG(INTR_SET, 0x02);
          break;

      case UNBIND_DMA:
      {
          DEFINE_SPINLOCK(mLock);
          unsigned long flags;
          unsigned int count;
          spin_lock_irqsave(&mLock, flags);
          
          count = getBufCnt();
          if(count != 0){
              kyouko3.suspend_state = 3;
          }
          spin_unlock_irqrestore(&mLock, flags);
          
          if(kyouko3.suspend_state == 3){
              printk(KERN_ALERT "Suspend call in state 3\n");  
              wait_event_interruptible(dma_snooze, (kyouko3.dma_fill == kyouko3.dma_drain && kyouko3.isQueueFull == 0));

              printk(KERN_ALERT "Got up from state 3\n");  
              spin_lock_irqsave(&mLock, flags);
              kyouko3.suspend_state = 0;
              spin_unlock_irqrestore(&mLock, flags);
          }
          
          for(i = 0; i < NUM_DMA_BUF; ++i)
          {
            vm_munmap(dma_buf[i].u_base, DMA_BUF_SIZE);
            pci_free_consistent(kyouko3.kyouko3_pci_dev, DMA_BUF_SIZE, dma_buf[i].k_base, dma_buf[i].p_base);
          }
          
          K_WRITE_REG(INTR_SET, 0x0);
          free_irq(kyouko3.kyouko3_pci_dev->irq, &kyouko3);
          pci_disable_msi(kyouko3.kyouko3_pci_dev);

          break;
      }
      case START_DMA:
      {
           unsigned long count = 0;
           DEFINE_SPINLOCK(mLock);
           unsigned long flags;
           
           ret = copy_from_user(&count, (unsigned long*)arg, sizeof(unsigned long));
           if(count == 0)
               return -1;

           spin_lock_irqsave(&mLock, flags);
           if(kyouko3.dma_fill == kyouko3.dma_drain)
           {
               //Queue was empty at this point
               spin_unlock_irqrestore(&mLock, flags);
               
               kyouko3.dma_fill = (kyouko3.dma_fill+1)%NUM_DMA_BUF;
               drainDMA(count);
               kyouko3.isQueueFull = 0;
               
               ret = copy_to_user((void __user*)arg, &(dma_buf[kyouko3.dma_fill].u_base), sizeof(unsigned long));
               return 0;
           }

           //System is busy therefore queueing the buffers
           dma_buf[kyouko3.dma_fill].count = count;
           kyouko3.dma_fill = (kyouko3.dma_fill+1)%NUM_DMA_BUF;

           if(kyouko3.dma_fill == kyouko3.dma_drain)
           {
               kyouko3.suspend_state = 1;
               kyouko3.isQueueFull = 1;
           }
           spin_unlock_irqrestore(&mLock, flags);
           
           if(kyouko3.suspend_state == 1)
           {
               kyouko3.suspend_state = 2;
               
               printk(KERN_ALERT "Suspend call in state 2\n");  
               wait_event_interruptible(dma_snooze, ((kyouko3.dma_fill != kyouko3.dma_drain) || 
                                                     ((kyouko3.dma_fill == kyouko3.dma_drain) && kyouko3.isQueueFull == 0)));
               printk(KERN_ALERT "Got up from state 2\n");  
               kyouko3.suspend_state = 0;
           }
           
           ret = copy_to_user((void __user*)arg, &(dma_buf[kyouko3.dma_fill].u_base), sizeof(unsigned long));
           break;
      }
  }
  return 0;
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
  .unlocked_ioctl = kyouko3_ioctl,
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
