#include <assert.h>
#include <stdint.h>

#include "c_types.h"
#include "mem.h"
#include "osapi.h"
#include "spi_flash.h"

#include "relib/relib.h"
#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"
#include "relib/s/partition_item.h"
#include "relib/s/spi_flash_header.h"

extern int system_phy_init_sector;
extern int system_rf_cal_sector;
extern int system_param_sector_start;

static uint32_t s_ota_2_addr;
static uint32_t s_system_data_addr;
static int s_app_partition_num;
static partition_item_st *s_app_partition_table;
static bool s_app_partition_check_flag;

bool ICACHE_FLASH_ATTR
system_partition_get_item(partition_type_t input_type, partition_item_st *app_partition_item)
{
	if (app_partition_item == NULL ||
	    s_app_partition_num == 0 ||
	    s_app_partition_check_flag == false ||
	    s_app_partition_table == NULL) {
		return false;
	}

	for (int i = 0; i < s_app_partition_num; i++) {
		partition_item_st *entry = &s_app_partition_table[i];
		if (entry->type == input_type) {
			app_partition_item->type = entry->type;
			app_partition_item->addr = entry->addr;
			app_partition_item->size = entry->size;
			return true;
		}
	}
	return false;
}

bool ICACHE_FLASH_ATTR
init_app_partition_addr(flash_size_map_t map)
{
	uint32_t system_data_addr;

	if (map == FLASH_SIZE_2M) {
		os_printf_plus("emap1\r\n");
		return false;
	}
	if (map == FLASH_SIZE_8M_MAP_512_512) {
		system_data_addr = 0xfd000;
	}
	else {
		system_data_addr = 0x1fd000;
		if ((map != FLASH_SIZE_16M_MAP_512_512) && (system_data_addr = 0x3fd000, map != FLASH_SIZE_32M_MAP_512_512)) {
			if (map == FLASH_SIZE_16M_MAP_1024_1024) {
				system_data_addr = 0x1fd000;
			}
			else if (map != FLASH_SIZE_32M_MAP_1024_1024) {
				if (map == FLASH_SIZE_32M_MAP_2048_2048) {
					os_printf_plus("emap7\r\n");
					return false;
				}
				if (map != FLASH_SIZE_64M_MAP_1024_1024) {
					if (map == FLASH_SIZE_128M_MAP_1024_1024) {
						s_ota_2_addr = 0x101000;
						s_system_data_addr = 0xffd000;
						return true;
					}
					os_printf_plus("emapg\r\n");
					return false;
				}
				s_ota_2_addr = 0x101000;
				s_system_data_addr = 0x7fd000;
				return true;
			}
			s_ota_2_addr = 0x101000;
			s_system_data_addr = system_data_addr;
			return true;
		}
	}
	s_system_data_addr = system_data_addr;
	s_ota_2_addr = 0x81000;
	return true;
}

static void ICACHE_FLASH_ATTR
app_partition_print(partition_item_st *entries, int n)
{
	for (int i = 0; i < n; i++) {
		os_printf("p[%d].addr=0x%08x\n", i, entries[i].addr);
	}
}

static inline int ICACHE_FLASH_ATTR
app_partition_quick_sort_partition(partition_item_st *entries, int l, int r)
{
	int pivot = (l + r) / 2;
	uint32_t pivot_addr = entries[pivot].addr;

	while (1) {
		while (entries[l].addr < pivot_addr) l++;
		while (entries[r].addr > pivot_addr) r--;
		if (l >= r) {
			return r;
		}

		partition_item_st tmp = entries[l];
		entries[l] = entries[r];
		entries[r] = tmp;
	}
}

void ICACHE_FLASH_ATTR
app_partition_quick_sort(partition_item_st *entries, int l, int r)
{
	if (l >= r || l < 0) {
		return;
	}

	int p = app_partition_quick_sort_partition(entries, l, r);
	app_partition_quick_sort(entries, l, p);
	app_partition_quick_sort(entries, p + 1, r);
}

