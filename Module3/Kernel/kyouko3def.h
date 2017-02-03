#ifndef KYOUKO3_DEFINES_HEADER
#define KYOUKO3_DEFINES_HEADER

#include <linux/ioctl.h>

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

#define VMODE _IOW(0xcc, 0, unsigned long)
#define BIND_DMA _IOW(0xcc, 1, unsigned long)
#define START_DMA _IOWR(0xcc, 2, unsigned long)
#define FIFO_QUEUE _IOWR(0xcc, 3, unsigned long)
#define FIFO_FLUSH _IO(0xcc, 4)
#define UNBIND_DMA _IOW(0xcc, 5, unsigned long)

#endif //KYOUKO3_DEFINES_HEADER