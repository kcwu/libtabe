/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: tsidump.c,v 1.1 2000/12/09 09:14:30 thhsieh Exp $
 *
 */
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <db.h>
#include <tabe.h>

void
usage(void)
{
  printf("Usage: tsidump -d <TsiDB> [-f output file -ry]\n");
  printf("   -d <TsiDB>     \t path to TsiDB\n");
  printf("   -f <output file>\t output file in plain text (default: stdout)\n");
  printf("   -r             \t include reference count (default: not)\n");
  printf("   -y             \t include yin data (default: not)\n");
  exit(0);
}

void
dump(struct TsiDB *db, FILE *fp, int ref, int tsiyin)
{
  struct TsiInfo *tsi;
  int rval, i, j, k, len;

  rval = db->RecordNumber(db);
  if (rval < 0) {
    fprintf(stderr, "tsidump: wrong DB format.\n");
    usage();
  }

  tsi = (struct TsiInfo *)malloc(sizeof(struct TsiInfo));
  tsi->tsi = (ZhiStr)malloc(sizeof(unsigned char)*80);
  memset(tsi->tsi, 0, 80);
  tsi->refcount = -1;
  tsi->yinnum = -1;
  tsi->yindata = (Yin *)NULL;

  i = 0;
  while (1) {
    if (i == 0) {
      db->CursorSet(db, tsi);
    }
    else {
      rval = db->CursorNext(db, tsi);
      if (rval < 0) {
        break;
      }
    }
    i++;
    len = strlen((char *)tsi->tsi)/2;
    fprintf(fp, "%s", tsi->tsi);
    if (ref) {
      fprintf(fp, " %ld", tsi->refcount);
    }
    if (tsiyin) {
      ZuYinSymbolSequence zs = NULL;
      int begin = 0;

      fprintf(fp, " ");
      for (j = 0; j < tsi->yinnum; j++) {
        for (k = 0; k < len; k++) {
          zs = tabeYinToZuYinSymbolSequence(tsi->yindata[j*len+k]);
          if (zs) {
            if (begin) {
              fprintf(fp, "¡@");
            }
            else {
              begin = 1;
            }
            fprintf(fp, "%s", zs);
            free(zs);
          }
        }
      }
    }
    fprintf(fp, "\n");
  }

  db->Close(db);
}

int
main(int argc, char **argv)
{
  int ch;
  int ref, tsiyin;
  FILE *fp;
  struct TsiDB *db;
extern char *optarg;
extern int optind, opterr, optopt;

  char *db_name, *op_name;

  db_name = op_name = (char *)NULL;
  ref = 0;
  tsiyin = 0;

  while ((ch = getopt(argc, argv, "d:f:ry")) != -1) {
    switch(ch) {
      case 'd':
        db_name = (char *)strdup(optarg);
        break;
      case 'f':
        op_name = (char *)strdup(optarg);
        break;
      case 'r':
        ref = 1;
        break;
      case 'y':
        tsiyin = 1;
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

  db = tabeTsiDBOpen(DB_TYPE_DB, db_name, 0);
  if (!db) {
    usage();
  }

  if (op_name) {
    fp = fopen(op_name, "w");
    dump(db, fp, ref, tsiyin);
    fclose(fp);
  }
  else {
    dump(db, stdout, ref, tsiyin);
  }

  return(0);
}
