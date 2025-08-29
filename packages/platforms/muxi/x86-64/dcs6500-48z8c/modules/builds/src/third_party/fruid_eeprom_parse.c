/*
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 * This program is kfree software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/module.h> /*
                            => module_init()
                            => module_exit()
                            => MODULE_LICENSE()
                            => MODULE_VERSION()
                            => MODULE_AUTHOR()
                            => struct module
                          */
#include <linux/init.h>  /*
                            => typedef int (*initcall_t)(void);
                                 Note: the 'initcall_t' function returns 0 when succeed.
                                       In the Linux kernel, error codes are negative numbers
                                       belonging to the set defined in <linux/errno.h>.
                            => typedef void (*exitcall_t)(void);
                            => __init
                            => __exit
                         */
#include <linux/moduleparam.h>  /*
                                  => moduleparam()
                                */
#include <linux/types.h>  /*
                             => dev_t  (u32)
                          */
#include <linux/kdev_t.h>  /*
                              => MAJOR()
                              => MINOR()
                           */
#include <linux/string.h>  /*
                              => void *memset()
                           */
#include <linux/slab.h>  /*
                            => void kfree()
                          */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>/*
                                => struct spinlock
                           */
#include <linux/io.h>

#include <uapi/mtd/mtd-abi.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
// #include <stdio.h>
// #include <errno.h>
// #include <string.h>
// #include <syslog.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include "fruid_eeprom_parse.h"
#include "libboundscheck/include/securec.h"

#define FIELD_TYPE(x)     ((x & (0x03 << 6)) >> 6)
#define FIELD_LEN(x)      (x & ~(0x03 << 6))
#define FIELD_EMPTY       "N/A"
#define NO_MORE_DATA_BYTE 0xC1
#define MAX_FIELD_LENGTH  63  // 6-bit for length

/* Unix time difference between 1970 and 1996. */
#define UNIX_TIMESTAMP_1996   820454400

/* Array for BCD Plus definition. */
const char bcd_plus_array[] = "0123456789 -.XXX";

/* Array for 6-Bit ASCII definition. */
const char * ascii_6bit[4] = {
	" !\"#$%&'()*+,-./",
	"0123456789:;<=>?",
	"@ABCDEFGHIJKLMNO",
	"PQRSTUVWXYZ[\\]^_"
};

/* List of all the Chassis types. */
const char * fruid_chassis_type [] = {
	"Other",                    /* 0x01 */
	"Unknown",                  /* 0x02 */
	"Desktop",                  /* 0x03 */
	"Low Profile Desktop",      /* 0x04 */
	"Pizza Box",                /* 0x05 */
	"Mini Tower",               /* 0x06 */
	"Tower",                    /* 0x07 */
	"Portable",                 /* 0x08 */
	"Laptop",                   /* 0x09 */
	"Notebook",                 /* 0x0A */
	"Hand Held",                /* 0x0B */
	"Docking Station",          /* 0x0C */
	"All in One",               /* 0x0D */
	"Sub Notebook",             /* 0x0E */
	"Space-saving",             /* 0x0F */
	"Lunch Box",                /* 0x10 */
	"Main Server Chassis",      /* 0x11 */
	"Expansion Chassis",        /* 0x12 */
	"SubChassis",               /* 0x13 */
	"Bus Expansion Chassis",    /* 0x14 */
	"Peripheral Chassis",       /* 0x15 */
	"RAID Chassis",             /* 0x16 */
	"Rack Mount Chassis",       /* 0x17 */
	"Sealed-case PC",           /* 0x18 */
	"Multi-system Chassis",     /* 0x19 */
	"Compact PCI",              /* 0x1A */
	"Advanced TCA",             /* 0x1B */
	"Blade",                    /* 0x1C */
	"Blade Enclosure",          /* 0x1D */
	"Tablet",                   /* 0x1E */
	"Convertible",              /* 0x1F */
	"Detachable"                /* 0x20 */
};

const char * fruid_field_all_opt[] = {
	"--CPN",
	"--CSN",
	"--CCD1",
	"--CCD2",
	"--CCD3",
	"--CCD4",
	"--BMD",
	"--BM",
	"--BP",
	"--BSN",
	"--BPN",
	"--BFI",
	"--BCD1",
	"--BCD2",
	"--BCD3",
	"--BCD4",
	"--PM",
	"--PN",
	"--PPN",
	"--PV",
	"--PSN",
	"--PAT",
	"--PFI",
	"--PCD1",
	"--PCD2",
	"--PCD3",
	"--PCD4"
};


/*
 * calculate_time - calculate time from the unix time stamp stored
 *
 * @mfg_time    : Unix timestamp since 1996
 *
 * returns char * for mfg_time_str
 * returns NULL for memory allocation failure
 */
