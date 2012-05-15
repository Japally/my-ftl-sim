/* 
 * Contributors: mingbang wang (mingbang24@gmail.com)
 *             
 *
 * In case if you have any doubts or questions, kindly write to:mingbang24@gmail.com 
 * 
 * Description: This is a header file for tftl.c.  
 * 
 */
#ifndef TFTL
#define TFTL

#include "type.h"

#define MAP_INVALID 1 
#define MAP_REAL 2
#define MAP_GHOST 3

#define CACHE_INVALID 0
#define CACHE_VALID 1

int swicth_num;

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
  int update;
};

struct zone_map_dir{
  _u32 free;
  _u32 ppn;
  int update;
};

struct page_map_dir *page_mapdir;
struct zone_map_dir *zone_mapdir;

//the zone base parameters
#define MAP_ENTRIES_PER_PAGE 512
#define PAGE_MAPDIR_NUM_PER_PAGE 512

int block_num_per_zone;
int page_mapdir_num_per_zone;
int zone_mapdir_num_per_zone;

int TOTAL_MAP_ENTRIES; 

sect_t tftl_pagemap_num;
struct tftl_page_entry *tftl_pagemap;

void zone_maptable_read (sect_t lsn, sect_t size, int mapdir_flag);
void zone_maptable_write (sect_t lsn, sect_t size, int mapdir_flag);
void page_maptable_read (sect_t lsn, sect_t size, int mapdir_flag);
void page_maptable_write (sect_t lsn, sect_t size, int mapdir_flag);
_u32 tftl_gc_cost_benefit( int zone_id );
void tftl_gc_get_free_blk(int zone_id, int mapdir_flag);
void tftl_gc_run( int zone_id );
int tftl_init(blk_t blk_num, blk_t extra_num);
void switch_zone(int zone_id);

int curr_zone_id;
int curr_2nd_maptable_no;
int curr_2nd_maptable_update_mark;

_u32 curr_zonemap_blk_no;
_u16 curr_zonemap_page_no;

_u32 curr_pagemap_blk_no[100];
_u16 curr_pagemap_page_no[100];

_u32 curr_data_blk_no[100];
_u16 curr_data_page_no[100];

_u32 curr_cold_data_blk_no[100];
_u16 curr_cold_data_page_no[100];

#endif 
