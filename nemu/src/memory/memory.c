#include "common.h"
#include "memory/cache.h"
#include "burst.h"

#include <stddef.h>

/* Memory accessing interfaces */

uint32_t hwaddr_read(hwaddr_t addr, size_t len) {
	int primary_index = read_cache_L1(addr);
	uint32_t block_bias = addr & (CACHE_B - 1);
	uint8_t buf[4] = {0};
	size_t first_part = CACHE_B - block_bias;
	if (first_part > len) {
		first_part = len;
	}
	memcpy(buf, cache_L1[primary_index].data + block_bias, first_part);
	if (first_part < len) {
		int secondary_index = read_cache_L1(addr + first_part);
		memcpy(buf + first_part, cache_L1[secondary_index].data, len - first_part);
	}
	uint32_t result = 0;
	memcpy(&result, buf, len);
	result &= (~0u >> ((4 - len) << 3));
	return result;
}

void hwaddr_write(hwaddr_t addr, size_t len, uint32_t data) {
	write_cache_L1(addr, len, data);
}

uint32_t lnaddr_read(lnaddr_t addr, size_t len) {
	return hwaddr_read(addr, len);
}

void lnaddr_write(lnaddr_t addr, size_t len, uint32_t data) {
	hwaddr_write(addr, len, data);
}

uint32_t swaddr_read(swaddr_t addr, size_t len) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	return lnaddr_read(addr, len);
}

void swaddr_write(swaddr_t addr, size_t len, uint32_t data) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	lnaddr_write(addr, len, data);
}

