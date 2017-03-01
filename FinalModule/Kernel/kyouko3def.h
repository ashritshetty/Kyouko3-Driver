#ifndef KYOUKO3_DEFINES_HEADER
#define KYOUKO3_DEFINES_HEADER

#include <linux/ioctl.h>

#define KYOUKO_MAJOR 500
#define KYOUKO_MINOR 127

#define CONTROL_SIZE 65536

//Define FIFO_ENTRIES as there are 1024 according to notes
#define FIFO_ENTRIES 1024

//Defines to keep track of graphic on/off
#define GRAPHICS_ON 1
#define GRAPHICS_OFF 0

#define CONFIG_ACC_DEF 0x80000000
#define CONFIG_ACC_MASK 0x40000000
#define CONFIG_MODE_SET_VAL 0
#define FIFO_CLEAR_BUF_VAL 0x03
#define FIFO_FLUSH_REG_VAL 0x0

#define CONTROL_OFF 0
#define FB_OFF 0x80000000

#define NUM_DMA_BUF 8
#define DMA_BUF_SIZE 126976u //1024*124 = (124K)

#define PCI_VENDOR_ID_CCORSI 0x1234
#define PCI_DEVICE_ID_CCORSI_KYOUKO3 0x1113
#define DEVICE_RAM 0x0020

//Define offsets for FiFo Start and End => H/w start and end
#define FIFO_START 0x1020
#define FIFO_END 0x1024
#define FIFO_HEAD 0x4010
#define FIFO_TAIL 0x4014

#define VERTEX_COORD 0x5000
#define VERTEX_COLOR 0x5010
#define CLEAR_COLOR 0x5100

#define CONFIG_MODE_SET 0x1008
#define CONFIG_ACC 0x1010

#define FRAME_COL 0x8000
#define FRAME_ROW 0x8004
#define FRAME_RPITCH 0x8008
#define FRAME_PIXFORMAT 0x800c
#define FRAME_STARTADDR 0x8010

#define ENC_WIDTH 0x9000
#define ENC_HEIGHT 0x9004
#define ENC_OFFSETX 0x9008
#define ENC_OFFSETY 0x900c
#define ENC_FRAME 0x9010

#define FIFO_CLEAR_BUF 0x3008
#define FIFO_FLUSH_REG 0x3ffc

#define DMA_BUF_ADDR_A 0x2000
#define DMA_BUF_CONF_A 0x2008

#define INTR_SET 0x100C
#define INTR_STATUS 0x4008

#define RASTER_PRIMITIVE 0x3000
#define RASTER_EMIT 0x3004

#define VMODE _IOW(0xcc, 0, unsigned long)
#define BIND_DMA _IOW(0xcc, 1, unsigned long)
#define START_DMA _IOWR(0xcc, 2, unsigned long)
#define FIFO_QUEUE _IOWR(0xcc, 3, unsigned long)
#define FIFO_FLUSH _IO(0xcc, 4)
#define UNBIND_DMA _IOW(0xcc, 5, unsigned long)

#endif //KYOUKO3_DEFINES_HEADER
