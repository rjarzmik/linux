

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/rslib.h>
#include <linux/moduleparam.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <linux/fs.h> 
#include <linux/cdev.h>
#include <asm/uaccess.h>


#define DoC_G3_IO             0x0800

#define DoC_G3_ChipID         0x1000
#define DoC_G3_DeviceIdSelect 0x100a
#define DoC_G3_Ctrl           0x100c

#define DoC_G3_CtrlConfirm    0x1072
#define DoC_G3_ReadAddress    0x101a
#define DoC_G3_FlashSelect    0x1032
#define DoC_G3_FlashCmd       0x1034
#define DoC_G3_FlashAddr      0x1036
#define DoC_G3_FlashCtrl      0x1038
#define DoC_G3_Nop            0x103e

#define DOC_MODE_RESET  0x0
#define DOC_MODE_NORMAL 0x1
#define DOC_MODE_MDWREN 0x4


#define DoC_G3_ID 0x200

#define CMD_FLASH_RESET  0xff
#define DoC_G3_IOREMAP_LEN  0x8000

static unsigned long doc_config_location = 0xffffffff;


#define DOCG3_MAJOR 254
#define DOCG3_MINOR 0

int major,minor;
#define SECTOR_SIZE 0x200
#define NUM_SECTOR  0xffff
#define CHIP_SIZE (0xffff*0x100)

module_param(doc_config_location, ulong, 0);
MODULE_PARM_DESC(doc_config_location, "Physical memory address at which to probe for DiskOnChipG3");

#define ReadDOC(adr,reg) ((unsigned char)(*(volatile __u32 *)(((unsigned long)adr)+((reg)))))
#define ReadDOC_16(adr,reg) ((__u16)(*(volatile __u32 *)(((unsigned long)adr)+((reg)))))
#define ReadDOC_32(adr,reg) ((__u32)(*(volatile __u32 *)(((unsigned long)adr)+((reg)))))
#define WriteDOC(d, adr, reg)  do{ *(volatile __u16 *)(((unsigned long)adr)+((reg))) = (__u16)d; wmb();} while(0)
#define WriteDOC_8(d, adr, reg)  do{ *(volatile unsigned char *)(((unsigned long)adr)+((reg))) = (unsigned char)d; wmb();} while(0)


struct doc_g3_dev {
	void __iomem *virtaddr;
	unsigned long physaddr;
	
	struct cdev cdev;
};
struct doc_g3_dev *g3;
static void docg3_nop(struct doc_g3_dev *g3) {
	WriteDOC(0xff,g3->virtaddr,DoC_G3_Nop);
}


static void docg3_wait_flash_completion(struct doc_g3_dev *g3) {
	__u32 c;
	int i=0;
	docg3_nop(g3);
	docg3_nop(g3);
	docg3_nop(g3);
	docg3_nop(g3);
	/* Prepare read */
	WriteDOC(g3->physaddr+DoC_G3_FlashCtrl,g3->virtaddr,DoC_G3_ReadAddress);
	c =ReadDOC(g3->virtaddr,DoC_G3_FlashCtrl); 
	while(((c & 0x01) != 0x01) && (i++<300)) { 
		//printk("%x(%x) ",c,c<<31);
		c =ReadDOC(g3->virtaddr,DoC_G3_FlashCtrl); 
	}
//	printk("\n");
}

static void docg3_flash_select(struct doc_g3_dev *g3,unsigned char f) {
	WriteDOC(f,g3->virtaddr,DoC_G3_FlashSelect);
}

static void docg3_flash_cmd(struct doc_g3_dev *g3,unsigned char c) {
	WriteDOC(c,g3->virtaddr,DoC_G3_FlashCmd);
	docg3_nop(g3);
	docg3_nop(g3);
}

static void docg3_reset(struct doc_g3_dev *g3) {
	WriteDOC( DOC_MODE_RESET|DOC_MODE_MDWREN,g3->virtaddr,DoC_G3_Ctrl);
	WriteDOC(~( DOC_MODE_RESET|DOC_MODE_MDWREN),g3->virtaddr, DoC_G3_CtrlConfirm);


	WriteDOC( DOC_MODE_NORMAL|DOC_MODE_MDWREN,g3->virtaddr,DoC_G3_Ctrl);
	WriteDOC( ~(DOC_MODE_NORMAL|DOC_MODE_MDWREN),g3->virtaddr, DoC_G3_CtrlConfirm);
	
}

