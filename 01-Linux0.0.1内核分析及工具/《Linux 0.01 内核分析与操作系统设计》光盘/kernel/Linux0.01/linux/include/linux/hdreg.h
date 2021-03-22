/*
 * This file contains some defines for the AT-hd-controller.
 * Various sources. Check out some definitions (see comments with
 * a ques).
 */
#ifndef _HDREG_H
#define _HDREG_H

/* currently supports only 1 hd, put type here */
#define HARD_DISK_TYPE 17

/*
 * Ok, hard-disk-type is currently hardcoded. Not beatiful,
 * but easier. We don't use BIOS for anything else, why should
 * we get HD-type from it? Get these values from Reference Guide.
 */

#if HARD_DISK_TYPE == 17
#define _CYL	977
#define _HEAD	5
#define __WPCOM	300
#define _LZONE	977
#define _SECT	17
#define _CTL	0
#elif HARD_DISK_TYPE == 18
#define _CYL	977
#define _HEAD	7
#define __WPCOM	(-1)
#define _LZONE	977
#define _SECT	17
#define _CTL	0
#else
#error Define HARD_DISK_TYPE and parameters, add your own entries as well
#endif

/* Controller wants just wp-com/4 */
#if __WPCOM >= 0
#define _WPCOM ((__WPCOM)>>2)
#else
#define _WPCOM __WPCOM
#endif

/* Hd controller regs. Ref: IBM AT Bios-listing */
#define HD_DATA		0x1f0	/* _CTL when writing */
#define HD_ERROR	0x1f1	/* see err-bits */
#define HD_NSECTOR	0x1f2	/* nr of sectors to read/write */
#define HD_SECTOR	0x1f3	/* starting sector */
#define HD_LCYL		0x1f4	/* starting cylinder */
#define HD_HCYL		0x1f5	/* high byte of starting cyl */
#define HD_CURRENT	0x1f6	/* 101dhhhh , d=drive, hhhh=head */
#define HD_STATUS	0x1f7	/* see status-bits */
#define HD_PRECOMP HD_ERROR	/* same io address, read=error, write=precomp */
#define HD_COMMAND HD_STATUS	/* same io address, read=status, write=cmd */

#define HD_CMD		0x3f6

/* Bits of HD_STATUS */
#define ERR_STAT	0x01
#define INDEX_STAT	0x02
#define ECC_STAT	0x04	/* Corrected error */
#define DRQ_STAT	0x08
#define SEEK_STAT	0x10
#define WRERR_STAT	0x20
#define READY_STAT	0x40
#define BUSY_STAT	0x80

/* Values for HD_COMMAND */
#define WIN_RESTORE		0x10
#define WIN_READ		0x20
#define WIN_WRITE		0x30
#define WIN_VERIFY		0x40
#define WIN_FORMAT		0x50
#define WIN_INIT		0x60
#define WIN_SEEK 		0x70
#define WIN_DIAGNOSE		0x90
#define WIN_SPECIFY		0x91

/* Bits for HD_ERROR */
#define MARK_ERR	0x01	/* Bad address mark ? */
#define TRK0_ERR	0x02	/* couldn't find track 0 */
#define ABRT_ERR	0x04	/* ? */
#define ID_ERR		0x10	/* ? */
#define ECC_ERR		0x40	/* ? */
#define	BBD_ERR		0x80	/* ? */

struct partition {
	unsigned char boot_ind;		/* 0x80 - active (unused) */
	unsigned char head;		/* ? */
	unsigned char sector;		/* ? */
	unsigned char cyl;		/* ? */
	unsigned char sys_ind;		/* ? */
	unsigned char end_head;		/* ? */
	unsigned char end_sector;	/* ? */
	unsigned char end_cyl;		/* ? */
	unsigned int start_sect;	/* starting sector counting from 0 */
	unsigned int nr_sects;		/* nr of sectors in partition */
};

#endif

