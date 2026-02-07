#include <ksyscall.h>

int64_t sys_file_read(const char* path, void* user_buf, uint64_t max_len) {
    FIL file;
    FRESULT res;
    UINT bytes_read;

    res = f_open(&file, path, FA_READ);
    if (res != FR_OK) return -1;

    uint64_t fsize = f_size(&file);
    if (fsize > max_len) fsize = max_len;

    // TODO: Verify user buffer
    res = f_read(&file, user_buf, fsize, &bytes_read);
    f_close(&file);

    if (res != FR_OK) return -1;
    return bytes_read;
}

int sys_stat(const char* user_path, struct kstat* user_stat_out) {
    FILINFO fno;

    FRESULT res = f_stat(user_path, &fno);
    if (res != FR_OK) {
        return -1;
    }

    struct kstat kst;
    kst.size = fno.fsize;
    
    // NOTE: FATFS AM_DIR (0x10) indicates directory
    kst.flags = (fno.fattrib & AM_DIR) ? 1 : 0; 

    memcpy(user_stat_out, &kst, sizeof(struct kstat));
    
    return 0;
}

int sys_read_dir_entry(const char* path, uint64_t index, struct kdirent* user_out) {
    DIR dir;
    FILINFO fno;
    FRESULT res;

    res = f_opendir(&dir, path);
    if (res != FR_OK) {
        return -1;
    }

    // Skip to the requested index
    // Loop 'index + 1' times. 
    // Example: Index 0 -> read once. Index 1 -> read twice (skip 0, get 1).
    for (uint64_t i = 0; i <= index; i++) {
        res = f_readdir(&dir, &fno);
        
        if (res != FR_OK || fno.fname[0] == 0) {
            // Error or End of Directory
            f_closedir(&dir);
            return -2; // Return distinctive code for "End of Dir"
        }
    }

    // Found the entry! Copy to user struct
    struct kdirent k_ent;
    
    memset(&k_ent, 0, sizeof(struct kdirent));
    strncpy(k_ent.name, fno.fname, sizeof(k_ent.name) - 1);
    
    k_ent.size = fno.fsize;
    k_ent.is_dir = (fno.fattrib & AM_DIR) ? 1 : 0;

    // TODO: verify user_out
    memcpy(user_out, &k_ent, sizeof(struct kdirent));

    f_closedir(&dir);
    return 0;
}