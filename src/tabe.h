/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: tabe.h,v 1.6 2001/12/22 01:16:38 thhsieh Exp $
 *
 */
#ifndef __TABE_H__
#define __TABE_H__

#ifdef __cplusplus 
extern "C" { 
#endif 

typedef unsigned char     *ZuYinSymbol;
typedef unsigned char     *ZuYinSymbolSequence;
typedef unsigned int       ZuYinIndex;

typedef unsigned int       ZhiCode;
typedef unsigned char     *Zhi;
typedef unsigned char     *ZhiStr;

typedef unsigned short int Yin;

struct TsiInfo {
  ZhiStr             tsi;
  unsigned long int  refcount;
  unsigned long int  yinnum;
  Yin               *yindata;
};

struct TsiYinInfo {
  Yin               *yin;
  unsigned long int  yinlen;
  unsigned long int  tsinum;
  ZhiStr             tsidata;
};

struct ChunkInfo {
  ZhiStr          chunk;
  int             num_tsi;
  struct TsiInfo *tsi;
};

struct ChuInfo {
  ZhiStr            chu;
  int               num_chunk;
  struct ChunkInfo *chunk;
};

struct TsiDB {
  int type;
  int flags;
  char *db_name;
  void *dbp;
  void *dbcp;
  void (*Close)(struct TsiDB *tsidb);
  int  (*RecordNumber)(struct TsiDB *tsidb);
  int  (*Put)(struct TsiDB *tsidb, struct TsiInfo *tsi);
  int  (*Get)(struct TsiDB *tsidb, struct TsiInfo *tsi);
  int  (*CursorSet)(struct TsiDB *tsidb, struct TsiInfo *tsi, int set_range);
  int  (*CursorNext)(struct TsiDB *tsidb, struct TsiInfo *tsi);
  int  (*CursorPrev)(struct TsiDB *tsidb, struct TsiInfo *tsi);
};

struct TsiYinDB {
  int type;
  int flags;
  char *db_name;
  void *dbp;
  void *dbcp;
  void (*Close)(struct TsiYinDB *tsidb);
  int  (*RecordNumber)(struct TsiYinDB *tsidb);
  int  (*Put)(struct TsiYinDB *tsidb, struct TsiYinInfo *tsiyin);
  int  (*Get)(struct TsiYinDB *tsidb, struct TsiYinInfo *tsiyin);
  int  (*CursorSet)(struct TsiYinDB *tsidb, struct TsiYinInfo *tsiyin,
		    int set_range);
  int  (*CursorNext)(struct TsiYinDB *tsidb, struct TsiYinInfo *tsiyin);
  int  (*CursorPrev)(struct TsiYinDB *tsidb, struct TsiYinInfo *tsiyin);
};

enum {
  DB_TYPE_DB,
  DB_TYPE_LAST
};

#define DB_FLAG_OVERWRITE	0x01
#define DB_FLAG_CREATEDB	0x02
#define DB_FLAG_READONLY	0x04
#define DB_FLAG_NOSYNC		0x08
#define DB_FLAG_SHARED		0x10
#define DB_FLAG_NOUNPACK_YIN	0x20

struct TsiDB       *tabeTsiDBOpen(int type, const char *db_name, int flags);

int                 tabeTsiInfoLookupPossibleTsiYin(struct TsiDB *tsidb,
                                                    struct TsiInfo *tsi);

struct TsiYinDB    *tabeTsiYinDBOpen(int type, const char *db_name,
				     int flags);

struct ChunkInfo   *tabeChunkInfoNew(char *str);

void                tabeChunkInfoDestroy(struct ChunkInfo *chunk);

int                 tabeChuInfoToChunkInfo(struct ChuInfo *chu);

int                 tabeChunkSegmentationSimplex(struct TsiDB *tsidb,
						 struct ChunkInfo *chunk);
int                 tabeChunkSegmentationComplex(struct TsiDB *tsidb,
						 struct ChunkInfo *chunk);
int                 tabeChunkSegmentationBackward(struct TsiDB *tsidb,
						  struct ChunkInfo *chunk);
struct TsiInfo *    tabeTsiInfoNew(char *str);

void                tabeTsiInfoDestroy(struct TsiInfo *tsi);

int                 tabeTsiInfoLookupZhiYin(struct TsiDB *tsidb,
                                            struct TsiInfo *z);
ZhiStr              tabeYinLookupZhiList(Yin yin);

ZuYinSymbolSequence tabeYinToZuYinSymbolSequence(Yin yin);
Yin                 tabeZuYinSymbolSequenceToYin(ZuYinSymbolSequence str);
const Zhi           tabeZuYinIndexToZuYinSymbol(ZuYinIndex idx);
ZuYinIndex          tabeZuYinSymbolToZuYinIndex(ZuYinSymbol sym);
ZuYinIndex          tabeZozyKeyToZuYinIndex(int key);

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

int                 tabeZhiIsBig5Code(Zhi zhi);
ZhiCode             tabeZhiToZhiCode(Zhi zhi);
Zhi                 tabeZhiCodeToZhi(ZhiCode code);
int                 tabeZhiCodeToPackedBig5Code(ZhiCode code);
unsigned long int   tabeZhiCodeLookupRefCount(ZhiCode code);

#ifdef __cplusplus 
} /* extern "C" */
#endif 

#endif /* __TABE_H__ */
