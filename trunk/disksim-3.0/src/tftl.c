/* 
 * Contributors: mingbang wang (mingbang24@gmail.com)
 *               
 * 
 * In case if you have any doubts or questions, kindly write to: mingbang wang (mingbang24@gmail.com) 
 *
 * 
 */

#include <stdlib.h>
#include <string.h>

#include "flash.h"
#include "dftl.h"
#include "ssd_interface.h"
#include "disksim_global.h"
#include "tftl.h"

struct page_map_dir *page_mapdir;
struct zone_map_dir *zone_mapdir;

int curr_zone_id;
int zone_num = total_util_blk_num / BLOCK_NUM_PER_ZONE ; //total_util_blk_num define at ssd_interface.c

_u32 curr_zonemap_blk_no;
_u16 curr_zonemap_page_no;

_u32 curr_pagemap_blk_no[zone_num];
_u16 curr_pagemap_page_no[zone_num];

_u32 curr_data_blk_no[zone_num];
_u16 curr_data_page_no[zone_num];

extern int merge_switch_num;
extern int merge_partial_num;
extern int merge_full_num;
extern int page_num_for_2nd_map_table;
int stat_gc_called_num;
double total_gc_overhead_time;

int map_pg_read=0;

_u32 tftl_gc_cost_benefit( int zone_id )
{
  int max_cb = 0;
  int blk_cb;
  
  _u32 max_blk = -1, i;

  for (i = 0; i < nand_blk_num; i++) {
    if ( nand_blk[i].zone_id == zone_id ){
    	  if (i == curr_data_blk_no[zone_id] || i == curr_pagemap_blk_no[zone_id] || i == curr_zonemap_blk_no ){continue;}
    	  blk_cb = nand_blk[i].ipc;
      if (blk_cb > max_cb) {
        max_cb = blk_cb;
        max_blk = i;
      }
    	}
  }

  ASSERT(max_blk != -1);
  ASSERT(nand_blk[max_blk].ipc > 0);
  return max_blk;
}

/****************************************************************
                             the map_table operation
*****************************************************************/
void switch_zone(int zone_id)
{
  ASSERT(zone_id != curr_zone_id);

 //flush the page_mapdir back to the flash
  zone_maptable_write (curr_zone_id *SECT_NUM_PER_PAGE, SECT_NUM_PER_PAGE, 3);
  
  //read the pagemap table from flash
  zone_maptable_read (zone_id*SECT_NUM_PER_PAGE, SECT_NUM_PER_PAGE, 3)
  
  //update the curr_zone_id 
  curr_zone_id = zone_id;
  
}

void zone_maptable_read (sect_t lsn, sect_t size, int mapdir_flag)
{
  int i;
  int lpn = lsn/SECT_NUM_PER_PAGE; // logical page number
  int size_page = size/SECT_NUM_PER_PAGE; // size in page 

  sect_t s_lsn;
  sect_t s_psn;
  sect_t lsns[SECT_NUM_PER_PAGE];
  
  memset (lsns, 0xFF, sizeof (lsns));
  
  s_lsn = lpn * SECT_NUM_PER_PAGE;
  s_psn = zone_mapdir[lpn].ppn * SECT_NUM_PER_PAGE;
  
  for (i = 0; i < SECT_NUM_PER_PAGE; i++) {
    lsns[i] = s_lsn + i;
  }
  
  nand_page_read(s_psn, lsns, 0);
  
}

