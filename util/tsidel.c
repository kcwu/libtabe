/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: tsidel.c,v 1.2 2001/11/11 12:33:09 thhsieh Exp $
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

#include <db.h>
#include <tabe.h>

#define BUF_SIZE 1000

void
usage(void)
{
  printf("Usage: tsidel -d <TsiDB> [-f input file]\n");
  printf("   -d <TsiDB>     \t path to TsiDB\n");
  printf("   -f <input file>\t input file in plain text (default: stdin)\n");
  exit(0);
}

void
del(struct TsiDB *db, FILE *fp)
{
  struct TsiInfo *tsi;
  int rval, i, j;
  char buf[BUF_SIZE];
  DBT key;

  rval = db->RecordNumber(db);
  if (rval < 0) {
    fprintf(stderr, "tsidel: wrong DB format.\n");
    usage();
  }

  tsi = (struct TsiInfo *)malloc(sizeof(struct TsiInfo));
  tsi->tsi = (ZhiStr)malloc(sizeof(unsigned char)*80);
  memset(tsi->tsi, 0, 80);
  tsi->refcount = -1;
  tsi->yinnum = -1;
  tsi->yindata = (Yin *)NULL;

  i = j = 0;
  while (1) {
    if (!fgets(buf, BUF_SIZE-1, fp)) {
      break;
    }
    sscanf(buf, "%80s", tsi->tsi);
    i++;
    rval = db->Get(db, tsi);
    if (!rval) {
      memset(&key, 0, sizeof(DBT));
      key.data = tsi->tsi;
      key.size = strlen((char *)tsi->tsi);
      rval = ((DB *)(db->dbp))->del((DB *)db->dbp, NULL, &key, 0);
      j++;
    }
    else {
      /* donothing */
    }
  }

  printf("There're %d queries, %d deleted.\n", i, j);
  db->Close(db);
}

int
main(int argc, char **argv)
{
  int ch;
  FILE *fp;
  struct TsiDB *db;
extern char *optarg;
extern int optind, opterr, optopt;

  char *db_name, *op_name;

  db_name = op_name = (char *)NULL;

  while ((ch = getopt(argc, argv, "d:f:")) != -1) {
    switch(ch) {
      case 'd':
        db_name = (char *)strdup(optarg);
        break;
      case 'f':
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

  db = tabeTsiDBOpen(DB_TYPE_DB, db_name, 0);
  if (!db) {
    usage();
  }

  if (op_name) {
    fp = fopen(op_name, "r");
    del(db, fp);
    fclose(fp);
  }
  else {
    del(db, stdin);
  }

  return(0);
}