static void docg3_set_read_offset(struct doc_g3_dev *g3,unsigned char offset) {
	WriteDOC_8(offset,g3->virtaddr,DoC_G3_FlashAddr);
	docg3_nop(g3);
}
static void docg3_set_read_addr(struct doc_g3_dev *g3, unsigned int addr) {
	unsigned char tmp;
	
	tmp = (unsigned char) (addr & 0xFF);
	//printk("%s: (%x,%x) %x ",__FUNCTION__,addr,offset,tmp);
	WriteDOC_8(tmp,g3->virtaddr,DoC_G3_FlashAddr);

	tmp = (unsigned char) ((addr>>8) & 0xFF);
	//printk("%x ",tmp);
	WriteDOC_8(tmp,g3->virtaddr,DoC_G3_FlashAddr);

	tmp = (unsigned char) ((addr>>16) & 0xFF);
	//printk("%x ",tmp);
	WriteDOC_8(tmp,g3->virtaddr,DoC_G3_FlashAddr);
	docg3_nop(g3);
	//docg3_set_read_offset(g3,offset);
	//printk("%x\n",offset);
}

static void docg3_enable_stuff(struct doc_g3_dev *g3) {
	WriteDOC(0x8a0f,g3->virtaddr,0x1040);
	docg3_nop(g3);
	docg3_nop(g3);
	docg3_nop(g3);
	docg3_nop(g3);
	docg3_nop(g3);
}

// Read the num the 512 bytes block of the chip flash 
static void docg3_read_sector(struct doc_g3_dev *g3,int chip,int num,unsigned int *buf) {
	unsigned int tmp,i,addr;
	addr= num%0x40 + 0x80*(num/0x40);
	//printk("docg3_read_sector: %x %x %x\n",chip, num, addr);
	do {
		docg3_flash_select(g3,0x00);
		docg3_flash_cmd(g3,CMD_FLASH_RESET);
		docg3_wait_flash_completion(g3);
		
		docg3_nop(g3);
		
		docg3_flash_select(g3,0x09);
		docg3_flash_cmd(g3,0xa2);
		docg3_flash_cmd(g3,0x22);


		docg3_flash_select(g3,0xe);
		docg3_flash_cmd(g3,0);
		docg3_flash_select(g3,0x12);
		if((0x0|addr)==(0x40|addr)) printk("loop for %x\n",addr);

		docg3_flash_cmd(g3,0x60);
		docg3_set_read_addr(g3,0x0|addr);
		

		docg3_flash_cmd(g3,0x60);
		docg3_set_read_addr(g3,0x40|addr);
		
	
		//docg3_set_read_addr(g3,0x80,0x80);
		WriteDOC_8(g3->physaddr+DoC_G3_FlashCtrl,g3->virtaddr,DoC_G3_ReadAddress);
		tmp = ReadDOC(g3->virtaddr,DoC_G3_FlashCtrl);
		//printk("%x %x\n",tmp,tmp & 0x06);
	} while(tmp & 0x06); //TODO: timeout
	
	docg3_flash_cmd(g3,0x30);
	docg3_wait_flash_completion(g3);
	docg3_flash_cmd(g3,0x05);
	if(addr & 1)
		docg3_set_read_offset(g3,0x84); // second block ?
	else
		docg3_set_read_offset(g3,0x00); // second block ?
		
	docg3_flash_cmd(g3,0xe0);
	docg3_enable_stuff(g3);

	WriteDOC(g3->physaddr+DoC_G3_IO,g3->virtaddr,DoC_G3_ReadAddress);

	for(i=0;i<128;i++) {
		tmp = ReadDOC_32(g3->virtaddr, DoC_G3_IO+4);
		buf[i] = tmp;
	}

	
}

int docg3_open(struct inode *inode, struct file *filp)
{
	printk("docg3_open\n");
	//dev = container_of(inode->i_cdev, struct doc_g3_dev, cdev);
	filp->private_data = g3; /* for other methods */

	return 0;          /* success */
}

int docg3_release(struct inode *inode, struct file *filp)
{
	printk("docg3_release\n");
	return 0;
}