void zone_maptable_write (sect_t lsn, sect_t size, int mapdir_flag)
{
  int i;
  int lpn = lsn/SECT_NUM_PER_PAGE; // logical page number
  int size_page = size/SECT_NUM_PER_PAGE; // size in page 
  int ppn;

  sect_t s_lsn;
  sect_t s_psn;
  sect_t s_psn1;
  sect_t lsns[SECT_NUM_PER_PAGE];
  
  memset (lsns, 0xFF, sizeof (lsns));
  
  s_lsn = lpn * SECT_NUM_PER_PAGE;
  
  	if (curr_zonemap_page_no >= SECT_NUM_PER_BLK) {
      if ((curr_zonemap_blk_no = nand_get_free_blk(0)) == -1) {
        while (free_blk_num < 3 ) { tftl_gc_run(curr_zone_id);}
        tftl_gc_get_free_blk(curr_zone_id, mapdir_flag);
      }
      else {
      curr_zonemap_page_no = 0;
      }
    }

  if (zone_mapdir[lpn].free == 0) {
    s_psn1 = zone_mapdir[lpn].ppn * SECT_NUM_PER_PAGE;
    for(i = 0; i<SECT_NUM_PER_PAGE; i++){
      nand_invalidate(s_psn1 + i, s_lsn + i);
    }
    nand_stat(OOB_WRITE);
  }
  else {
    zone_mapdir[lpn].free = 0;
  }

  for (i = 0; i < SECT_NUM_PER_PAGE; i++) {
    lsns[i] = s_lsn + i;
  }
  
  s_psn = SECTOR(curr_zonemap_blk_no , curr_zonemap_page_no);
  ppn = s_psn / SECT_NUM_PER_PAGE;
  
  /******************************************
            update the zone_maptable 
  ******************************************/
  zone_mapdir[lpn].ppn = ppn;
  curr_zonemap_page_no += SECT_NUM_PER_PAGE;
  
  nand_page_write(s_psn, lsns, 0, mapdir_flag);
  
}

void page_maptable_read (sect_t lsn, sect_t size, int mapdir_flag)
{
  int i;
  int lpn = lsn/SECT_NUM_PER_PAGE; // logical page number
  int size_page = size/SECT_NUM_PER_PAGE; // size in page

  sect_t s_lsn;
  sect_t s_psn;
  sect_t lsns[SECT_NUM_PER_PAGE];
  
  memset (lsns, 0xFF, sizeof (lsns));
  
  s_lsn = lpn * SECT_NUM_PER_PAGE;
  s_psn = page_mapdir[lpn].ppn * SECT_NUM_PER_PAGE;
  
  for (i = 0; i < SECT_NUM_PER_PAGE; i++) {
    lsns[i] = s_lsn + i;
  }
  
  nand_page_read(s_psn, lsns, 0);

}


void page_maptable_write (sect_t lsn, sect_t size, int mapdir_flag)
{
  int i;
  int lpn = lsn/SECT_NUM_PER_PAGE; // logical page number
  int size_page = size/SECT_NUM_PER_PAGE; // size in page 
  int ppn;
  int zone_id = lpn / PAGE_MAPDIR_NUM_PER_ZONE;
  
  sect_t s_lsn;
  sect_t s_psn;
  sect_t s_psn1;
  sect_t lsns[SECT_NUM_PER_PAGE];

  memset (lsns, 0xFF, sizeof (lsns));

  s_lsn = lpn * SECT_NUM_PER_PAGE;

  if( zone_id != curr_zone_id ) {
    switch_zone(zone_id);
  }

  	if (curr_pagemap_page_no[zone_id] >= SECT_NUM_PER_BLK) {
      if ((curr_pagemap_blk_no[zone_id]  = nand_get_free_blk(0)) == -1) {
        while (free_blk_num < 3 ){ tftl_gc_run(zone_id);}
        tftl_gc_get_free_blk(zone_id, mapdir_flag);
      }
      else {
      curr_pagemap_page_no[zone_id]  = 0;
      }
    }
 
  if (page_mapdir[lpn].free == 0) {
    s_psn1 = page_mapdir[lpn].ppn * SECT_NUM_PER_PAGE;
    for(i = 0; i<SECT_NUM_PER_PAGE; i++){
      nand_invalidate(s_psn1 + i, s_lsn + i);
    }
    nand_stat(OOB_WRITE);
  }
  else {
    page_mapdir[lpn].free = 0;
  }

  for (i = 0; i < SECT_NUM_PER_PAGE; i++) {
    lsns[i] = s_lsn + i;
  }

  s_psn = SECTOR(curr_pagemap_blk_no[zone_id] , curr_pagemap_page_no[zone_id]);
  ppn = s_psn / SECT_NUM_PER_PAGE;
  
  /******************************************
           update the zone_maptable 
  ******************************************/
  page_mapdir[lpn].ppn = ppn;
  curr_pagemap_page_no[zone_id] += SECT_NUM_PER_PAGE;
  nand_page_write(s_psn, lsns, 0, mapdir_flag);

}

