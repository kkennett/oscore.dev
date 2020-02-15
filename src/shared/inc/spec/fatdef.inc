//
//    Copyright <C> 2017, Kurt Kennett (K2)
//
//    All rights reserved.
// 
#ifndef __SPEC_FATDEF_INC
#define __SPEC_FATDEF_INC

//
// --------------------------------------------------------------------------------- 
//

#define FAT_DIRENTRY_NAME0_AVAIL            0x00
#define FAT_DIRENTRY_NAME0_E5               0x05
#define FAT_DIRENTRY_NAME0_DOT              0x2E
#define FAT_DIRENTRY_NAME0_ERASED           0xE5

#define FAT_DIRENTRY_CASE_LOWEXT            0x10
#define FAT_DIRENTRY_CASE_LOWBASE           0x08
#define FAT_DIRENTRY_CASE_NO83NAME          0x04

#define FAT_DIRENTRY_ATTR_READONLY          0x01
#define FAT_DIRENTRY_ATTR_HIDDEN            0x02
#define FAT_DIRENTRY_ATTR_SYSTEM            0x04
#define FAT_DIRENTRY_ATTR_LABEL             0x08
#define FAT_DIRENTRY_ATTR_DIR               0x10
#define FAT_DIRENTRY_ATTR_ARCHIVE           0x20
#define FAT_DIRENTRY_ATTR_LONGEND           0x40
#define FAT_DIRENTRY_ATTR_LONGDEL           0x80

#define FAT_DIRENTRY_IS_LONGNAME(x)         (((x)&0xFF)==0x0F)

#define FAT_CLUSTER12_IS_UNUSED(x)          (((x)&0xFFF)==0)
#define FAT_CLUSTER12_IS_NEXT_PTR(x)        ((((x)&0xFFF)>0) && (((x)&0xFFF)<0xFF0))
#define FAT_CLUSTER12_IS_RESERVED(x)        ((((x)&0xFFF)>=0xFF0) && (((x)&0xFFF)<=0xFF6))
#define FAT_CLUSTER12_IS_BAD(x)             (((x)&0xFFF)==0xFF7)
#define FAT_CLUSTER12_IS_CHAIN_END(x)       ((((x)&0xFFF)>=0xFF8) && (((x)&0xFFF)<=0xFFF))

#define FAT_CLUSTER16_IS_UNUSED(x)          (((x)&0xFFFF)==0)
#define FAT_CLUSTER16_IS_NEXT_PTR(x)        ((((x)&0xFFFF)>0) && (((x)&0xFFFF)<0xFFF0))
#define FAT_CLUSTER16_IS_RESERVED(x)        ((((x)&0xFFFF)>=0xFFF0) && (((x)&0xFFFF)<=0xFFF6))
#define FAT_CLUSTER16_IS_BAD(x)             (((x)&0xFFFF)==0xFFF7)
#define FAT_CLUSTER16_IS_CHAIN_END(x)       ((((x)&0xFFFF)>=0xFFF8) && (((x)&0xFFFF)<=0xFFFF))

#define FAT_CLUSTER32_IS_UNUSED(x)          (((x)&0x0FFFFFFF)==0)
#define FAT_CLUSTER32_IS_NEXT_PTR(x)        ((((x)&0x0FFFFFFF)>0) && (((x)&0x0FFFFFFF)<0x0FFFFFF0))
#define FAT_CLUSTER32_IS_RESERVED(x)        ((((x)&0x0FFFFFFF)>=0x0FFFFFF0) && (((x)&0x0FFFFFFF)<=0x0FFFFFF6))
#define FAT_CLUSTER32_IS_BAD(x)             (((x)&0x0FFFFFFF)==0x0FFFFFF7)
#define FAT_CLUSTER32_IS_CHAIN_END(x)       ((((x)&0x0FFFFFFF)>=0x0FFFFFF8) && (((x)&0x0FFFFFFF)<=0x0FFFFFFF))

#define FAT_CLUSTER_TO_SECTOR(firstDataSector, sectorsPerCluster, cluster)     ((((cluster) - 2) * (sectorsPerCluster)) + (firstDataSector))
#define FAT_SECTOR_TO_CLUSTER(firstDataSector, sectorsPerCluster, sector)      ((((sector) - (firstDataSector)) / (sectorsPerCluster)) + 2)

//
// --------------------------------------------------------------------------------- 
//

#endif // __SPEC_FATDEF_INC