static char * calculate_time(uint8_t * mfg_time)
{
	int len;
	struct rtc_time local;
	ktime_t unix_time = 0;
	char time_str_temp[30];
	char * mfg_time_str;

	unix_time = ((mfg_time[2] << 16) + (mfg_time[1] << 8) + mfg_time[0]) * 60;
	unix_time += UNIX_TIMESTAMP_1996;
	unix_time += (8*60*60);//beijin time
	local = rtc_ktime_to_tm(unix_time);
//	local = localtime(&unix_time);
//	char * str = asctime(local);

	sprintf_s(time_str_temp,sizeof(time_str_temp), "%04d-%02d-%02d %02d:%02d:%02d"
        ,local.tm_year+1900
        ,local.tm_mon+1
        ,local.tm_mday
        ,local.tm_hour
        ,local.tm_min
        ,local.tm_sec);
	len = strlen(time_str_temp);

	mfg_time_str = (char *) kmalloc(len+1, GFP_KERNEL);
	if (!mfg_time_str) {
		printk("fruid: kmalloc: memory allocation failed\n");
		return NULL;
	}

	memset_s(mfg_time_str,len+1, 0, len+1);

	memcpy_s(mfg_time_str, len+1, time_str_temp, len);

	mfg_time_str[len] = '\0';

	return mfg_time_str;
}

/*
 * verify_chksum - verify the zero checksum of the data
 *
 * @area        : offset of the area
 * @len         : len of the area in bytes
 * @chksum_read : stored checksum in the data.
 *
 * returns 0 if chksum is verified
 * returns -1 if there exist a mismatch
 */
//todo
static int verify_chksum(uint8_t * area, uint8_t len, uint8_t chksum_read)
{
	int i;
	uint8_t chksum = 0;

	for (i = 0; i < len - 1; i++)
	chksum += area[i];

	/* Zero checksum calculation */
	chksum = ~(chksum) + 1;

	return (chksum == chksum_read) ? 0 : -1;
}

/*
 * get_chassis_type - get the Chassis type
 *
 * @type_hex  : type stored in the data
 *
 * returns char ptr for chassis type string
 * returns NULL if type not in the list
 */
static char * get_chassis_type(uint8_t type_hex)
{
	long type;
	char type_int[4];
	char * type_str;
	sprintf_s(type_int,sizeof(type_int), "%u", type_hex);
//	type = atoi(type_int) - 1;
	if(kstrtol(type_int, 10, &type) != 0)
		return NULL;
	type--;
	/* If the type is not in the list defined.*/
	if (type > FRUID_CHASSIS_TYPECODE_MAX || type < FRUID_CHASSIS_TYPECODE_MIN) {
		printk("fruid: chassis area: invalid chassis type\n");
		return NULL;
	}

	type_str = (char *) kmalloc(strlen(fruid_chassis_type[type])+1, GFP_KERNEL);
	if (!type_str) {
		printk("fruid: kmalloc: memory allocation failed\n");
		return NULL;
	}

	memcpy_s(type_str, strlen(fruid_chassis_type[type])+1, fruid_chassis_type[type], strlen(fruid_chassis_type[type]));
	type_str[strlen(fruid_chassis_type[type])] = '\0';

	return type_str;
}

/*
 * _fruid_area_field_read - read the field data
 *
 * @offset    : offset of the field
 *
 * returns char ptr for the field data string
 */
static char * _fruid_area_field_read(uint8_t *offset)
{
	int field_type, field_len, field_len_eff;
	int idx, idx_eff, val;
	char * field;

	/* Bits 7:6 */
	field_type = FIELD_TYPE(offset[0]);
	/* Bits 5:0 */
	field_len = FIELD_LEN(offset[0]);

	/* Calculate the effective length of the field data based on type stored. */
	switch (field_type) {

		case TYPE_BINARY:
			/* TODO: Need to add support to read data stored in binary type. */
			field_len_eff = 1;
			break;

		case TYPE_ASCII_6BIT:
			/*
			 * Every 3 bytes have four 6-bit packed values
			 * + 6-bit values from the remaining field bytes.
			 */
			field_len_eff = (field_len / 3) * 4 + (field_len % 3);
			break;

		case TYPE_BCD_PLUS:
		case TYPE_ASCII_8BIT:
			field_len_eff = field_len;
			break;
	}

	/* If field data is zero, store 'N/A' for that field. */
	field_len_eff > 0 ? (field = (char *) kmalloc(field_len_eff + 1, GFP_KERNEL)) :
			(field = (char *) kmalloc(strlen(FIELD_EMPTY), GFP_KERNEL));
	if (!field) {
		printk("fruid: kmalloc: memory allocation failed\n");
		return NULL;
	}


	if (field_len_eff < 1) {
		memset_s(field,strlen(FIELD_EMPTY), 0, strlen(FIELD_EMPTY));
		strcpy_s(field,strlen(FIELD_EMPTY), FIELD_EMPTY);
		return field;
	}
	memset_s(field,field_len_eff + 1, 0, field_len + 1);
	/* Retrieve field data depending on the type it was stored. */
	switch (field_type) {
		case TYPE_BINARY:
			/* TODO: Need to add support to read data stored in binary type. */
			break;

		case TYPE_BCD_PLUS:
			idx = 0;
			while (idx != field_len) {
				field[idx] = bcd_plus_array[offset[idx + 1] & 0x0F];
				idx++;
			}
			field[idx] = '\0';
			break;

		case TYPE_ASCII_6BIT:
			idx_eff = 0, idx = 1;

			while (field_len > 0) {

				/* 6-Bits => Bits 5:0 of the first byte */
				val = offset[idx] & 0x3F;
				field[idx_eff++] = ascii_6bit[(val & 0xF0) >> 4][val & 0x0F];
				field_len--;

				if (field_len > 0) {
					/* 6-Bits => Bits 3:0 of second byte + Bits 7:6 of first byte. */
					val = ((offset[idx] & 0xC0) >> 6) |
					((offset[idx + 1] & 0x0F) << 2);
					field[idx_eff++] = ascii_6bit[(val & 0xF0) >> 4][val & 0x0F];
					field_len--;
				}

				if (field_len > 0) {
					/* 6-Bits => Bits 1:0 of third byte + Bits 7:4 of second byte. */
					val = ((offset[idx + 1] & 0xF0) >> 4) |
					((offset[idx + 2] & 0x03) << 4);
					field[idx_eff++] = ascii_6bit[(val & 0xF0) >> 4][val & 0x0F];

					/* 6-Bits => Bits 7:2 of third byte. */
					val = ((offset[idx + 2] & 0xFC) >> 2);
					field[idx_eff++] = ascii_6bit[(val & 0xF0) >> 4][val & 0x0F];

					field_len--;
					idx += 3;
				}
			}
			/* Add Null terminator */
			field[idx_eff] = '\0';
			break;

		case TYPE_ASCII_8BIT:
			memcpy_s(field, field_len_eff + 1, offset + 1, field_len);
			/* Add Null terminator */
			field[field_len] = '\0';
			break;
	}

	return field;
}