/****************************************************************
                             the flash operation
*****************************************************************/
size_t tftl_read(sect_t lsn, sect_t size, int mapdir_flag)
{
  int i;
  int lpn = lsn/SECT_NUM_PER_PAGE; // logical page number
  int size_page = size/SECT_NUM_PER_PAGE; // size in page 
  int sect_num;
  int zone_id = lpn / (PAGE_MAPDIR_NUM_PER_PAGE*MAP_ENTRIES_PER_PAGE);
  
  sect_t curr_2ndmappage;
  sect_t s_lsn;	// starting logical sector number
  sect_t s_psn; // starting physical sector number

  sect_t lsns[SECT_NUM_PER_PAGE];
  memset (lsns, 0xFF, sizeof (lsns));
  
  ASSERT(lpn < tftl_pagemap_num);
  ASSERT(lpn + size_page <= tftl_pagemap_num);

  sect_num = (size < SECT_NUM_PER_PAGE) ? size : SECT_NUM_PER_PAGE;

//1.switch the working zome to target zone
  if( zone_id != curr_zone_id ) {
    switch_zone(zone_id);
  }
  
//2.read the 2nd_maptable
  curr_2ndmappage = lpn / MAP_ENTRIES_PER_PAGE;
  page_maptable_read (curr_2ndmappage * SECT_NUM_PER_PAGE, SECT_NUM_PER_PAGE, 2);

//3.read  flash according the page_maptable
  s_psn = tftl_pagemap[lpn].ppn * SECT_NUM_PER_PAGE;
  s_lsn = lpn * SECT_NUM_PER_PAGE;
  for (i = 0; i < SECT_NUM_PER_PAGE; i++) {
    lsns[i] = s_lsn + i;
  }

  size = nand_page_read(s_psn, lsns, 0);

  ASSERT(size == SECT_NUM_PER_PAGE);

  return sect_num;
}

size_t tftl_write(sect_t lsn, sect_t size, int mapdir_flag)
{
  int i;
  int lpn = lsn/SECT_NUM_PER_PAGE; // logical page number
  int size_page = size/SECT_NUM_PER_PAGE; // size in page 
  int zone_id = lpn / (PAGE_NUM_PER_BLK*BLOCK_NUM_PER_ZONE);
  int ppn;
  int sect_num = SECT_NUM_PER_PAGE;

  sect_t curr_2ndmappage;
  sect_t lsns[SECT_NUM_PER_PAGE];
  sect_t s_lsn;	// starting logical sector number
  sect_t s_psn; // starting physical sector number 
  sect_t s_psn1;
  memset (lsns, 0xFF, sizeof (lsns));
  
  ASSERT(lpn < tftl_pagemap_num);
  ASSERT(lpn + size_page <= tftl_pagemap_num);

  s_lsn = lpn * SECT_NUM_PER_PAGE;

//1.switch the working zone to target zone
  if( zone_id != curr_zone_id ) {
    switch_zone(zone_id);
  }

//2.read the page_maptable
  curr_2ndmappage = lpn / MAP_ENTRIES_PER_PAGE;
  page_maptable_read (curr_2ndmappage * SECT_NUM_PER_PAGE, SECT_NUM_PER_PAGE, 2);

//3.check the page  already be write, if yes and invalidate the page
  if (tftl_pagemap[lpn].free == 0) {
    s_psn1 = tftl_pagemap[lpn].ppn * SECT_NUM_PER_PAGE;
    for(i = 0; i<SECT_NUM_PER_PAGE; i++){
      nand_invalidate(s_psn1 + i, s_lsn + i);
    }
    nand_stat(OOB_WRITE);
  }
  else {
    tftl_pagemap[lpn].free = 0;
  }

//4.find a free page to write the new data
  	if (curr_data_page_no[zone_id] >= SECT_NUM_PER_BLK) {
      if ((curr_data_blk_no[zone_id] = nand_get_free_blk(0)) == -1) {
        while (free_blk_num < 3 ){  tftl_gc_run( zone_id );}
        tftl_gc_get_free_blk(zone_id, mapdir_flag);
      }
      else {
      curr_data_page_no[zone_id] = 0;
      }
    }

//5.write new data to new page 
  s_psn = SECTOR(curr_zonemap_blk_no , curr_zonemap_page_no);
  for (i = 0; i < SECT_NUM_PER_PAGE; i++) {
    lsns[i] = s_lsn + i;
  }
  curr_pagemap_page_no[zone_id] += SECT_NUM_PER_PAGE;
  nand_page_write(s_psn, lsns, 0, mapdir_flag);

//6.update the map table 
  ppn = s_psn / SECT_NUM_PER_PAGE;
  tftl_pagemap[lpn].ppn = ppn;

//7.update the 2nd map table
  page_maptable_write (curr_2ndmappage * SECT_NUM_PER_PAGE, SECT_NUM_PER_PAGE, 2);

  return sect_num;
}