struct hd_driveid {
	unsigned short	config;		/* lots of obsolete bit flags */
	unsigned short	cyls;		/* "physical" cyls */
	unsigned short	reserved2;	/* reserved (word 2) */
	unsigned short	heads;		/* "physical" heads */
	unsigned short	track_bytes;	/* unformatted bytes per track */
	unsigned short	sector_bytes;	/* unformatted bytes per sector */
	unsigned short	sectors;	/* "physical" sectors per track */
	unsigned short	vendor0;	/* vendor unique */
	unsigned short	vendor1;	/* vendor unique */
	unsigned short	vendor2;	/* vendor unique */
	unsigned char	serial_no[20];	/* 0 = not_specified */
	unsigned short	buf_type;
	unsigned short	buf_size;	/* 512 byte increments; 0 = not_specified */
	unsigned short	ecc_bytes;	/* for r/w long cmds; 0 = not_specified */
	unsigned char	fw_rev[8];	/* 0 = not_specified */
	unsigned char	model[40];	/* 0 = not_specified */
	unsigned char	max_multsect;	/* 0=not_implemented */
	unsigned char	vendor3;	/* vendor unique */
	unsigned short	dword_io;	/* 0=not_implemented; 1=implemented */
	unsigned char	vendor4;	/* vendor unique */
	unsigned char	capability;	/* bits 0:DMA 1:LBA 2:IORDYsw 3:IORDYsup*/
	unsigned short	reserved50;	/* reserved (word 50) */
	unsigned char	vendor5;	/* vendor unique */
	unsigned char	tPIO;		/* 0=slow, 1=medium, 2=fast */
	unsigned char	vendor6;	/* vendor unique */
	unsigned char	tDMA;		/* 0=slow, 1=medium, 2=fast */
	unsigned short	field_valid;	/* bits 0:cur_ok 1:eide_ok */
	unsigned short	cur_cyls;	/* logical cylinders */
	unsigned short	cur_heads;	/* logical heads */
	unsigned short	cur_sectors;	/* logical sectors per track */
	unsigned short	cur_capacity0;	/* logical total sectors on drive */
	unsigned short	cur_capacity1;	/*  (2 words, misaligned int)     */
	unsigned char	multsect;	/* current multiple sector count */
	unsigned char	multsect_valid;	/* when (bit0==1) multsect is ok */
	unsigned int	lba_capacity;	/* total number of sectors */
	unsigned short	dma_1word;	/* single-word dma info */
	unsigned short	dma_mword;	/* multiple-word dma info */
	unsigned short  eide_pio_modes; /* bits 0:mode3 1:mode4 */
	unsigned short  eide_dma_min;	/* min mword dma cycle time (ns) */
	unsigned short  eide_dma_time;	/* recommended mword dma cycle time (ns) */
	unsigned short  eide_pio;       /* min cycle time (ns), no IORDY  */
	unsigned short  eide_pio_iordy; /* min cycle time (ns), with IORDY */
	unsigned short  word69;
	unsigned short  word70;
	/* HDIO_GET_IDENTITY currently returns only words 0 through 70 */
	unsigned short  word71;
	unsigned short  word72;
	unsigned short  word73;
	unsigned short  word74;
	unsigned short  word75;
	unsigned short  word76;
	unsigned short  word77;
	unsigned short  word78;
	unsigned short  word79;
	unsigned short  word80;
	unsigned short  word81;
	unsigned short  command_sets;	/* bits 0:Smart 1:Security 2:Removable 3:PM */
	unsigned short  word83;		/* bits 14:Smart Enabled 13:0 zero */
	unsigned short  word84;
	unsigned short  word85;
	unsigned short  word86;
	unsigned short  word87;
	unsigned short  dma_ultra;
	unsigned short	word89;		/* reserved (word 89) */
	unsigned short	word90;		/* reserved (word 90) */
	unsigned short	word91;		/* reserved (word 91) */
	unsigned short	word92;		/* reserved (word 92) */
	unsigned short	word93;		/* reserved (word 93) */
	unsigned short	word94;		/* reserved (word 94) */
	unsigned short	word95;		/* reserved (word 95) */
	unsigned short	word96;		/* reserved (word 96) */
	unsigned short	word97;		/* reserved (word 97) */
	unsigned short	word98;		/* reserved (word 98) */
	unsigned short	word99;		/* reserved (word 99) */
	unsigned short	word100;	/* reserved (word 100) */
	unsigned short	word101;	/* reserved (word 101) */
	unsigned short	word102;	/* reserved (word 102) */
	unsigned short	word103;	/* reserved (word 103) */
	unsigned short	word104;	/* reserved (word 104) */
	unsigned short	word105;	/* reserved (word 105) */
	unsigned short	word106;	/* reserved (word 106) */
	unsigned short	word107;	/* reserved (word 107) */
	unsigned short	word108;	/* reserved (word 108) */
	unsigned short	word109;	/* reserved (word 109) */
	unsigned short	word110;	/* reserved (word 110) */
	unsigned short	word111;	/* reserved (word 111) */
	unsigned short	word112;	/* reserved (word 112) */
	unsigned short	word113;	/* reserved (word 113) */
	unsigned short	word114;	/* reserved (word 114) */
	unsigned short	word115;	/* reserved (word 115) */
	unsigned short	word116;	/* reserved (word 116) */
	unsigned short	word117;	/* reserved (word 117) */
	unsigned short	word118;	/* reserved (word 118) */
	unsigned short	word119;	/* reserved (word 119) */
	unsigned short	word120;	/* reserved (word 120) */
	unsigned short	word121;	/* reserved (word 121) */
	unsigned short	word122;	/* reserved (word 122) */
	unsigned short	word123;	/* reserved (word 123) */
	unsigned short	word124;	/* reserved (word 124) */
	unsigned short	word125;	/* reserved (word 125) */
	unsigned short	word126;	/* reserved (word 126) */
	unsigned short	word127;	/* reserved (word 127) */
	unsigned short	security;	/* bits 0:support 1:enabled 2:locked 3:frozen */
	unsigned short	reserved[127];
}id_first[1];