/* Free all the memory allocated for fruid information */
void free_fruid_info(fruid_info_t * fruid)
{
	if (fruid->chassis.flag) {
		kfree(fruid->chassis.type_str);
		kfree(fruid->chassis.part);
		kfree(fruid->chassis.serial);
		kfree(fruid->chassis.custom1);
		kfree(fruid->chassis.custom2);
		kfree(fruid->chassis.custom3);
		kfree(fruid->chassis.custom4);
	}

	if (fruid->board.flag) {
		kfree(fruid->board.mfg_time_str);
		kfree(fruid->board.mfg_time);
		kfree(fruid->board.mfg);
		kfree(fruid->board.name);
		kfree(fruid->board.serial);
		kfree(fruid->board.part);
		kfree(fruid->board.fruid);
		kfree(fruid->board.custom1);
		kfree(fruid->board.custom2);
		kfree(fruid->board.custom3);
		kfree(fruid->board.custom4);
	}

	if (fruid->product.flag) {
		kfree(fruid->product.mfg);
		kfree(fruid->product.name);
		kfree(fruid->product.part);
		kfree(fruid->product.version);
		kfree(fruid->product.serial);
		kfree(fruid->product.asset_tag);
		kfree(fruid->product.fruid);
		kfree(fruid->product.custom1);
		kfree(fruid->product.custom2);
		kfree(fruid->product.custom3);
		kfree(fruid->product.custom4);
	}
}
EXPORT_SYMBOL_GPL(free_fruid_info);

/* Initialize the fruid information struct */
static void init_fruid_info(fruid_info_t * fruid)
{
	fruid->chassis.flag = 0;
	fruid->board.flag = 0;
	fruid->product.flag = 0;
	fruid->chassis.type_str = NULL;
	fruid->chassis.part_type_len = 0;
	fruid->chassis.part = NULL;
	fruid->chassis.serial_type_len = 0;
	fruid->chassis.serial = NULL;
	fruid->chassis.custom1_type_len = 0;
	fruid->chassis.custom1 = NULL;
	fruid->chassis.custom2_type_len = 0;
	fruid->chassis.custom2 = NULL;
	fruid->chassis.custom3_type_len = 0;
	fruid->chassis.custom3 = NULL;
	fruid->chassis.custom4_type_len = 0;
	fruid->chassis.custom4 = NULL;
	fruid->chassis.chksum = 0;
	fruid->board.mfg_time_str = NULL;
	fruid->board.mfg_time = NULL;
	fruid->board.mfg_type_len = 0;
	fruid->board.mfg = NULL;
	fruid->board.name_type_len = 0;
	fruid->board.name = NULL;
	fruid->board.serial_type_len = 0;
	fruid->board.serial = NULL;
	fruid->board.part_type_len = 0;
	fruid->board.part = NULL;
	fruid->board.fruid_type_len = 0;
	fruid->board.fruid = NULL;
	fruid->board.custom1_type_len = 0;
	fruid->board.custom1 = NULL;
	fruid->board.custom2_type_len = 0;
	fruid->board.custom2 = NULL;
	fruid->board.custom3_type_len = 0;
	fruid->board.custom3 = NULL;
	fruid->board.custom4_type_len = 0;
	fruid->board.custom4 = NULL;
	fruid->board.chksum = 0;
	fruid->product.mfg_type_len = 0;
	fruid->product.mfg = NULL;
	fruid->product.name_type_len = 0;
	fruid->product.name = NULL;
	fruid->product.part_type_len = 0;
	fruid->product.part = NULL;
	fruid->product.version_type_len = 0;
	fruid->product.version = NULL;
	fruid->product.serial_type_len = 0;
	fruid->product.serial = NULL;
	fruid->product.asset_tag_type_len = 0;
	fruid->product.asset_tag = NULL;
	fruid->product.fruid_type_len = 0;
	fruid->product.fruid = NULL;
	fruid->product.custom1_type_len = 0;
	fruid->product.custom1 = NULL;
	fruid->product.custom2_type_len = 0;
	fruid->product.custom2 = NULL;
	fruid->product.custom3_type_len = 0;
	fruid->product.custom3 = NULL;
	fruid->product.custom4_type_len = 0;
	fruid->product.custom4 = NULL;
	fruid->product.chksum = 0;
}