void tftl_gc_get_free_blk(int zone_id, int mapdir_flag)
{
  if ( mapdir_flag == 3 ){
    if (curr_zonemap_page_no >= SECT_NUM_PER_BLK) {
      curr_zonemap_blk_no = nand_get_free_blk(1);
      curr_zonemap_page_no = 0;
    }
  }else if ( mapdir_flag == 2 ){
    if (curr_pagemap_page_no[zone_id] >= SECT_NUM_PER_BLK) {
      curr_pagemap_blk_no[zone_id] = nand_get_free_blk(1);
      curr_pagemap_page_no[zone_id] = 0;
    }
  }else if ( mapdir_flag == 1 ){
    if (curr_data_page_no[zone_id] >= SECT_NUM_PER_BLK) {
      curr_data_blk_no[zone_id] = nand_get_free_blk(1);
      curr_data_page_no[zone_id] = 0;
    }
  }
}

void tftl_gc_run( int zone_id )
{
  blk_t victim_blk_no;
  int merge_count;
  int i,z, j,m,q, benefit = 0;
  int k,old_flag,temp_arr[PAGE_NUM_PER_BLK],temp_arr1[PAGE_NUM_PER_BLK],map_arr[PAGE_NUM_PER_BLK]; 
  int valid_flag,pos;

  _u32 copy_lsn[SECT_NUM_PER_PAGE], copy[SECT_NUM_PER_PAGE];
  _u16 valid_sect_num,  l;
  
  memset(copy_lsn, 0xFF, sizeof (copy_lsn));
  pos = 0;
  merge_count = 0;

  victim_blk_no = tftl_gc_cost_benefit( zone_id );

/*********************************************************
                                        for debug
***********************************************************/
  for( q = 0; q < PAGE_NUM_PER_BLK; q++){
  	if(nand_blk[victim_blk_no].page_status[q] == 2){ //map block
      for( q = 0; q  < 64; q++) {
        if(nand_blk[victim_blk_no].page_status[q] ��= 2 ){
          printf("something corrupted1=%d",victim_blk_no);
        }
      }
    }
    else if(nand_blk[victim_blk_no].page_status[q] == 1){ //map block
      for( q = 0; q  < 64; q++) {
        if(nand_blk[victim_blk_no].page_status[q] ��= 1 ){
          printf("something corrupted1=%d",victim_blk_no);
        }
      }
    }
    else if(nand_blk[victim_blk_no].page_status[q] == 0){ //data block
      for( q = 0; q  < 64; q++) {
        if(nand_blk[victim_blk_no].page_status[q] ��= 0 ){
          printf("something corrupted2",victim_blk_no);
        }
      }
    }
  }
/**************************************************************************************/

  for (i = 0; i < PAGE_NUM_PER_BLK; i++) 
  {
    valid_flag = nand_oob_read( SECTOR(victim_blk_no, i * SECT_NUM_PER_PAGE));

    if(valid_flag == 1)
    {
        valid_sect_num = nand_page_read( SECTOR(victim_blk_no, i * SECT_NUM_PER_PAGE), copy, 1);

        merge_count++;

        ASSERT(valid_sect_num == 4);
        
        k=0;
        for (j = 0; j < valid_sect_num; j++) {
          copy_lsn[k] = copy[j];
          k++;
        }

          if(nand_blk[victim_blk_no].page_status[i] == 2)
          {
            tftl_gc_get_free_blk( zone_id, 3 );
            zone_mapdir[(copy_lsn[0]/SECT_NUM_PER_PAGE)].ppn  = BLK_PAGE_NO_SECT(SECTOR(curr_zonemap_blk_no, curr_zonemap_page_no));
            nand_page_write(SECTOR(curr_zonemap_blk_no, curr_zonemap_page_no) & (~OFF_MASK_SECT), copy_lsn, 1, 3);
            curr_zonemap_page_no += SECT_NUM_PER_PAGE;
          }
          else if(nand_blk[victim_blk_no].page_status[i] == 1)
          {
            tftl_gc_get_free_blk( zone_id, 2 );
            page_mapdir[(copy_lsn[0]/SECT_NUM_PER_PAGE)].ppn  = BLK_PAGE_NO_SECT(SECTOR(curr_pagemap_blk_no[zone_id], curr_pagemap_page_no[zone_id]));
            nand_page_write(SECTOR(curr_pagemap_blk_no[zone_id], curr_pagemap_page_no[zone_id]) & (~OFF_MASK_SECT), copy_lsn, 1, 2);
            curr_pagemap_page_no[zone_id] += SECT_NUM_PER_PAGE;
          }
          else{
            tftl_gc_get_free_blk( zone_id, 1 );
            tftl_pagemap[(copy_lsn[0]/SECT_NUM_PER_PAGE)].ppn = BLK_PAGE_NO_SECT(SECTOR(curr_data_blk_no[zone_id], curr_data_page_no[zone_id]));
            nand_page_write(SECTOR(curr_data_blk_no[zone_id], curr_data_page_no[zone_id]) & (~OFF_MASK_SECT), copy_lsn, 1, 1);
            curr_data_page_no[zone_id] += SECT_NUM_PER_PAGE;
            //find page which have been change 
            map_arr[pos] = copy_lsn[0];
            pos++;
          }
    }
  }
  
  for(i=0;i < PAGE_NUM_PER_BLK;i++) {
      temp_arr[i]=-1;
  }
  
  /***********************************************************************************************
            find those tranlastion_page which should be change cause by data page 
  ***********************************************************************************************/
  k=0;
  for(i =0 ; i < pos; i++) {
      old_flag = 0;
      for( j = 0 ; j < k; j++) {
           if(temp_arr[j] == page_mapdir[((map_arr[i]/SECT_NUM_PER_PAGE)/MAP_ENTRIES_PER_PAGE)].ppn) {
                if(temp_arr[j] == -1){
                      printf("something wrong");
                      ASSERT(0);
                }
                old_flag = 1;
                break;
           }
      }
      if( old_flag == 0 ) {
           temp_arr[k] = page_mapdir[((map_arr[i]/SECT_NUM_PER_PAGE)/MAP_ENTRIES_PER_PAGE)].ppn;
           temp_arr1[k] = map_arr[i];
           k++;
      }
      else
        save_count++;
  }

  for ( i=0; i < k; i++) {
      //read the 2nd map table and invalidate it 
      nand_page_read(temp_arr[i]*SECT_NUM_PER_PAGE,copy,1);

      for(m = 0; m<SECT_NUM_PER_PAGE; m++){
         nand_invalidate(page_mapdir[((temp_arr1[i]/SECT_NUM_PER_PAGE)/MAP_ENTRIES_PER_PAGE)].ppn*SECT_NUM_PER_PAGE+m, copy[m]);
      } 
      nand_stat(OOB_WRITE);
      
      //find a free block if the current one do not have enough free page
      if  (curr_pagemap_page_no[zone_id] >= SECT_NUM_PER_BLK) {
         curr_pagemap_blk_no[zone_id] = nand_get_free_blk(1)
         curr_pagemap_page_no[zone_id] = 0;
      }

      //write back to the 2nd map table 
      page_mapdir[((temp_arr1[i]/SECT_NUM_PER_PAGE)/MAP_ENTRIES_PER_PAGE)].ppn  = BLK_PAGE_NO_SECT(SECTOR(curr_pagemap_blk_no[zone_id], curr_pagemap_page_no[zone_id]));
      nand_page_write(SECTOR(curr_pagemap_blk_no[zone_id], curr_pagemap_page_no[zone_id]) & (~OFF_MASK_SECT), copy, 1, 2);
      curr_pagemap_page_no[zone_id] += SECT_NUM_PER_PAGE;
  }
  
  if(merge_count == 0 ) 
    merge_switch_num++;
  else if(merge_count > 0 && merge_count < PAGE_NUM_PER_BLK)
    merge_partial_num++;
  else if(merge_count == PAGE_NUM_PER_BLK)
    merge_full_num++;
  else if(merge_count > PAGE_NUM_PER_BLK){
    printf("merge_count =%d PAGE_NUM_PER_BLK=%d",merge_count,PAGE_NUM_PER_BLK);
    ASSERT(0);
  }

  nand_erase(victim_blk_no);

}

