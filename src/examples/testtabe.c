/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: testtabe.c,v 1.3 2004/09/18 17:50:00 kcwu Exp $
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "tabe.h"

#if 0
/*
 * cross test zhi2yin and yin2zhi tables
 */
void
test_table()
{
  int rval, i;
  struct ZhiInfo h;
  ZhiStr charlist;
  unsigned long int charsum, yinsum, yinsubsum[4];
  double refchar, refyin;

  printf("==> Cross-testing two tables ...\n");
  charsum = yinsum = 0;
  yinsubsum[0] = yinsubsum[1] = yinsubsum[2] = yinsubsum[3] = 0;
  refchar = refyin = 0;

  /* loop all possible Big5 character code range */
  for (h.code = 0xa440; h.code <= 0xf9dc; h.code++) {
    /* blind test */
    rval = tabeZhiInfoLookupYin(&h);
    if (rval < 0) { /* it's not a Big5 character */
      continue;
    }
    h.chct = tabeZhiCodeToZhi(h.code);
    h.refcount = tabeZhiCodeLookupRefCount(h.code);
    if (h.refcount <= 0) {
      printf("Error: Char %s has weird weird reference count %ld.\n",
	     h.chct, h.refcount);
    }
    refchar += h.refcount;
    charsum++;

    for (i = 0; i < 4; i++) {
      if (h.yin[i]) {
	charlist = tabeYinLookupZhiList(h.yin[i]);
	yinsum++;
	if (!charlist) {
	  printf("Error: Char %s has strange Yin %d.\n",
		 h.chct, h.yin[i]);
	}
	if (!strstr(charlist, h.chct)) {
	  printf("Error: Char %s not found in Yin %d.\n",
		 h.chct, h.yin[i]);
	}
	free(charlist);
      }
    }

    if (h.yin[3]) {
      yinsubsum[3]++;
      refyin += 4*h.refcount;
    }
    if (!h.yin[3] && h.yin[2]) {
      yinsubsum[2]++;
      refyin += 3*h.refcount;
    }
    if (!h.yin[2] && h.yin[1]) {
      yinsubsum[1]++;
      refyin += 2*h.refcount;
    }
    if (!h.yin[1] && h.yin[0]) {
      yinsubsum[0]++;
      refyin += 1*h.refcount;
    }
  }
  printf("Total %ld char and %ld yin found.\n", charsum, yinsum);
  printf("Char of one   Yin = %ld\n", yinsubsum[0]);
  printf("Char of two   Yin = %ld\n", yinsubsum[1]);
  printf("Char of three Yin = %ld\n", yinsubsum[2]);
  printf("Char of four  Yin = %ld\n", yinsubsum[3]);
  printf("Average Yin of Zhi = %.2f\n", refyin/refchar);
}
#endif

void
test_zuyin()
{
  ZhiStr str = "ㄒㄧㄠ";
  Yin yin;

  printf("==> Testing for ZuyinSymbolSequence to Big5Yin...\n");
  yin = tabeZuYinSymbolSequenceToYin(str);
  if (yin) {
    printf("Done, Yin = %d (%s)[%s].\n",
	   yin, str, tabeYinLookupZhiList(yin));
  }
  else {
    printf("Error: Sequence %s not a valid Yin.\n", str);
  }
}

void
test_input()
{
  int i, j, len, idx;
  Yin yin;
  ZhiStr str;
  unsigned char tmp[3];
  ZhiStr sample = "軟體台灣地區化計畫";
  int key_sequence[9][4] = {
    { 'b', 'j', '0', '3' },  /* 軟 */
    { 'w', 'u', '3',   0 },  /* 體 */
    { 'w', '9', '6',   0 },  /* 臺 */
    { 'j', '0',   0,   0 },  /* 灣 */
    { '2', 'u', '4',   0 },  /* 地 */
    { 'f', 'm',   0,   0 },  /* 區 */
    { 'c', 'j', '8', '4' },  /* 化 */
    { 'r', 'u', '4',   0 },  /* 計 */
    { 'c', 'j', '8', '4' }   /* 畫 */
  };

  printf("==> Testing input method ...\n");
  len = strlen(sample)/2;
  tmp[2] = (unsigned char)NULL;

  for (i = 0; i < len; i++) {
    yin = 0;
    for (j = 0; j < 4; j++) {
      idx = tabeZozyKeyToZuYinIndex(key_sequence[i][j]);
      if (idx > 0 && idx < 22) {
        yin |= idx << 9;
      }
      if (idx > 21 && idx < 25) {
        yin |= (idx - 21) << 7;
      }
      if (idx > 24 && idx < 38) {
        yin |= (idx - 24) << 3;
      }
      if (idx > 38 && idx < 43) {
        yin |= (idx - 37);
      }                                                    
    }
    str = tabeYinLookupZhiList(yin);
    strncpy(tmp, sample+2*i, 2);
    if (!strstr(str, tmp)) {
      printf("Error: input key sequence wrong.\n");
    }
    else {
      printf("%s [%s]\n", tmp, str);
    }
  }
}

