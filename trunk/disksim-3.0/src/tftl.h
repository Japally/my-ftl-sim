/* 
 * Contributors: mingbang wang (mingbang24@gmail.com)
 *             
 *
 * In case if you have any doubts or questions, kindly write to:mingbang24@gmail.com 
 * 
 * Description: This is a header file for tftl.c.  
 * 
 */

#include "type.h"

#define MAP_INVALID 1 
#define MAP_REAL 2
#define MAP_GHOST 3

#define CACHE_INVALID 0
#define CACHE_VALID 1

int flash_hit;
int disk_hit;
int read_cache_hit;
int write_cache_hit;
int evict;
int update_reqd;
int delay_flash_update;
int save_count;
struct ftl_operation * tftl_setup();


struct tftl_page_entry {
  _u32 free  : 1;
  _u32 ppn   : 31;
  int  cache_status;
  int  cache_age;
  int  map_status;
  int  map_age;
  int  update;
};

struct page_map_dir{
  _u32 free;
  _u32 ppn;
};

struct zone_map_dir{
  _u32 free;
  _u32 ppn;
};

#define MAP_ENTRIES_PER_PAGE 512
#define PAGE_MAPDIR_NUM_PER_PAGE 512
#define PAGE_MAPDIR_NUM_PER_ZONE 512
#define BLOCK_NUM_PER_ZONE 4096

int TOTAL_MAP_ENTRIES; 

sect_t tftl_pagemap_num;
struct tftl_page_entry *tftl_pagemap;