int docg3_ioctl(struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
	return -ENOTTY;
}

ssize_t docg3_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct doc_g3_dev *dev = filp->private_data;
	int block=0;
	int chip=0;
	int n=SECTOR_SIZE;
	unsigned int buffer[128];
	loff_t cur = filp->f_pos;
	//printk("docg3_read: %d offset: %llx\n",count,filp->f_pos);

	//chip = ((unsigned int)filp->f_pos) / CHIP_SIZE; //todo
	if(cur >= 0x4000000) {
		printk("Failed for %llx %x\n",filp->f_pos,count);
		return -EIO;
	}
	if((cur + count) >  0x4000000 ) {
		printk("truncating  %llx %x",cur,count);
                count = 0x4000000 - cur;
		printk(" to %x\n",count);
	}
	// TODO:
	//  separate reques if over a chip change.
	do {
		block = (cur - chip * CHIP_SIZE) / SECTOR_SIZE; 
		//printk("docg3_read: %x %x %x\n",chip,block,(( int)filp->f_pos));

		//if(block> NUM_SECTOR) return -EFAULT;

		docg3_read_sector(dev,chip,block,buffer);

		if(count < SECTOR_SIZE) n = count;
		if (copy_to_user(buf,buffer,n))
		 return -EFAULT;
		//printk("return %d (requested %d)\n",n,count);
		*f_pos+=n;
		count-=n;
		buf +=n;
		cur +=n;

	}while (count >= SECTOR_SIZE );

	return cur - filp->f_pos;
	

}


loff_t docg3_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;
	printk("docg3_seek\n");
	switch(whence) {
	  case 0: /* SEEK_SET */
		newpos = off; 
		break;

	  case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	  default: 
		return -EINVAL;
	}
	if (newpos < 0) return -EINVAL;
	filp->f_pos = newpos;
	return newpos;
}


struct file_operations docg3_fops = {
        .owner =    THIS_MODULE,
        .llseek =   docg3_llseek,
        .read =     docg3_read,
        .ioctl =    docg3_ioctl,
        .open =     docg3_open,
        .release =  docg3_release,
};


static int __init init_docg3(void)
{
	static struct doc_g3_dev s_g3;
	unsigned int chipId;
	int i;
	dev_t dev=0;


	register_chrdev(DOCG3_MAJOR,"docG3",&docg3_fops);
	dev = MKDEV(DOCG3_MAJOR,DOCG3_MINOR);

	g3=&s_g3;
	g3->physaddr = doc_config_location;
	g3->virtaddr = ioremap(g3->physaddr, DoC_G3_IOREMAP_LEN);
	if(!g3->virtaddr ) {
		printk(KERN_ERR "DoC_G3 ioremap failed\n");
		return -EIO;
	}

	//devfs_mk_cdev(dev, S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP,"DocG3");
	cdev_init(&g3->cdev,&docg3_fops);
	g3->cdev.owner= THIS_MODULE;
	g3->cdev.ops = &docg3_fops;
	i = cdev_add(&g3->cdev,MKDEV(DOCG3_MAJOR,DOCG3_MINOR),1);
	if(i)
		printk("cdev_add failed\n");

	docg3_reset(g3);
	
	WriteDOC(g3->physaddr+DoC_G3_ChipID,g3->virtaddr,DoC_G3_ReadAddress);
	chipId = ReadDOC_16(g3->virtaddr, DoC_G3_ChipID);
	printk("Doc_G3: chip id=%x\n",chipId);
	if(chipId != DoC_G3_ID) return -1;


	printk("Starting flash stuff\n");
	
	WriteDOC(g3->physaddr+DoC_G3_ChipID,g3->virtaddr,DoC_G3_DeviceIdSelect);
	WriteDOC(0x39,g3->virtaddr,DoC_G3_FlashCtrl);

	printk("end\n");
	return 0;
}

static void __exit cleanup_docg3(void)
{
	cdev_del(&g3->cdev);
	unregister_chrdev_region(MKDEV(DOCG3_MAJOR,DOCG3_MINOR),1);	
}

module_init(init_docg3);
module_exit(cleanup_docg3);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thomas Cougnard <tgnard@free.fr>");
MODULE_DESCRIPTION("Test modules for Diskonchip G3 device description\n");