bool ICACHE_FLASH_ATTR
system_partition_table_regist(partition_item_st *partitions, uint32_t num, flash_size_map_t map)
{
	uint32_t uVar1;
	partition_item_st *ptr;
	partition_item_st *entry;
	bool map_ok;
	int iVar2;
	partition_item_st *sorted_entry;
	uint32_t size;
	partition_type_t type;
	uint32_t size2;
	uint32_t i;
	int iVar3;
	spi_flash_header_st flash_hdr;
	partition_item_st *sorted_partitions;
	partition_item_st *items;
	uint32_t check_bits;
	bool map_matches;

	items = partitions;
	spi_flash_read(0,(uint32_t*)&flash_hdr,4);
	map_matches = flash_hdr.flash_map == map;
	if (!map_matches) {
		os_printf_plus("mismatch map %d,spi_size_map %d\n",
			map, flash_hdr.flash_map);
	}
	map_ok = init_app_partition_addr(map);
	if (!map_ok) {
		os_printf_plus("map %d err\n", map);
	}
	map_ok = map_ok && map_matches;
	s_app_partition_table = (partition_item_st *)items;
	s_app_partition_num = num;
	if (num == 0) {
		check_bits = 0;
	}
	else {
		i = 0;
		iVar3 = 0;
		check_bits = 0;
		do {
			entry = (partition_item_st *)((int)&s_app_partition_table->type + iVar3);
			if (((entry->size & 0xfff) != 0) || ((entry->addr & 0xfff) != 0)) {
				os_printf_plus("p %d size 0x%x ,0x%x err\r\n",
					entry->type, entry->size, entry->addr);
				map_ok = false;
				entry = (partition_item_st *)((int)&s_app_partition_table->type + iVar3);
			}
			type = entry->type;
			size = entry->size;
			if (type == SYSTEM_PARTITION_BOOTLOADER) {
				if ((entry->addr != 0) || (entry->size != 0x1000)) {
					map_ok = false;
				}
				size = check_bits & 1;
				check_bits = check_bits | 1;
LAB_402402a9:
				if (size != 0) {
					map_ok = false;
				}
			}
			else {
				if (type == SYSTEM_PARTITION_OTA_1) {
					if ((entry->addr != 0x1000) || (0x100000 < entry->size)) {
						os_printf_plus("ota1 partition error\r\n");
						map_ok = false;
					}
					size = check_bits >> 1 & 1;
					check_bits = check_bits | 2;
					goto LAB_402402a9;
				}
				if (type == SYSTEM_PARTITION_OTA_2) {
					if ((s_ota_2_addr != entry->addr) || (0x100000 < entry->size)) {
						os_printf_plus("ota2 partition error\r\n");
						map_ok = false;
					}
					size = check_bits >> 2 & 1;
					check_bits = check_bits | 4;
					goto LAB_402402a9;
				}
				if (type == SYSTEM_PARTITION_RF_CAL) {
					uVar1 = check_bits >> 3;
					if (size != 0x1000) {
						map_ok = false;
					}
					check_bits = check_bits | 8;
					if ((uVar1 & 1) != 0) {
						map_ok = false;
					}
					system_rf_cal_sector = entry->addr / flashchip->sector_size;
				}
				else if (type == SYSTEM_PARTITION_PHY_DATA) {
					if (size != 0x1000) {
						map_ok = false;
					}
					if ((check_bits >> 4 & 1) != 0) {
						map_ok = false;
					}
					check_bits = check_bits | 0x10;
					system_phy_init_sector = entry->addr / flashchip->sector_size;
				}
				else if (type == SYSTEM_PARTITION_SYSTEM_PARAMETER) {
					if (size != 0x3000) {
						map_ok = false;
					}
					if (check_bits >> 5 != 0) {
						map_ok = false;
					}
					check_bits = check_bits | 0x20;
					iVar2 = s_system_data_addr / flashchip->sector_size;
					if (system_param_sector_start != iVar2) {
						os_printf_plus("system param partition error\r\n");
						map_ok = false;
					}
				}
				else if (flashchip->chip_size < size) {
					map_ok = false;
				}
			}
			iVar3 = iVar3 + 0xc;
			i = i + 1;
		} while (i < s_app_partition_num);
	}
	if ((check_bits & 1) == 0) {
		os_printf_plus("boot not set\r\n");
		check_bits = check_bits | 1;
	}
	if ((check_bits >> 1 & 1) == 0) {
		os_printf_plus("ota1 not set\r\n");
		check_bits = check_bits | 2;
	}
	if ((check_bits >> 2 & 1) == 0) {
		os_printf_plus("ota2 not set\r\n");
		check_bits = check_bits | 4;
	}
	if (check_bits != 0x3f) {
		os_printf_plus("nchk:%x\r\n", check_bits);
		map_ok = false;
	}
	sorted_entry = os_zalloc(s_app_partition_num * sizeof(*sorted_entry));
	if (sorted_entry == (partition_item_st *)0x0) {
		map_ok = false;
	}
	else {
		memcpy(sorted_entry,s_app_partition_table,s_app_partition_num * sizeof(*sorted_entry));
		os_printf("before qsort:\n");
		app_partition_print(sorted_entry, s_app_partition_num);
		app_partition_quick_sort(sorted_entry,0,s_app_partition_num - 1);
		os_printf("after qsort:\n");
		app_partition_print(sorted_entry, s_app_partition_num);
		sorted_partitions = sorted_entry;
		if (s_app_partition_num != 1) {
			i = 0;
			do {
				size2 = sorted_entry->size;
				if (size2 == 0) {
					os_printf_plus("partition 0x%x size is %d\r\n",
						sorted_entry->addr, 0);
					size2 = sorted_entry->size;
					map_ok = false;
				}
				if (sorted_entry[1].addr < size2 + sorted_entry->addr) {
					os_printf_plus("p %d and p %d covered: addr:0x%x + len:0x%x > 0x%x\r\n",
						sorted_entry[0].type, sorted_entry[1].type,
						sorted_entry[0].addr, size2, sorted_entry[1].addr);
					map_ok = false;
				}
				sorted_entry = sorted_entry + 1;
				i = i + 1;
			} while (i < s_app_partition_num - 1);
		}
		partition_item_st *app_part = &sorted_partitions[s_app_partition_num - 1];
		if ((flashchip->chip_size + 4) < app_part->addr + app_part->size) {
			os_printf_plus("--- The partition table size is larger than flash size 0x%x ---\r\n",	
				flashchip->chip_size);
			os_printf_plus("please check partition type %d addr:%x len:%x\r\n",
				app_part->type, app_part->addr, app_part->size);
			map_ok = false;
		}
		os_free(ptr);
		s_app_partition_check_flag = true;
	}
	return map_ok;
}