/* Parse the Product area data */
int parse_fruid_area_product(uint8_t * product,
      fruid_area_product_t * fruid_product)
{
	int ret, index;

	index = 0;

	/* Reset the struct to zero */
	memset_s(fruid_product,sizeof(fruid_area_product_t), 0, sizeof(fruid_area_product_t));

	/* Check if the format version is as per IPMI FRUID v1.0 format spec */
	fruid_product->format_ver = product[index++];
	if (fruid_product->format_ver != FRUID_FORMAT_VER) {
		printk("fruid: product_area: format version not supported");
		return EPROTONOSUPPORT;
	}

	fruid_product->area_len = product[index++] * FRUID_AREA_LEN_MULTIPLIER;
	fruid_product->lang_code = product[index++];

	fruid_product->chksum = product[fruid_product->area_len - 1];
	ret = verify_chksum((uint8_t *) product,
			fruid_product->area_len, fruid_product->chksum);

	if (ret) {
		printk("fruid: product_area: chksum not verified.");
		return EBADF;
	}

	fruid_product->mfg_type_len = product[index];
	fruid_product->mfg = _fruid_area_field_read(&product[index]);
	if (fruid_product->mfg == NULL)
		return ENOMEM;
	index += FIELD_LEN(product[index]) + 1;

	fruid_product->name_type_len = product[index];
	fruid_product->name = _fruid_area_field_read(&product[index]);
	if (fruid_product->name == NULL)
		return ENOMEM;
	index += FIELD_LEN(product[index]) + 1;

	fruid_product->part_type_len = product[index];
	fruid_product->part = _fruid_area_field_read(&product[index]);
	if (fruid_product->part == NULL)
		return ENOMEM;
	index += FIELD_LEN(product[index]) + 1;

	fruid_product->version_type_len = product[index];
	fruid_product->version = _fruid_area_field_read(&product[index]);
	if (fruid_product->version == NULL)
		return ENOMEM;
	index += FIELD_LEN(product[index]) + 1;

	fruid_product->serial_type_len = product[index];
	fruid_product->serial = _fruid_area_field_read(&product[index]);
	if (fruid_product->serial == NULL)
		return ENOMEM;
	index += FIELD_LEN(product[index]) + 1;

	fruid_product->asset_tag_type_len = product[index];
	fruid_product->asset_tag = _fruid_area_field_read(&product[index]);
	if (fruid_product->asset_tag == NULL)
		return ENOMEM;
	index += FIELD_LEN(product[index]) + 1;

	fruid_product->fruid_type_len = product[index];
	fruid_product->fruid = _fruid_area_field_read(&product[index]);
	if (fruid_product->fruid == NULL)
		return ENOMEM;
	index += FIELD_LEN(product[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_product->custom1_type_len = product[index];
	if (product[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_product->custom1 = _fruid_area_field_read(&product[index]);
	if (fruid_product->custom1 == NULL)
		return ENOMEM;
	index += FIELD_LEN(product[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_product->custom2_type_len = product[index];
	if (product[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_product->custom2 = _fruid_area_field_read(&product[index]);
	if (fruid_product->custom2 == NULL)
		return ENOMEM;
	index += FIELD_LEN(product[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_product->custom3_type_len = product[index];
	if (product[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_product->custom3 = _fruid_area_field_read(&product[index]);
	if (fruid_product->custom3 == NULL)
		return ENOMEM;
	index += FIELD_LEN(product[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_product->custom4_type_len = product[index];
	if (product[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_product->custom4 = _fruid_area_field_read(&product[index]);
	if (fruid_product->custom4 == NULL)
		return ENOMEM;

	return 0;
}

/* Parse the Board area data */
int parse_fruid_area_board(uint8_t * board,
		fruid_area_board_t * fruid_board)
{
	int ret, index, i;

	index = 0;

	/* Reset the struct to zero */
	memset_s(fruid_board,sizeof(fruid_area_board_t), 0, sizeof(fruid_area_board_t));

	/* Check if the format version is as per IPMI FRUID v1.0 format spec */
	fruid_board->format_ver = board[index++];
	if (fruid_board->format_ver != FRUID_FORMAT_VER) {
		printk("fruid: board_area: format version not supported");
		return EPROTONOSUPPORT;
	}
	fruid_board->area_len = board[index++] * FRUID_AREA_LEN_MULTIPLIER;
	fruid_board->lang_code = board[index++];

	fruid_board->chksum = board[fruid_board->area_len - 1];
	ret = verify_chksum((uint8_t *) board,
			fruid_board->area_len, fruid_board->chksum);

	if (ret) {
		printk("fruid: board_area: chksum not verified.");
		return EBADF;
	}

	fruid_board->mfg_time = (uint8_t *) kmalloc(3*sizeof(uint8_t), GFP_KERNEL);
	if (fruid_board->mfg_time == NULL)
		return ENOMEM;
	for (i = 0; i < 3; i++) {
		fruid_board->mfg_time[i] = board[index++];
	}

	fruid_board->mfg_time_str = calculate_time(fruid_board->mfg_time);
	if (fruid_board->mfg_time_str == NULL)
		return ENOMEM;

	fruid_board->mfg_type_len = board[index];
	fruid_board->mfg = _fruid_area_field_read(&board[index]);
	if (fruid_board->mfg == NULL)
		return ENOMEM;
	index += FIELD_LEN(board[index]) + 1;

	fruid_board->name_type_len = board[index];
	fruid_board->name = _fruid_area_field_read(&board[index]);
	if (fruid_board->name == NULL)
		return ENOMEM;
	index += FIELD_LEN(board[index]) + 1;

	fruid_board->serial_type_len = board[index];
	fruid_board->serial = _fruid_area_field_read(&board[index]);
	if (fruid_board->serial == NULL)
		return ENOMEM;
	index += FIELD_LEN(board[index]) + 1;

	fruid_board->part_type_len = board[index];
	fruid_board->part = _fruid_area_field_read(&board[index]);
	if (fruid_board->part == NULL)
		return ENOMEM;
	index += FIELD_LEN(board[index]) + 1;

	fruid_board->fruid_type_len = board[index];
	fruid_board->fruid = _fruid_area_field_read(&board[index]);
	if (fruid_board->fruid == NULL)
		return ENOMEM;
	index += FIELD_LEN(board[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_board->custom1_type_len = board[index];
	if (board[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_board->custom1 = _fruid_area_field_read(&board[index]);
	if (fruid_board->custom1 == NULL)
		return ENOMEM;
	index += FIELD_LEN(board[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_board->custom2_type_len = board[index];
	if (board[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_board->custom2 = _fruid_area_field_read(&board[index]);
	if (fruid_board->custom2 == NULL)
		return ENOMEM;
	index += FIELD_LEN(board[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_board->custom3_type_len = board[index];
	if (board[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_board->custom3 = _fruid_area_field_read(&board[index]);
	if (fruid_board->custom3 == NULL)
		return ENOMEM;
	index += FIELD_LEN(board[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_board->custom4_type_len = board[index];
	if (board[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_board->custom4 = _fruid_area_field_read(&board[index]);
	if (fruid_board->custom4 == NULL)
		return ENOMEM;

	return 0;
}

/* Parse the Chassis area data */
int parse_fruid_area_chassis(uint8_t * chassis,
		fruid_area_chassis_t * fruid_chassis)
{
	int ret, index;

	index = 0;

	/* Reset the struct to zero */
	memset_s(fruid_chassis,sizeof(fruid_area_chassis_t), 0, sizeof(fruid_area_chassis_t));

	/* Check if the format version is as per IPMI FRUID v1.0 format spec */
	fruid_chassis->format_ver = chassis[index++];
	if (fruid_chassis->format_ver != FRUID_FORMAT_VER) {
		printk("fruid: chassis_area: format version not supported");
		return EPROTONOSUPPORT;
	}

	fruid_chassis->area_len = chassis[index++] * FRUID_AREA_LEN_MULTIPLIER;
	fruid_chassis->type = chassis[index++];

	fruid_chassis->chksum = chassis[fruid_chassis->area_len - 1];
	ret = verify_chksum((uint8_t *) chassis,
			fruid_chassis->area_len, fruid_chassis->chksum);
	if (ret) {
		printk("fruid: chassis_area: chksum not verified.");
		return EBADF;
	}

	fruid_chassis->type_str = get_chassis_type(fruid_chassis->type);
	if (fruid_chassis->type_str == NULL)
		return ENOMSG;

	fruid_chassis->part_type_len = chassis[index];
	fruid_chassis->part = _fruid_area_field_read(&chassis[index]);
	if (fruid_chassis->part == NULL)
		return ENOMEM;
	index += FIELD_LEN(chassis[index]) + 1;

	fruid_chassis->serial_type_len = chassis[index];
	fruid_chassis->serial = _fruid_area_field_read(&chassis[index]);
	if (fruid_chassis->serial == NULL)
		return ENOMEM;
	index += FIELD_LEN(chassis[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_chassis->custom1_type_len = chassis[index];
	if (chassis[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_chassis->custom1 = _fruid_area_field_read(&chassis[index]);
	if (fruid_chassis->custom1 == NULL)
		return ENOMEM;
	index += FIELD_LEN(chassis[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_chassis->custom2_type_len = chassis[index];
	if (chassis[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_chassis->custom2 = _fruid_area_field_read(&chassis[index]);
	if (fruid_chassis->custom2 == NULL)
		return ENOMEM;
	index += FIELD_LEN(chassis[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_chassis->custom3_type_len = chassis[index];
	if (chassis[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_chassis->custom3 = _fruid_area_field_read(&chassis[index]);
	if (fruid_chassis->custom3 == NULL)
		return ENOMEM;
	index += FIELD_LEN(chassis[index]) + 1;

	/* Check if this field was last and there is no more custom data */
	fruid_chassis->custom4_type_len = chassis[index];
	if (chassis[index] == NO_MORE_DATA_BYTE)
		return 0;
	fruid_chassis->custom4 = _fruid_area_field_read(&chassis[index]);
	if (fruid_chassis->custom4 == NULL)
		return ENOMEM;

	return 0;
}
//todo
/* Calculate the area offsets and populate the fruid_eeprom_t struct */
void set_fruid_eeprom_offsets(uint8_t * eeprom, fruid_header_t * header,
	fruid_eeprom_t * fruid_eeprom)
{
	fruid_eeprom->header = eeprom + 0x00;

	header->offset_area.chassis ? (fruid_eeprom->chassis = eeprom + \
	(header->offset_area.chassis * FRUID_OFFSET_MULTIPLIER)) : \
	(fruid_eeprom->chassis = NULL);

	header->offset_area.board ? (fruid_eeprom->board = eeprom + \
	(header->offset_area.board * FRUID_OFFSET_MULTIPLIER)) : \
	(fruid_eeprom->board = NULL);

	header->offset_area.product ? (fruid_eeprom->product = eeprom + \
	(header->offset_area.product * FRUID_OFFSET_MULTIPLIER)) : \
	(fruid_eeprom->product = NULL);

	header->offset_area.multirecord ? (fruid_eeprom->multirecord = eeprom + \
	(header->offset_area.multirecord * FRUID_OFFSET_MULTIPLIER)) : \
	(fruid_eeprom->multirecord = NULL);
}
//todo
/* Populate the common header struct */
int parse_fruid_header(uint8_t * eeprom, fruid_header_t * header)
{
	int ret;

	memcpy_s((uint8_t *)header, sizeof(fruid_header_t), (uint8_t *)eeprom, sizeof(fruid_header_t));
	ret = verify_chksum((uint8_t *) header,
			sizeof(fruid_header_t), header->chksum);
	if (ret) {
		printk("fruid: common_header: chksum not verified.");
		return EBADF;
	}

	return ret;
}
//todo
/* Parse the eeprom dump and populate the fruid info in struct */
int populate_fruid_info(fruid_eeprom_t * fruid_eeprom, fruid_info_t * fruid)
{
	int ret;

	/* Initial all the required fruid structures */
	fruid_area_chassis_t fruid_chassis;
	fruid_area_board_t fruid_board;
	fruid_area_product_t fruid_product;

	/* If Chassis area is present, parse and print it */
	if (fruid_eeprom->chassis) {
		ret = parse_fruid_area_chassis(fruid_eeprom->chassis, &fruid_chassis);
		if (!ret) {
			fruid->chassis.flag = 1;
			fruid->chassis.format_ver = fruid_chassis.format_ver;
			fruid->chassis.area_len = fruid_chassis.area_len;
			fruid->chassis.type = fruid_chassis.type;
			fruid->chassis.type_str = fruid_chassis.type_str;
			fruid->chassis.part_type_len = fruid_chassis.part_type_len;
			fruid->chassis.part = fruid_chassis.part;
			fruid->chassis.serial_type_len = fruid_chassis.serial_type_len;
			fruid->chassis.serial = fruid_chassis.serial;
			fruid->chassis.custom1_type_len = fruid_chassis.custom1_type_len;
			fruid->chassis.custom1 = fruid_chassis.custom1;
			fruid->chassis.custom2_type_len = fruid_chassis.custom2_type_len;
			fruid->chassis.custom2 = fruid_chassis.custom2;
			fruid->chassis.custom3_type_len = fruid_chassis.custom3_type_len;
			fruid->chassis.custom3 = fruid_chassis.custom3;
			fruid->chassis.custom4_type_len = fruid_chassis.custom4_type_len;
			fruid->chassis.custom4 = fruid_chassis.custom4;
			fruid->chassis.chksum = fruid_chassis.chksum;
		} else
			return ret;
	}

	/* If Board area is present, parse and print it */
	if (fruid_eeprom->board) {
		ret = parse_fruid_area_board(fruid_eeprom->board, &fruid_board);
		if (!ret) {
			fruid->board.flag = 1;
			fruid->board.format_ver = fruid_board.format_ver;
			fruid->board.area_len = fruid_board.area_len;
			fruid->board.lang_code = fruid_board.lang_code;
			fruid->board.mfg_time = fruid_board.mfg_time;
			fruid->board.mfg_time_str = fruid_board.mfg_time_str;
			fruid->board.mfg_type_len = fruid_board.mfg_type_len;
			fruid->board.mfg = fruid_board.mfg;
			fruid->board.name_type_len = fruid_board.name_type_len;
			fruid->board.name = fruid_board.name;
			fruid->board.serial_type_len = fruid_board.serial_type_len;
			fruid->board.serial = fruid_board.serial;
			fruid->board.part_type_len = fruid_board.part_type_len;
			fruid->board.part = fruid_board.part;
			fruid->board.fruid_type_len = fruid_board.fruid_type_len;
			fruid->board.fruid = fruid_board.fruid;
			fruid->board.custom1_type_len = fruid_board.custom1_type_len;
			fruid->board.custom1 = fruid_board.custom1;
			fruid->board.custom2_type_len = fruid_board.custom2_type_len;
			fruid->board.custom2 = fruid_board.custom2;
			fruid->board.custom3_type_len = fruid_board.custom3_type_len;
			fruid->board.custom3 = fruid_board.custom3;
			fruid->board.custom4_type_len = fruid_board.custom4_type_len;
			fruid->board.custom4 = fruid_board.custom4;
			fruid->board.chksum = fruid_board.chksum;
		} else
			return ret;
	}

	/* If Product area is present, parse and print it */
	if (fruid_eeprom->product) {
		ret = parse_fruid_area_product(fruid_eeprom->product, &fruid_product);
		if (!ret) {
			fruid->product.flag = 1;
			fruid->product.format_ver = fruid_product.format_ver;
			fruid->product.area_len = fruid_product.area_len;
			fruid->product.lang_code = fruid_product.lang_code;
			fruid->product.mfg_type_len = fruid_product.mfg_type_len;
			fruid->product.mfg = fruid_product.mfg;
			fruid->product.name_type_len = fruid_product.name_type_len;
			fruid->product.name = fruid_product.name;
			fruid->product.part_type_len = fruid_product.part_type_len;
			fruid->product.part = fruid_product.part;
			fruid->product.version_type_len = fruid_product.version_type_len;
			fruid->product.version = fruid_product.version;
			fruid->product.serial_type_len = fruid_product.serial_type_len;
			fruid->product.serial = fruid_product.serial;
			fruid->product.asset_tag_type_len = fruid_product.asset_tag_type_len;
			fruid->product.asset_tag = fruid_product.asset_tag;
			fruid->product.fruid_type_len = fruid_product.fruid_type_len;
			fruid->product.fruid = fruid_product.fruid;
			fruid->product.custom1_type_len = fruid_product.custom1_type_len;
			fruid->product.custom1 = fruid_product.custom1;
			fruid->product.custom2_type_len = fruid_product.custom2_type_len;
			fruid->product.custom2 = fruid_product.custom2;
			fruid->product.custom3_type_len = fruid_product.custom3_type_len;
			fruid->product.custom3 = fruid_product.custom3;
			fruid->product.custom4_type_len = fruid_product.custom4_type_len;
			fruid->product.custom4 = fruid_product.custom4;
			fruid->product.chksum = fruid_product.chksum;
		} else
			return ret;
	}

	return 0;
}


/* Populate the fruid from eeprom dump*/
int fruid_parse_eeprom(const uint8_t * eeprom, int eeprom_len, fruid_info_t * fruid)
{
	int ret = 0;

	/* Initial all the required fruid structures */
	fruid_header_t fruid_header;
	fruid_eeprom_t fruid_eeprom;

	memset_s(&fruid_header,sizeof(fruid_header_t), 0, sizeof(fruid_header_t));
	memset_s(&fruid_eeprom,sizeof(fruid_eeprom_t), 0, sizeof(fruid_eeprom_t));

	/* Parse the common header data */
	ret = parse_fruid_header((uint8_t *)eeprom, &fruid_header);
	if (ret) {
		printk("parse header error\n");
		return ret;
	}

	/* Calculate all the area offsets */
	set_fruid_eeprom_offsets((uint8_t * )eeprom, &fruid_header, &fruid_eeprom);

	init_fruid_info(fruid);
	/* Parse the eeprom and populate the fruid information */
	ret = populate_fruid_info(&fruid_eeprom, fruid);
	if (ret) {
		printk("parse fru info failed\n");
		/* Free the malloced memory for the fruid information */
		free_fruid_info(fruid);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(fruid_parse_eeprom);


static int __init fruid_eeprom_parse_init(void)
{
	// char myeeprom[]={
	// 	0x01, 0x00, 0x00, 0x01, 0x09, 0x12, 0x00, 0xe3,  0x01, 0x08, 0x00, 0x45, 0xaa, 0xc9, 0xc6, 0x41,
	// 	0x63, 0x63, 0x74, 0x6f, 0x6e, 0xce, 0x66, 0x61,  0x6e, 0x20, 0x74, 0x72, 0x61, 0x79, 0x20, 0x62,
	// 	0x6f, 0x61, 0x72, 0x64, 0xca, 0x41, 0x4a, 0x30,  0x37, 0x30, 0x33, 0x31, 0x30, 0x34, 0x38, 0xcd,
	// 	0x31, 0x34, 0x32, 0x30, 0x30, 0x30, 0x30, 0x30,  0x58, 0x58, 0x58, 0x58, 0x48, 0xc3, 0x4e, 0x2f,
	// 	0x41, 0xc3, 0x52, 0x30, 0x41, 0xc1, 0x00, 0x66,  0x01, 0x09, 0x00, 0xc6, 0x41, 0x63, 0x63, 0x74,
	// 	0x6f, 0x6e, 0xd2, 0x54, 0x48, 0x34, 0x31, 0x2d,  0x38, 0x30, 0x46, 0x41, 0x4e, 0x2d, 0x30, 0x31,
	// 	0x2d, 0x46, 0x2d, 0x41, 0x54, 0xcd, 0x43, 0x30,  0x54, 0x41, 0x31, 0x38, 0x35, 0x35, 0x30, 0x30,
	// 	0x30, 0x31, 0x48, 0xc3, 0x52, 0x30, 0x41, 0xca,  0x41, 0x4a, 0x30, 0x37, 0x30, 0x33, 0x31, 0x30,
	// 	0x34, 0x38, 0xc3, 0x4e, 0x2f, 0x41, 0xc3, 0x4e,  0x2f, 0x41, 0xc2, 0x30, 0x32, 0xc1, 0x00, 0xce,
	// 	0xc0, 0x02, 0xca, 0x3b, 0x39, 0x36, 0x31, 0x30,  0x30, 0x20, 0x2b, 0x2d, 0x31, 0x30, 0x25, 0xc0,
	// 	0x02, 0xcb, 0x02, 0x71, 0x31, 0x36, 0x39, 0x30,  0x30, 0x20, 0x2b, 0x2d, 0x31, 0x30, 0x25, 0xc0,
	// 	0x02, 0xcb, 0x0f, 0x64, 0x33, 0x30, 0x30, 0x30,  0x30, 0x20, 0x2b, 0x2d, 0x31, 0x30, 0x25, 0xc0,
	// 	0x02, 0xca, 0x33, 0x41, 0x35, 0x35, 0x35, 0x30,  0x20, 0x2b, 0x2d, 0x31, 0x30, 0x25, 0xc0, 0x02,
	// 	0xcb, 0x07, 0x6c, 0x31, 0x35, 0x35, 0x30, 0x30,  0x20, 0x2b, 0x2d, 0x31, 0x30, 0x25, 0xc0, 0x82,
	// 	0xcb, 0x04, 0xef, 0x32, 0x37, 0x35, 0x30, 0x30,  0x20, 0x2b, 0x2d, 0x31, 0x30, 0x25
	// };
	// fruid_info_t myfruid;
	// fruid_info_t *fruid = &myfruid;
	// int rev = 0;
	// int len = strlen(myeeprom);
	// char buf[1000] = {0};
	// long temp_input = 12100;
	// int multiplier = 1000;

	// rev = fruid_parse_eeprom((uint8_t *)myeeprom, len, fruid);

	// sprintf_s(buf,sizeof(buf), "%ld.%03ld\n", temp_input/multiplier, temp_input%multiplier);
	// printk("%s \n", buf);

	// if (fruid->chassis.flag) {
	// 	printk("%-27s: %s", "\nChassis Type",fruid->chassis.type_str);
	// 	printk("%-27s: %s", "\nChassis Part Number",fruid->chassis.part);
	// 	printk("%-27s: %s", "\nChassis Serial Number",fruid->chassis.serial);
	// 	if (fruid->chassis.custom1 != NULL)
	// 		printk("%-27s: %s", "\nChassis Custom Data 1",fruid->chassis.custom1);
	// 	if (fruid->chassis.custom2 != NULL)
	// 		printk("%-27s: %s", "\nChassis Custom Data 2",fruid->chassis.custom2);
	// 	if (fruid->chassis.custom3 != NULL)
	// 		printk("%-27s: %s", "\nChassis Custom Data 3",fruid->chassis.custom3);
	// 	if (fruid->chassis.custom4 != NULL)
	// 		printk("%-27s: %s", "\nChassis Custom Data 4",fruid->chassis.custom4);
	// }

	// if (fruid->board.flag) {
	// 	printk("%-27s: %s", "\nBoard Mfg Date",fruid->board.mfg_time_str);
	// 	printk("%-27s: %s", "\nBoard Mfg",fruid->board.mfg);
	// 	printk("%-27s: %s", "\nBoard Product",fruid->board.name);
	// 	printk("%-27s: %s", "\nBoard Serial",fruid->board.serial);
	// 	printk("%-27s: %s", "\nBoard Part Number",fruid->board.part);
	// 	printk("%-27s: %s", "\nBoard FRU ID",fruid->board.fruid);
	// 	if (fruid->board.custom1 != NULL)
	// 		printk("%-27s: %s", "\nBoard Custom Data 1",fruid->board.custom1);
	// 	if (fruid->board.custom2 != NULL)
	// 		printk("%-27s: %s", "\nBoard Custom Data 2",fruid->board.custom2);
	// 	if (fruid->board.custom3 != NULL)
	// 		printk("%-27s: %s", "\nBoard Custom Data 3",fruid->board.custom3);
	// 	if (fruid->board.custom4 != NULL)
	// 		printk("%-27s: %s", "\nBoard Custom Data 4",fruid->board.custom4);
	// }

	// if (fruid->product.flag) {
	// 	printk("%-27s: %s", "\nProduct Manufacturer",fruid->product.mfg);
	// 	printk("%-27s: %s", "\nProduct Name",fruid->product.name);
	// 	printk("%-27s: %s", "\nProduct Part Number",fruid->product.part);
	// 	printk("%-27s: %s", "\nProduct Version",fruid->product.version);
	// 	printk("%-27s: %s", "\nProduct Serial",fruid->product.serial);
	// 	printk("%-27s: %s", "\nProduct Asset Tag",fruid->product.asset_tag);
	// 	printk("%-27s: %s", "\nProduct FRU ID",fruid->product.fruid);
	// 	if (fruid->product.custom1 != NULL)
	// 		printk("%-27s: %s", "\nProduct Custom Data 1",fruid->product.custom1);
	// 	if (fruid->product.custom2 != NULL)
	// 		printk("%-27s: %s", "\nProduct Custom Data 2",fruid->product.custom2);
	// 	if (fruid->product.custom3 != NULL)
	// 		printk("%-27s: %s", "\nProduct Custom Data 3",fruid->product.custom3);
	// 	if (fruid->product.custom4 != NULL)
	// 		printk("%-27s: %s", "\nProduct Custom Data 4",fruid->product.custom4);
	// }
	// free_fruid_info(fruid);
	// printk("\n");
    return 0;
}

static void __exit fruid_eeprom_parse_exit(void)
{
    return;
}

MODULE_AUTHOR("Zhonghu Zhang <zhonghu_zhang@accton.com>");
MODULE_DESCRIPTION("Fruid eeprom parse");
MODULE_VERSION("0.0.0.1");
MODULE_LICENSE("GPL");

module_init(fruid_eeprom_parse_init);
module_exit(fruid_eeprom_parse_exit);