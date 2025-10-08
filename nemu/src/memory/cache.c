#include "memory/cache.h"
#include "burst.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>

CacheLineL1 cache_L1[CACHE_L1_S * CACHE_L1_E];
CacheLineL2 cache_L2[CACHE_L2_S * CACHE_L2_E];

static void seed_random_once(void) {
  static int seeded = 0;
  if (!seeded) {
    srand(time(NULL));
    seeded = 1;
  }
}

void init_cache() {
  seed_random_once();
  int i;
  for (i = 0; i < CACHE_L1_S * CACHE_L1_E; i++) {
    cache_L1[i].validVal = false;
  }
  for (i = 0; i < CACHE_L2_S * CACHE_L2_E; i++) {
    cache_L2[i].dirtyVal = false;
    cache_L2[i].validVal = false;
  }
  return;
}

void ddr3_read_me(hwaddr_t addr, void* data);
 
int read_cache_L1(hwaddr_t addr) {
  seed_random_once();
  uint32_t set = ((addr >> CACHE_b) & (CACHE_L1_S - 1)); 
  uint32_t tag = (addr >> (CACHE_b + CACHE_L1_s)); 

  int block_i;
  int set_begin = set * CACHE_L1_E;
  int set_end = (set + 1) * CACHE_L1_E;
  for (block_i = set_begin; block_i < set_end; block_i++)
    if (cache_L1[block_i].validVal && cache_L1[block_i].tag == tag) // 命中
      return block_i;
  
  int block_i_L2 = read_cache_L2(addr);
  //在L1中找一个空的缓存块
  for (block_i = set_begin; block_i < set_end; block_i++)
    if (!cache_L1[block_i].validVal)
      break;
  //如果没有空的缓存块，则随机替换一个
  if (block_i == set_end)
    block_i = set_begin + rand() % CACHE_L1_E;
  //将L2中的数据读到L1中
  memcpy(cache_L1[block_i].data, cache_L2[block_i_L2].data, CACHE_B);
 
  cache_L1[block_i].validVal = true;
  cache_L1[block_i].tag = tag;
  return block_i;
}




void ddr3_write_me(hwaddr_t addr, void* data, uint8_t* mask);
 
// 查找二级高速缓存中与地址匹配的缓存块，返回缓存块的位置参数
int read_cache_L2(hwaddr_t addr) {
  seed_random_once();
  uint32_t set = ((addr >> CACHE_b) & (CACHE_L2_S - 1));
  uint32_t tag = (addr >> (CACHE_b + CACHE_L2_s));
  uint32_t block_start = ((addr >> CACHE_b) << CACHE_b);
  int block_i;
  int set_begin = set * CACHE_L2_E;
  int set_end = (set + 1) * CACHE_L2_E;
  for (block_i = set_begin; block_i < set_end; block_i++)
    if (cache_L2[block_i].validVal && cache_L2[block_i].tag == tag) 
      return block_i; // 命中
  for (block_i = set_begin; block_i < set_end; block_i++)
    if (!cache_L2[block_i].validVal)
      break;

  if (block_i == set_end)
    block_i = set_begin + rand() % CACHE_L2_E;

  int i;
  if (cache_L2[block_i].validVal && cache_L2[block_i].dirtyVal) {
    uint8_t tmp[BURST_LEN << 1];
    memset(tmp, 1, sizeof(tmp));
    uint32_t block_start_x = (cache_L2[block_i].tag << (CACHE_L2_s + CACHE_b)) | (set << CACHE_b);
    for (i = 0; i < CACHE_B / BURST_LEN; i++) {
      ddr3_write_me(block_start_x + BURST_LEN * i, cache_L2[block_i].data + BURST_LEN * i, tmp);
    }
  }

  for (i = 0; i < CACHE_B / BURST_LEN; i++) {
    ddr3_read_me(block_start + BURST_LEN * i, cache_L2[block_i].data + BURST_LEN * i);
  }
  cache_L2[block_i].validVal = true;
  cache_L2[block_i].dirtyVal = false;
  cache_L2[block_i].tag = tag;
  return block_i;
}

void dram_write(hwaddr_t addr, size_t len, uint32_t data);
 
void write_cache_L1(hwaddr_t addr, size_t len, uint32_t data) {
  uint32_t set = ((addr >> CACHE_b) & (CACHE_L1_S - 1));
  uint32_t tag = (addr >> (CACHE_b + CACHE_L1_s));
  uint32_t block_bias = addr & (CACHE_B - 1);
  int block_i;
  int set_begin = set * CACHE_L1_E;
  int set_end = (set + 1) * CACHE_L1_E;
  for (block_i = set_begin; block_i < set_end; block_i++) {
    if (cache_L1[block_i].validVal && cache_L1[block_i].tag == tag) {
      // 命中
      // 直写策略
      if (block_bias + len > CACHE_B) {
        dram_write(addr, CACHE_B - block_bias, data);
        memcpy(cache_L1[block_i].data + block_bias, &data, CACHE_B - block_bias);
        write_cache_L2(addr, CACHE_B - block_bias, data);
        write_cache_L1(addr + CACHE_B - block_bias, len - (CACHE_B - block_bias), data >> (CACHE_B - block_bias));
      } else {
        dram_write(addr, len, data);
        memcpy(cache_L1[block_i].data + block_bias, &data, len);
        write_cache_L2(addr, len, data);
      }
      return;
    }
  }
  write_cache_L2(addr, len, data);
  return;
}
 
void write_cache_L2(hwaddr_t addr, size_t len, uint32_t data) {
  uint32_t set = ((addr >> CACHE_b) & (CACHE_L2_S - 1));
  uint32_t tag = (addr >> (CACHE_b + CACHE_L2_s));
  uint32_t block_bias = addr & (CACHE_B - 1);
  int block_i;
  int set_begin = set * CACHE_L2_E;
  int set_end = (set + 1) * CACHE_L2_E;
  for (block_i = set_begin; block_i < set_end; block_i++) {
    if (cache_L2[block_i].validVal && cache_L2[block_i].tag == tag) {
      cache_L2[block_i].dirtyVal = true;
      if (block_bias + len > CACHE_B) {
        memcpy(cache_L2[block_i].data + block_bias, &data, CACHE_B - block_bias);
        write_cache_L2(addr + CACHE_B - block_bias, len - (CACHE_B - block_bias), data >> (CACHE_B - block_bias));
      } else {
        memcpy(cache_L2[block_i].data + block_bias, &data, len);
      }
      return;
    }
  }
  block_i = read_cache_L2(addr);
  cache_L2[block_i].dirtyVal = true;
  memcpy(cache_L2[block_i].data + block_bias, &data, len);
  return;
}