void
test_tsidb()
{
  struct TsiDB *db;
  struct TsiInfo *tsi;
  char *db_name = "tsi.db";
  char *sample_tsi = "巴拿馬運河";
  int rval;
  int i;

  db = tabeTsiDBOpen(DB_TYPE_DB, db_name, DB_FLAG_READONLY);
  if (!db) {
    printf("Error: can not open db %s.\n", db_name);
    return;
  }

  tsi = (struct TsiInfo *)malloc(sizeof(struct TsiInfo)); 
  memset(tsi, 0, sizeof(struct TsiInfo));
  tsi->tsi = (unsigned char *)malloc(sizeof(unsigned char)*80);

  printf("==> Dumping TsiDB from ...%s\n", sample_tsi);
  strcpy(tsi->tsi, sample_tsi);
  rval = db->Get(db, tsi);
  if (rval < 0) {
    printf("Error: %s not found.\n", tsi->tsi);
  }
  db->CursorSet(db, tsi, 0);
  for (i = 0; i < 100; i++) {
    db->CursorNext(db, tsi);
    printf("%d %s %ld %ld\n", i, tsi->tsi, tsi->yinnum, tsi->refcount);
  }

  db->Close(db);
}

void
test_seg_simplex()
{
  struct ChuInfo *chu;
  int i, j;
  struct TsiDB *tdb;
  char buf[1000];
  char *filename = "libtabe.sgml";

  FILE *fp = fopen(filename, "r");

  printf("==> Segmentation file \"%s\" using Simplex Method...\n", filename);
  if (!fp) {
    perror("test_seg_simplex()");
    return;
  }
  chu = malloc(sizeof(struct ChuInfo));
  memset(chu, 0, sizeof(struct ChuInfo));
  chu->chu = buf;

  tdb = tabeTsiDBOpen(DB_TYPE_DB, "tsi.db", DB_FLAG_READONLY);

  while (1) {
    if (!fgets(buf, 1000, fp)) {
      break;
    }

    tabeChuInfoToChunkInfo(chu);

    for (i = 0; i < chu->num_chunk; i++) {
      printf("%s -> ", (chu->chunk[i]).chunk);
      tabeChunkSegmentationSimplex(tdb, chu->chunk+i);
      for (j = 0; j < chu->chunk[i].num_tsi; j++) {
        printf("[%s] ", chu->chunk[i].tsi[j].tsi);
      }
      printf("\n");
    }
  }
  fclose(fp);
  tdb->Close(tdb);
}

void
test_seg_backward()
{
  struct ChuInfo *chu;
  int i, j;
  struct TsiDB *tdb;
  char buf[1000];
  char *filename = "libtabe.sgml";

  FILE *fp = fopen(filename, "r");

  printf("==> Segmentation file \"%s\" using Backward Method...\n", filename);
  if (!fp) {
    perror("test_seg_backward()");
    return;
  }
  chu = malloc(sizeof(struct ChuInfo));
  memset(chu, 0, sizeof(struct ChuInfo));
  chu->chu = buf;

  tdb = tabeTsiDBOpen(DB_TYPE_DB, "tsi.db", DB_FLAG_READONLY);

  while (1) {
    if (!fgets(buf, 1000, fp)) {
      break;
    }

    tabeChuInfoToChunkInfo(chu);

    for (i = 0; i < chu->num_chunk; i++) {
      printf("%s -> ", (chu->chunk[i]).chunk);
      tabeChunkSegmentationBackward(tdb, chu->chunk+i);
      for (j = 0; j < chu->chunk[i].num_tsi; j++) {
        printf("[%s] ", chu->chunk[i].tsi[j].tsi);
      }
      printf("\n");
    }
  }
  fclose(fp);
  tdb->Close(tdb);
}

void
test_seg_complex()
{
  struct ChuInfo *chu;
  int i, j;
  struct TsiDB *tdb;
  char buf[1000];
  char *filename = "libtabe.sgml";

  FILE *fp = fopen(filename, "r");

  printf("==> Segmentation file \"%s\" using Complex Method...\n", filename);
  if (!fp) {
    perror("test_seg_complex()");
    return;
  }
  chu = malloc(sizeof(struct ChuInfo));
  memset(chu, 0, sizeof(struct ChuInfo));
  chu->chu = buf;

  tdb = tabeTsiDBOpen(DB_TYPE_DB, "tsi.db", DB_FLAG_READONLY);

  while (1) {
    if (!fgets(buf, 1000, fp)) {
      break;
    }

    tabeChuInfoToChunkInfo(chu);

    for (i = 0; i < chu->num_chunk; i++) {
      printf("%s -> ", (chu->chunk[i]).chunk);
      tabeChunkSegmentationComplex(tdb, chu->chunk+i);
      for (j = 0; j < chu->chunk[i].num_tsi; j++) {
        printf("[%s] ", chu->chunk[i].tsi[j].tsi);
      }
      printf("\n");
    }
  }
  fclose(fp);
  tdb->Close(tdb);
}

int
main(void)
{
  printf("\n");
//  test_table();
  printf("\n");
  test_zuyin();
  printf("\n");
  test_input();
  printf("\n");
  test_tsidb();
  printf("\n");
  test_seg_simplex();
  printf("\n");
  test_seg_complex();
  printf("\n");
  test_seg_backward();
  printf("\n");

  return(0);
}
