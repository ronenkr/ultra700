#include "fs_fat32.h"
#include "sd_minimal.h"
#include <string.h>

// Minimal static buffer for one sector
static uint8_t g_sec[512];

static uint16_t rd16(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
static uint32_t rd32(const uint8_t *p){ return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }

boolean FAT32_Mount(FAT32_Volume *vol)
{
    if (!vol) return false;
    memset(vol,0,sizeof(*vol));
    // Read LBA0 (could be MBR or VBR). Assume either FAT32 boot sector or MBR with first partition FAT32.
    if (!SDM_ReadBlock(0, g_sec)) return false;
    uint16_t sig = rd16(&g_sec[510]);
    if (sig != 0xAA55) {
        return false; // not a valid boot / mbr sector
    }
    uint8_t part_type = g_sec[0x1BE + 4];
    uint32_t part_lba = rd32(&g_sec[0x1BE + 8]);
    uint32_t part_size = rd32(&g_sec[0x1BE + 12]);
    uint32_t bpb_lba = 0;
    if (part_type == 0x0B || part_type == 0x0C) {
        bpb_lba = part_lba;
    } else {
        // Treat sector 0 as boot sector directly
        bpb_lba = 0;
    }
    if (!SDM_ReadBlock(bpb_lba, g_sec)) return false;
    if (rd16(&g_sec[510]) != 0xAA55) return false;
    uint16_t bytes_per_sector = rd16(&g_sec[11]);
    uint8_t spc = g_sec[13];
    uint16_t rsvd = rd16(&g_sec[14]);
    uint8_t fats = g_sec[16];
    uint32_t total_sectors = rd16(&g_sec[19]);
    if (total_sectors == 0) total_sectors = rd32(&g_sec[32]);
    uint32_t sectors_per_fat = rd16(&g_sec[22]);
    if (sectors_per_fat == 0) sectors_per_fat = rd32(&g_sec[36]);
    uint32_t root_cluster = rd32(&g_sec[44]);
    if (bytes_per_sector != 512 || spc == 0) return false; // only 512 supported now
    uint32_t fat_begin = bpb_lba + rsvd;
    uint32_t cluster_begin = fat_begin + fats * sectors_per_fat;
    uint32_t data_sectors = total_sectors - (rsvd + fats * sectors_per_fat);
    uint32_t total_clusters = data_sectors / spc;
    vol->sectors_per_fat = sectors_per_fat;
    vol->fat_begin_lba = fat_begin;
    vol->cluster_begin_lba = cluster_begin;
    vol->root_dir_first_cluster = root_cluster;
    vol->total_sectors = total_sectors;
    vol->data_sectors = data_sectors;
    vol->total_clusters = total_clusters;
    vol->bytes_per_sector = bytes_per_sector;
    vol->sectors_per_cluster = spc;
    return true;
}

static uint32_t lba_of_cluster(const FAT32_Volume *vol, uint32_t clust)
{
    return vol->cluster_begin_lba + (clust - 2) * vol->sectors_per_cluster;
}

static void format_name83(const uint8_t *dirent, char *out)
{
    char name[12]; memcpy(name, dirent, 11); name[11]='\0';
    for(int i=0;i<11;i++) if (name[i]==' ') name[i]='\0';
    int n=0; for(int i=0;i<8 && name[i];++i) out[n++]=name[i];
    if (name[8]) { out[n++]='.'; for(int i=8;i<11 && name[i];++i) out[n++]=name[i]; }
    out[n]='\0';
}

boolean FAT32_ListDirectory(FAT32_Volume *vol, uint32_t startCluster, FAT32_ListCallback cb, void *user)
{
    if (!vol || !cb) return false;
    uint32_t cl = startCluster;
    while (cl >= 2 && cl < 0x0FFFFFF8) {
        for (uint8_t s=0; s<vol->sectors_per_cluster; ++s) {
            if (!SDM_ReadBlock(lba_of_cluster(vol, cl) + s, g_sec)) return false;
            for (int off=0; off<512; off+=32) {
                uint8_t first = g_sec[off];
                if (first == 0x00) return true; // end of directory
                if (first == 0xE5) continue; // deleted
                uint8_t attr = g_sec[off + 11];
                if (attr == 0x0F) continue; // LFN entry skip
                // Build name
                char nbuf[14]; format_name83(&g_sec[off], nbuf);
                uint16_t cl_lo = rd16(&g_sec[off + 26]);
                uint16_t cl_hi = rd16(&g_sec[off + 20]);
                uint32_t firstCl = ((uint32_t)cl_hi << 16) | cl_lo;
                uint32_t fsz = rd32(&g_sec[off + 28]);
                cb(nbuf, attr, firstCl, fsz, user);
            }
        }
        // follow FAT
        uint32_t fat_sector = vol->fat_begin_lba + (cl * 4) / 512;
        if (!SDM_ReadBlock(fat_sector, g_sec)) return false;
        uint32_t entry = rd32(&g_sec[(cl * 4) % 512]) & 0x0FFFFFFF;
        cl = entry;
    }
    return true;
}

boolean FAT32_ListRoot(FAT32_Volume *vol, FAT32_ListCallback cb, void *user)
{
    return FAT32_ListDirectory(vol, vol->root_dir_first_cluster, cb, user);
}

// Very simple 8.3 upper-case matcher
static boolean match_name83(const uint8_t *dirent, const char *name83)
{
    char temp[12];
    memcpy(temp, dirent, 11); temp[11] = '\0';
    for (int i=0;i<11;i++) if (temp[i]==' ') temp[i]='\0';
    // Build NAME[.EXT]
    char fmt[14];
    int n=0; for(int i=0;i<8 && temp[i];++i) fmt[n++]=temp[i];
    if (temp[8]) { fmt[n++]='.'; for(int i=8;i<11 && temp[i];++i) fmt[n++]=temp[i]; }
    fmt[n]='\0';
    return strcmp(fmt, name83)==0;
}

boolean FAT32_Open(FAT32_Volume *vol, const char *name83, FAT32_File *file)
{
    if (!vol || !file) return false;
    memset(file,0,sizeof(*file));
    uint32_t cl = vol->root_dir_first_cluster;
    // Iterate clusters in root (no subdirs yet)
    while (cl >= 2 && cl < 0x0FFFFFF8) {
        for (uint8_t s=0; s<vol->sectors_per_cluster; ++s) {
            if (!SDM_ReadBlock(lba_of_cluster(vol, cl) + s, g_sec)) return false;
            for (int off=0; off<512; off+=32) {
                uint8_t first = g_sec[off];
                if (first == 0x00) return false; // end
                if (first == 0xE5) continue; // deleted
                if (g_sec[off + 11] & 0x08) continue; // volume label
                if (g_sec[off + 11] & 0x10) continue; // skip subdirs for now
                if (match_name83(&g_sec[off], name83)) {
                    uint16_t cl_lo = rd16(&g_sec[off + 26]);
                    uint16_t cl_hi = rd16(&g_sec[off + 20]);
                    file->first_cluster = ((uint32_t)cl_hi << 16) | cl_lo;
                    file->current_cluster = file->first_cluster;
                    file->size_bytes = rd32(&g_sec[off + 28]);
                    file->file_pos = 0;
                    return true;
                }
            }
        }
        // Follow FAT chain
        uint32_t fat_sector = vol->fat_begin_lba + (cl * 4) / 512;
        if (!SDM_ReadBlock(fat_sector, g_sec)) return false;
        uint32_t entry = rd32(&g_sec[(cl * 4) % 512]) & 0x0FFFFFFF;
        cl = entry;
    }
    return false;
}

static boolean advance_cluster(FAT32_Volume *vol, FAT32_File *file)
{
    uint32_t cl = file->current_cluster;
    uint32_t fat_sector = vol->fat_begin_lba + (cl * 4) / 512;
    if (!SDM_ReadBlock(fat_sector, g_sec)) return false;
    uint32_t entry = rd32(&g_sec[(cl * 4) % 512]) & 0x0FFFFFFF;
    if (entry >= 0x0FFFFFF8) return false; // EOF
    file->current_cluster = entry;
    return true;
}

size_t FAT32_Read(FAT32_Volume *vol, FAT32_File *file, void *buf, size_t bytes)
{
    if (!vol || !file || !buf) return 0;
    if (file->file_pos >= file->size_bytes) return 0;
    if (bytes > file->size_bytes - file->file_pos) bytes = file->size_bytes - file->file_pos;
    uint8_t *out = (uint8_t*)buf;
    while (bytes) {
        uint32_t cluster_offset = file->file_pos % (vol->sectors_per_cluster * 512u);
        uint32_t sector_in_cluster = cluster_offset / 512u;
        uint32_t lba = lba_of_cluster(vol, file->current_cluster) + sector_in_cluster;
        if (!SDM_ReadBlock(lba, g_sec)) break;
        uint32_t within_sector = file->file_pos % 512u;
        uint32_t copy = 512u - within_sector;
        if (copy > bytes) copy = (uint32_t)bytes;
        memcpy(out, &g_sec[within_sector], copy);
        out += copy;
        file->file_pos += copy;
        bytes -= copy;
        // Advance sector / cluster as needed
        within_sector += copy;
        if (within_sector == 512u) {
            sector_in_cluster++;
            if (sector_in_cluster == vol->sectors_per_cluster) {
                if (!advance_cluster(vol, file)) break;
            }
        }
    }
    return (size_t)(out - (uint8_t*)buf);
}

void FAT32_Seek(FAT32_File *file, uint32_t pos)
{
    // Simple: only support rewind to 0 right now; full seek would need chain traversal
    if (pos == 0) {
        file->file_pos = 0;
        file->current_cluster = file->first_cluster;
    }
}
