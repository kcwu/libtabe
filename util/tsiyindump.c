/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: tsiyindump.c,v 1.7 2003/05/13 10:01:07 kcwu Exp $
 *
 */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HPUX
#  define _INCLUDE_POSIX_SOURCE
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../src/version.h"

#include <db.h>
#include <tabe.h>

void
usage(void)
{
  printf("tsiyindump: libtabe-%s\n", RELEASE_VER);
  printf("Usage: tsiyindump -d <TsiDB> -y <TsiYinDB>\n");
  printf("   -d <TsiDB>     \t path to TsiDB\n");
  printf("   -y <TsiYinDB>  \t path to TsiYinDB\n");
  exit(0);
}

void
dump(struct TsiDB *db, struct TsiYinDB *ydb)
{
  struct TsiInfo *tsi;
  struct TsiYinInfo *tsiyin;
  int rval, i, j, len;

  rval = db->RecordNumber(db);
  if (rval < 0) {
    fprintf(stderr, "tsiyindump: wrong DB format.\n");
    usage();
  }

  tsi = (struct TsiInfo *)malloc(sizeof(struct TsiInfo));
  memset(tsi, 0, sizeof(struct TsiInfo));
  tsi->tsi = (ZhiStr)malloc(sizeof(unsigned char)*80);
  memset(tsi->tsi, 0, 80);

  tsiyin = (struct TsiYinInfo *)malloc(sizeof(struct TsiYinInfo));
  memset(tsiyin, 0, sizeof(struct TsiYinInfo));

  i = 0;
  while (1) {
    if (i == 0) {
      db->CursorSet(db, tsi, 0);
    }
    else {
      rval = db->CursorNext(db, tsi);
      if (rval < 0) {
        break;
      }
    }
    i++;
    if (!tsi->yinnum) {
      tabeTsiInfoLookupPossibleTsiYin(db, tsi);
    }
    len = strlen((char *)tsi->tsi)/2;
    for (j = 0; j < tsi->yinnum; j++) {
      tsiyin->yinlen = len;
      tsiyin->yin = (Yin *)malloc(sizeof(Yin)*len);
      memcpy(tsiyin->yin, tsi->yindata+j*len, sizeof(Yin)*len);
      rval = ydb->Get(ydb, tsiyin);
      if (rval < 0) { /* no such tsiyin */
        tsiyin->tsinum = 1;
        tsiyin->tsidata = (ZhiStr)malloc(sizeof(unsigned char)*len*2);
        memcpy(tsiyin->tsidata, tsi->tsi, sizeof(unsigned char)*len*2);
        ydb->Put(ydb, tsiyin);
      }
      else {
        tsiyin->tsidata =
          (ZhiStr)realloc(tsiyin->tsidata,
                          sizeof(unsigned char)*((tsiyin->tsinum+1)*len*2));
        memcpy(tsiyin->tsidata+(tsiyin->tsinum*len*2), tsi->tsi,
               sizeof(unsigned char)*len*2);
        tsiyin->tsinum++;
        ydb->Put(ydb, tsiyin);
      }
    }
  }
}

int
main(int argc, char **argv)
{
  int ch;
  struct TsiDB *db;
  struct TsiYinDB *ydb;
extern char *optarg;
extern int optind, opterr, optopt;

  char *db_name, *op_name;

  db_name = op_name = (char *)NULL;

  while ((ch = getopt(argc, argv, "d:y:")) != -1) {
    switch(ch) {
      case 'd':
        db_name = (char *)strdup(optarg);
        break;
      case 'y':
        op_name = (char *)strdup(optarg);
        break;
      default:
        usage();
        break;
    }
  }
  argc -= optind;
  argv += optind;

  if (!db_name) {
    usage();
  }

  db = tabeTsiDBOpen(DB_TYPE_DB, db_name, DB_FLAG_READONLY);
  if (!db) {
    usage();
  }

  if (!op_name) {
    usage();
  }

  ydb = tabeTsiYinDBOpen(DB_TYPE_DB, op_name,
                         DB_FLAG_CREATEDB | DB_FLAG_OVERWRITE | DB_FLAG_NOSYNC);
  if (!ydb) {
    usage();
  }

  dump(db, ydb);

  db->Close(db);
  ydb->Close(ydb);

  return(0);
}
