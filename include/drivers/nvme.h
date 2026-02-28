#ifndef NVME_H
#define NVME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    bool present;
    uint32_t namespace_id;
    uint64_t total_blocks;
    uint32_t block_size;
} nvme_namespace_t;

void nvme_init(void);
bool nvme_is_ready(void);
const nvme_namespace_t* nvme_get_namespace(void);

bool nvme_read_blocks(uint64_t lba, uint32_t block_count, void* buffer);
bool nvme_write_blocks(uint64_t lba, uint32_t block_count, const void* buffer);

#endif //NVME_H
