#ifndef FS_FAT32_H
#define FS_FAT32_H
#include <stdint.h>
#include <stddef.h>
#include "systypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t sectors_per_fat;
    uint32_t fat_begin_lba;
    uint32_t cluster_begin_lba;
    uint32_t root_dir_first_cluster;
    uint32_t total_sectors;
    uint32_t data_sectors;
    uint32_t total_clusters;
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
} FAT32_Volume;

typedef struct {
    uint32_t first_cluster;
    uint32_t size_bytes;
    uint32_t current_cluster;
    uint32_t file_pos;
} FAT32_File;

typedef void (*FAT32_ListCallback)(const char *name83, uint8_t attr, uint32_t firstCluster, uint32_t sizeBytes, void *user);

boolean FAT32_ListRoot(FAT32_Volume *vol, FAT32_ListCallback cb, void *user); // list root entries (files + dirs)
boolean FAT32_ListDirectory(FAT32_Volume *vol, uint32_t startCluster, FAT32_ListCallback cb, void *user); // generic cluster chain dir

boolean FAT32_Mount(FAT32_Volume *vol); // Mount first partition or raw volume
boolean FAT32_Open(FAT32_Volume *vol, const char *name83, FAT32_File *file); // NAME.EXT (upper)
size_t  FAT32_Read(FAT32_Volume *vol, FAT32_File *file, void *buf, size_t bytes);
void    FAT32_Seek(FAT32_File *file, uint32_t pos); // simple absolute seek within file

#ifdef __cplusplus
}
#endif
#endif
