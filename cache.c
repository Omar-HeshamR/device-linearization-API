#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

int IsDestroyed = 1;

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  if(IsDestroyed == 0){
    return -1;
  }
  if( num_entries < 2 || num_entries > 4096){
    return -1;
  }
  cache = calloc(num_entries, sizeof(cache_entry_t));
  cache_size = num_entries;
  IsDestroyed = 0;
  return 1;
}

int cache_destroy(void) {
  if(IsDestroyed == 1){
    return -1;
  }
  free(cache);
  cache = NULL;
  cache_size = 0;
  IsDestroyed = 1;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  if( cache_enabled() == false || buf == NULL || cache_size == 0){
    return -1;
  }

  int isFound = 0;

  num_queries += 1;
  clock += 1;
  for(int i = 0; i < cache_size; i++){
    if(cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num){
      cache[i].access_time = clock;
      num_hits += 1;
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
      isFound = 1;
    }
  }

  if(isFound == 0){
    return -1;
  }

  return 1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {

  clock += 1;
  
  for(int i = 0; i < cache_size; i++){
    if(cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num){
      cache[i].access_time = clock;
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
    }
  }

}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {

  if( cache_enabled() == false || buf == NULL || disk_num < 0 || disk_num > 15 || block_num < 0 || block_num > 255){
    return -1;
  }
  
  num_queries += 1;
  clock += 1;
  int already_inserted = 0;

  // check if cache already exsits
  for(int i = 0; i < cache_size; i++){
    // printf("ASKED FOR %d %d \n", disk_num, block_num);
    // printf("CAHCE VALUE: %d %d \n", cache[i].disk_num, cache[i].block_num);
      if(cache[i].valid == true && cache[i].disk_num == disk_num && cache[i].block_num == block_num ){
        return -1;
      }
  }

  // check if cache has space, if it does add new cache
  for(int i = 0; i < cache_size; i++){
    if(cache[i].valid == false ){
      cache[i].access_time = clock;
      cache[i].disk_num = disk_num;
      cache[i].block_num = block_num;
      cache[i].valid = true;
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
      already_inserted = 1;
      break;
    }
  }

  // replace with LRU if no space exists
  if(already_inserted == 0){

    int index_of_least_recently_used_cache = 0;
    for(int i = 0; i < cache_size; i++){
        if (cache[i].access_time < cache[index_of_least_recently_used_cache].access_time){
          index_of_least_recently_used_cache = i;
        }
    }
  
    cache[index_of_least_recently_used_cache].access_time = clock;
    cache[index_of_least_recently_used_cache].disk_num = disk_num;
    cache[index_of_least_recently_used_cache].block_num = block_num;
    cache[index_of_least_recently_used_cache].valid = true;
    memcpy(cache[index_of_least_recently_used_cache].block, buf, JBOD_BLOCK_SIZE);

  }

  return 1;
}

bool cache_enabled(void) {
  if(IsDestroyed == 0){
    return true;
  }
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}

void print_cache() {
  printf("Cache contents:\n");
  for (int i = 0; i < cache_size; i++) {
    if (cache[i].valid) {
      printf("Entry %d: disk_num=%d block_num=%d access_time=%d\n",
             i, cache[i].disk_num, cache[i].block_num, cache[i].access_time);
      printf("\n");
    }
  }
}
