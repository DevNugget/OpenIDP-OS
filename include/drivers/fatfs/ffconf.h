/*---------------------------------------------------------------------------/
/  Configurations of FatFs Module
/---------------------------------------------------------------------------*/

#define FFCONF_DEF	5380	/* Revision ID */

#ifndef FFCONF_H
#define FFCONF_H

/* Basic configuration */
#define FF_FS_READONLY    0
#define FF_FS_MINIMIZE    0
#define FF_USE_STRFUNC    0
#define FF_USE_FIND       0
#define FF_USE_MKFS       0
#define FF_USE_LABEL      0
#define FF_USE_FORWARD    0

/* Locale and namespace */
#define FF_CODE_PAGE      437
#define FF_USE_LFN        0
#define FF_MAX_LFN        255
#define FF_LFN_UNICODE    0
#define FF_LFN_BUF        255
#define FF_SFN_BUF        12

/* Drive/Volume Config */
#define FF_VOLUMES        1
#define FF_MULTI_PARTITION 0
#define FF_MIN_SS         512
#define FF_MAX_SS         512
#define FF_USE_TRIM       0
#define FF_FS_NOFSINFO    0

/* Filesystem Features */
#define FF_FS_TINY        0
#define FF_FS_EXFAT       0
#define FF_FS_NORTC       1
#define FF_FS_RPATH       0

/* Memory and sync */
#define FF_FS_LOCK        0
#define FF_FS_REENTRANT   0

#define FF_NORTC_MON   1    /* January */
#define FF_NORTC_MDAY  1    /* 1st */
#define FF_NORTC_YEAR  2025 /* Just a valid year */

#define FF_LBA64 0
#define ATA_CMD_WRITE_PIO  0x30

#endif /* FFCONF_H */