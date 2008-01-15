/*
* Driver interface to the ASIC Complasion chip on the iPAQ H3800
*
* Copyright 2001 Compaq Computer Corporation.
*
* This program is free software; you can redistribute it and/or modify 
* it under the terms of the GNU General Public License as published by 
* the Free Software Foundation; either version 2 of the License, or 
* (at your option) any later version. 
* 
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author:  Andrew Christian
*          <Andrew.Christian@compaq.com>
*          October 2001
*
* Restrutured June 2002
*/

#ifndef H3600_ASIC_MMC_H
#define H3600_ASIC_MMC_H

int  h3600_asic_mmc_init(void);
void h3600_asic_mmc_cleanup(void);
int  h3600_asic_mmc_suspend(void);
void h3600_asic_mmc_resume(void);

/****************************************************/
/* H3800, ASIC #1
 * This ASIC is accesed through ASIC #2, and
 * mapped into the 1c00 - 1f00 region
 */
#define H3800_ASIC1_OFFSET(s,x,y)   \
     (*((volatile s *) (_IPAQ_ASIC2_GPIO_Base + _H3800_ASIC1_ ## x ## _Base + (_H3800_ASIC1_ ## x ## _ ## y))))

#define _H3800_ASIC1_MMC_Base             0x1c00

#define _H3800_ASIC1_MMC_StartStopClock     0x00    /* R/W 8bit                                  */
#define _H3800_ASIC1_MMC_Status             0x04    /* R   See below, default 0x0040             */
#define _H3800_ASIC1_MMC_ClockRate          0x08    /* R/W 8bit, low 3 bits are clock divisor    */
#define _H3800_ASIC1_MMC_Revision           0x0c    /* R/W Write to this to reset the asic!      */
#define _H3800_ASIC1_MMC_SPIRegister        0x10    /* R/W 8bit, see below                       */
#define _H3800_ASIC1_MMC_CmdDataCont        0x14    /* R/W 8bit, write to start MMC adapter      */
#define _H3800_ASIC1_MMC_ResponseTimeout    0x18    /* R/W 8bit, clocks before response timeout  */
#define _H3800_ASIC1_MMC_ReadTimeout        0x1c    /* R/W 16bit, clocks before received data timeout */
#define _H3800_ASIC1_MMC_BlockLength        0x20    /* R/W 10bit */
#define _H3800_ASIC1_MMC_NumOfBlocks        0x24    /* R/W 16bit, in block mode, number of blocks  */
#define _H3800_ASIC1_MMC_InterruptMask      0x34    /* R/W 8bit */
#define _H3800_ASIC1_MMC_CommandNumber      0x38    /* R/W 6 bits */
#define _H3800_ASIC1_MMC_ArgumentH          0x3c    /* R/W 16 bits  */
#define _H3800_ASIC1_MMC_ArgumentL          0x40    /* R/W 16 bits */
#define _H3800_ASIC1_MMC_ResFifo            0x44    /* R   8 x 16 bits - contains response FIFO */
#define _H3800_ASIC1_MMC_DataBuffer         0x4c    /* R/W 16 bits?? */
#define _H3800_ASIC1_MMC_BufferPartFull     0x50    /* R/W 8 bits */

#define H3800_ASIC1_MMC_StartStopClock    H3800_ASIC1_OFFSET(  u8, MMC, StartStopClock )
#define H3800_ASIC1_MMC_Status            H3800_ASIC1_OFFSET( u16, MMC, Status )
#define H3800_ASIC1_MMC_ClockRate         H3800_ASIC1_OFFSET(  u8, MMC, ClockRate )
#define H3800_ASIC1_MMC_Revision          H3800_ASIC1_OFFSET( u16, MMC, Revision )
#define H3800_ASIC1_MMC_SPIRegister       H3800_ASIC1_OFFSET(  u8, MMC, SPIRegister )
#define H3800_ASIC1_MMC_CmdDataCont       H3800_ASIC1_OFFSET(  u8, MMC, CmdDataCont )
#define H3800_ASIC1_MMC_ResponseTimeout   H3800_ASIC1_OFFSET(  u8, MMC, ResponseTimeout )
#define H3800_ASIC1_MMC_ReadTimeout       H3800_ASIC1_OFFSET( u16, MMC, ReadTimeout )
#define H3800_ASIC1_MMC_BlockLength       H3800_ASIC1_OFFSET( u16, MMC, BlockLength )
#define H3800_ASIC1_MMC_NumOfBlocks       H3800_ASIC1_OFFSET( u16, MMC, NumOfBlocks )
#define H3800_ASIC1_MMC_InterruptMask     H3800_ASIC1_OFFSET(  u8, MMC, InterruptMask )
#define H3800_ASIC1_MMC_CommandNumber     H3800_ASIC1_OFFSET(  u8, MMC, CommandNumber )
#define H3800_ASIC1_MMC_ArgumentH         H3800_ASIC1_OFFSET( u16, MMC, ArgumentH )
#define H3800_ASIC1_MMC_ArgumentL         H3800_ASIC1_OFFSET( u16, MMC, ArgumentL )
#define H3800_ASIC1_MMC_ResFifo           H3800_ASIC1_OFFSET( u16, MMC, ResFifo )
#define H3800_ASIC1_MMC_DataBuffer        H3800_ASIC1_OFFSET( u16, MMC, DataBuffer )
#define H3800_ASIC1_MMC_BufferPartFull    H3800_ASIC1_OFFSET(  u8, MMC, BufferPartFull )

#define MMC_STOP_CLOCK                   (1 << 0)   /* Write to "StartStopClock" register */
#define MMC_START_CLOCK                  (1 << 1)

#define MMC_STATUS_READ_TIMEOUT          (1 << 0)
#define MMC_STATUS_RESPONSE_TIMEOUT      (1 << 1)
#define MMC_STATUS_CRC_WRITE_ERROR       (1 << 2)
#define MMC_STATUS_CRC_READ_ERROR        (1 << 3)
#define MMC_STATUS_SPI_READ_ERROR        (1 << 4)  /* SPI data token error received */
#define MMC_STATUS_CRC_RESPONSE_ERROR    (1 << 5)
#define MMC_STATUS_FIFO_EMPTY            (1 << 6)
#define MMC_STATUS_FIFO_FULL             (1 << 7)
#define MMC_STATUS_CLOCK_ENABLE          (1 << 8)  /* MultiMediaCard clock stopped */
#define MMC_STATUS_WR_CRC_ERROR_CODE     (3 << 9)  /* 2 bits wide */
#define MMC_STATUS_DATA_TRANSFER_DONE    (1 << 11) /* Write operation, indicates transfer finished */
#define MMC_STATUS_END_PROGRAM           (1 << 12) /* End write and read operations */
#define MMC_STATUS_END_COMMAND_RESPONSE  (1 << 13) /* End command response */

#define MMC_CLOCK_RATE_FULL              0
#define MMC_CLOCK_RATE_DIV_2             1
#define MMC_CLOCK_RATE_DIV_4             2
#define MMC_CLOCK_RATE_DIV_8             3
#define MMC_CLOCK_RATE_DIV_16            4
#define MMC_CLOCK_RATE_DIV_32            5
#define MMC_CLOCK_RATE_DIV_64            6

#define MMC_SPI_REG_SPI_ENABLE           (1 << 0)  /* Enables SPI mode */
#define MMC_SPI_REG_CRC_ON               (1 << 1)  /* 1:turn on CRC    */
#define MMC_SPI_REG_SPI_CS_ENABLE        (1 << 2)  /* 1:turn on SPI CS */
#define MMC_SPI_REG_CS_ADDRESS_MASK      0x38      /* Bits 3,4,5 are the SPI CS relative address */

#define MMC_CMD_DATA_CONT_FORMAT_NO_RESPONSE  0x00
#define MMC_CMD_DATA_CONT_FORMAT_R1           0x01
#define MMC_CMD_DATA_CONT_FORMAT_R2           0x02
#define MMC_CMD_DATA_CONT_FORMAT_R3           0x03
#define MMC_CMD_DATA_CONT_DATA_ENABLE         (1 << 2)  /* This command contains a data transfer */
#define MMC_CMD_DATA_CONT_READ                (0 << 3)  /* This data transfer is a read */
#define MMC_CMD_DATA_CONT_WRITE               (1 << 3)  /* This data transfer is a write */
#define MMC_CMD_DATA_CONT_STREAM_BLOCK        (1 << 4)  /* This data transfer is in stream mode */
#define MMC_CMD_DATA_CONT_MULTI_BLOCK         (1 << 5)
#define MMC_CMD_DATA_CONT_BC_MODE             (1 << 6)
#define MMC_CMD_DATA_CONT_BUSY_BIT            (1 << 7)  /* Busy signal expected after current cmd */
#define MMC_CMD_DATA_CONT_INITIALIZE          (1 << 8)  /* Enables the 80 bits for initializing card */
#define MMC_CMD_DATA_CONT_NO_COMMAND          (1 << 9)
#define MMC_CMD_DATA_CONT_WIDE_BUS            (1 << 10)
#define MMC_CMD_DATA_CONT_WE_WIDE_BUS         (1 << 11)
#define MMC_CMD_DATA_CONT_NOB_ON              (1 << 12)

#define MMC_INT_MASK_DATA_TRANSFER_DONE       (1 << 0)
#define MMC_INT_MASK_PROGRAM_DONE             (1 << 1)
#define MMC_INT_MASK_END_COMMAND_RESPONSE     (1 << 2)
#define MMC_INT_MASK_BUFFER_READY             (1 << 3)
#define MMC_INT_MASK_ALL                      (0xf)

#define MMC_BUFFER_PART_FULL                  (1 << 0)
#define MMC_BUFFER_LAST_BUF                   (1 << 1)


#endif /* H3600_ASIC_MMC_H */
