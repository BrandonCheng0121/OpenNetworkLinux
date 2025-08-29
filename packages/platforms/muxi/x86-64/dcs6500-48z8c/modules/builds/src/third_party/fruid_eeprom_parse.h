/*
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
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

#ifndef __FRUID_H__
#define __FRUID_H__

#ifdef __cplusplus
extern "C" {
#endif

// #include <stdlib.h>
// #include <stdint.h>

// Type Definition
#define TYPE_BINARY          0
#define TYPE_BCD_PLUS        1
#define TYPE_ASCII_6BIT      2
#define TYPE_ASCII_8BIT      3

#define FRUID_FORMAT_VER           0x01
#define FRUID_OFFSET_MULTIPLIER    8
#define FRUID_AREA_LEN_MULTIPLIER  8

#define FRUID_CHASSIS_TYPECODE_MIN        0
#define FRUID_CHASSIS_TYPECODE_MAX        31

/* To hold the common header information. */
typedef struct fruid_header_t {
	uint8_t format_ver : 4;
	struct {
		uint8_t internal;
		uint8_t chassis;
		uint8_t board;
		uint8_t product;
		uint8_t multirecord;
	} offset_area;
	uint8_t pad;
	uint8_t chksum;
} fruid_header_t;

/* To hold the Chassis area information. */
typedef struct fruid_area_chassis_t {
	uint8_t format_ver : 4;
	uint8_t area_len;
	uint8_t type;
	char * type_str;
	uint8_t part_type_len;
	char * part;
	uint8_t serial_type_len;
	char * serial;
	uint8_t custom1_type_len;
	char * custom1;
	uint8_t custom2_type_len;
	char * custom2;
	uint8_t custom3_type_len;
	char * custom3;
	uint8_t custom4_type_len;
	char * custom4;
	uint8_t chksum;
} fruid_area_chassis_t;

/* To hold the Board area information. */
typedef struct fruid_area_board_t {
	uint8_t format_ver : 4;
	uint8_t area_len;
	uint8_t lang_code;
	uint8_t * mfg_time;
	char * mfg_time_str;
	uint8_t mfg_type_len;
	char * mfg;
	uint8_t name_type_len;
	char * name;
	uint8_t serial_type_len;
	char * serial;
	uint8_t part_type_len;
	char * part;
	uint8_t fruid_type_len;
	char * fruid;
	uint8_t custom1_type_len;
	char * custom1;
	uint8_t custom2_type_len;
	char * custom2;
	uint8_t custom3_type_len;
	char * custom3;
	uint8_t custom4_type_len;
	char * custom4;
	uint8_t chksum;
} fruid_area_board_t;

/* To hold the Product area information. */
typedef struct fruid_area_product_t {
	uint8_t format_ver : 4;
	uint8_t area_len;
	uint8_t lang_code;
	uint8_t mfg_type_len;
	char * mfg;
	uint8_t name_type_len;
	char * name;
	uint8_t part_type_len;
	char * part;
	uint8_t version_type_len;
	char * version;
	uint8_t serial_type_len;
	char * serial;
	uint8_t asset_tag_type_len;
	char * asset_tag;
	uint8_t fruid_type_len;
	char * fruid;
	uint8_t custom1_type_len;
	char * custom1;
	uint8_t custom2_type_len;
	char * custom2;
	uint8_t custom3_type_len;
	char * custom3;
	uint8_t custom4_type_len;
	char * custom4;
	uint8_t chksum;
} fruid_area_product_t;

/* To hold all the fruid information */
typedef struct fruid_info_t {
	struct {
		uint8_t flag;
		uint8_t format_ver : 4;
		uint8_t area_len;
		uint8_t type;
		char * type_str;
		uint8_t part_type_len;
		char * part;
		uint8_t serial_type_len;
		char * serial;
		uint8_t custom1_type_len;
		char * custom1;
		uint8_t custom2_type_len;
		char * custom2;
		uint8_t custom3_type_len;
		char * custom3;
		uint8_t custom4_type_len;
		char * custom4;
		uint8_t chksum;
	} chassis;
	struct {
		uint8_t flag;
		uint8_t format_ver : 4;
		uint8_t area_len;
		uint8_t lang_code;
		uint8_t * mfg_time;
		char * mfg_time_str;
		uint8_t mfg_type_len;
		char * mfg;
		uint8_t name_type_len;
		char * name;
		uint8_t serial_type_len;
		char * serial;
		uint8_t part_type_len;
		char * part;
		uint8_t fruid_type_len;
		char * fruid;
		uint8_t custom1_type_len;
		char * custom1;
		uint8_t custom2_type_len;
		char * custom2;
		uint8_t custom3_type_len;
		char * custom3;
		uint8_t custom4_type_len;
		char * custom4;
		uint8_t chksum;
	} board;
	struct {
		uint8_t flag;
		uint8_t format_ver : 4;
		uint8_t area_len;
		uint8_t lang_code;
		uint8_t mfg_type_len;
		char * mfg;
		uint8_t name_type_len;
		char * name;
		uint8_t part_type_len;
		char * part;
		uint8_t version_type_len;
		char * version;
		uint8_t serial_type_len;
		char * serial;
		uint8_t asset_tag_type_len;
		char * asset_tag;
		uint8_t fruid_type_len;
		char * fruid;
		uint8_t custom1_type_len;
		char * custom1;
		uint8_t custom2_type_len;
		char * custom2;
		uint8_t custom3_type_len;
		char * custom3;
		uint8_t custom4_type_len;
		char * custom4;
		uint8_t chksum;
	} product;
} fruid_info_t;

/* To hold the different area offsets. */
typedef struct fruid_eeprom_t {
	uint8_t * header;
	uint8_t * chassis;
	uint8_t * board;
	uint8_t * product;
	uint8_t * multirecord;
} fruid_eeprom_t;

enum {
	CPN,
	CSN,
	CCD1,
	CCD2,
	CCD3,
	CCD4,
	BMD,
	BM,
	BP,
	BSN,
	BPN,
	BFI,
	BCD1,
	BCD2,
	BCD3,
	BCD4,
	PM,
	PN,
	PPN,
	PV,
	PSN,
	PAT,
	PFI,
	PCD1,
	PCD2,
	PCD3,
	PCD4
};

int fruid_parse(const char * bin, fruid_info_t * fruid);
int fruid_parse_eeprom(const uint8_t * eeprom, int eeprom_len, fruid_info_t * fruid);
void free_fruid_info(fruid_info_t * fruid);
int fruid_modify(const char * cur_bin, const char * new_bin, const char * field, const char * content);

#ifdef __cplusplus
}
#endif

#endif /* __FRUID_H__ */