void tftl_end()
{
  if (tftl_pagemap != NULL) {
    free(tftl_pagemap);
    free(page_mapdir);
    free(zone_mapdir);
  }
  tftl_pagemap_num = 0;
}

void tftl_pagemap_reset()
{
  cache_hit = 0;
  flash_hit = 0;  
  disk_hit = 0;
  evict = 0;
  delay_flash_update = 0; 
  read_count =0;
  write_count=0;
  map_pg_read=0;
  save_count = 0;
}

int tftl_init(blk_t blk_num, blk_t extra_num)
{
  int i;
  int page_mapdir_num;
  int zone_mapdir_num;

  tftl_pagemap_num = blk_num * PAGE_NUM_PER_BLK;
  page_mapdir_num = tftl_pagemap_num / MAP_ENTRIES_PER_PAGE;
  zone_mapdir_num = page_mapdir_num / PAGE_MAPDIR_NUM_PER_PAGE;
  
  if((tftl_pagemap_num % MAP_ENTRIES_PER_PAGE) != 0){
    printf("tftl_pagemap_num % MAP_ENTRIES_PER_PAGE is not zero\n"); 
    page_mapdir_num++;
  }

  if((page_mapdir_num % PAGE_MAPDIR_NUM_PER_PAGE) != 0){
    printf("page_mapdir_num % PAGE_MAPDIR_NUM_PER_PAGE is not zero\n"); 
    zone_mapdir_num++;
  }
  
  //create primary mapping table
  tftl_pagemap = (struct tftl_page_entry *) malloc(sizeof (struct tftl_page_entry) * tftl_pagemap_num);
  page_mapdir = (struct page_map_dir *)malloc(sizeof(struct page_map_dir) * page_mapdir_num); 
  zone_mapdir = (struct zone_map_dir *)malloc(sizeof(struct zone_map_dir) * zone_mapdir_num); 
  
  
  if ((tftl_pagemap == NULL) || (page_mapdir == NULL) || (zone_mapdir == NULL) ) {
    return -1;
  }

  memset(tftl_pagemap, 0xFF, sizeof (struct tftl_page_entry) * tftl_pagemap_num);
  memset(page_mapdir,  0xFF, sizeof (struct page_map_dir) * page_mapdir_num);
  memset(zone_mapdir,  0xFF, sizeof (struct zone_map_dir) * zone_mapdir_num);

  //youkim: initialize 1st map table 
  TOTAL_MAP_ENTRIES = tftl_pagemap_num;

  for(i = 0; i<TOTAL_MAP_ENTRIES; i++){
    tftl_pagemap[i].cache_status = 0;
    tftl_pagemap[i].cache_age = 0;
    tftl_pagemap[i].map_status = 0;
    tftl_pagemap[i].map_age = 0;
    tftl_pagemap[i].update = 0;
  }

  for(i = 0; i<zone_num; i++){
    curr_data_blk_no[i] = nand_get_free_blk(0);
    curr_data_page_no[i] = 0;
    curr_pagemap_blk_no[i]  = nand_get_free_blk(0);
    curr_pagemap_page_no[i]  = 0;
  }

  curr_zonemap_blk_no = nand_get_free_blk(0);
  curr_zonemap_page_no = 0;

  curr_zone_id = 0;

  //initialize variables
  cache_hit = 0;
  flash_hit = 0;
  disk_hit = 0;
  evict = 0;
  read_cache_hit = 0;
  write_cache_hit = 0;
  write_count =0;
  read_count = 0;
  save_count = 0;

  //initialize zone mapping table
  for(i = 0; i<zone_mapdir_num; i++){
    zone_maptable_write(i*SECT_NUM_PER_PAGE, SECT_NUM_PER_PAGE, 3);
  }
  
  //initialize page mapping table
  for(i = 0; i<page_mapdir_num; i++){
    page_maptable_write(i*SECT_NUM_PER_PAGE, SECT_NUM_PER_PAGE, 2);
  }
  
  return 0;
}

struct ftl_operation tftl_operation = {
  init:  tftl_init,
  read:  tftl_read,
  write: tftl_write,
  end:   tftl_end
};
  
struct ftl_operation * tftl_setup()
{
  return &tftl_operation;
}