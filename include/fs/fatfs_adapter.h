#ifndef FATFS_ADAPTER_H
#define FATFS_ADAPTER_H

#include <stdbool.h>

bool fatfs_mount_nvme(const char* mount_point);

#endif //FATFS_ADAPTER_H