/*
         U ets_memcpy
         U flashchip
         U free
         U os_printf_plus
         U pvPortZalloc
         U spi_flash_read
         U system_param_sector_start
         U system_phy_init_sector
         U system_rf_cal_sector
         U system_upgrade_userbin_check
         U __udivsi3
00000000 t app_partition_quick_sort
00000000 t mem_debug_file_39
00000000 b s_ota_2_addr_29
00000004 b s_system_data_addr_30
00000008 b s_app_partition_num_33
0000000c b s_app_partition_table_34
00000010 t flash_str$2970_2_3
00000010 b s_app_partition_check_flag_43
00000018 t flash_str$2979_2_5
00000020 t flash_str$2983_2_6
00000030 t flash_str$2998_4_9
00000060 t flash_str$2999_4_10
00000070 t flash_str$3000_4_11
00000090 t flash_str$3004_4_13
000000b0 t flash_str$3006_4_14
000000d0 t flash_str$3010_4_15
000000d8 t init_app_partition_addr
000000f0 t flash_str$3015_4_16
00000100 t flash_str$3016_4_17
00000110 t flash_str$3017_4_18
00000120 t flash_str$3018_4_19
00000130 t flash_str$3022_4_23
00000150 t flash_str$3023_4_24
00000190 t flash_str$3027_4_25
000001e0 t flash_str$3028_4_26
00000218 T system_partition_table_regist
00000558 T system_partition_get_ota_partition_size
000005b0 T system_partition_get_item
*/
