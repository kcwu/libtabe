/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: tabe_tsiyindbint.c,v 1.10 2004/01/24 20:14:55 kcwu Exp $
 *
 */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HPUX
#  define _INCLUDE_POSIX_SOURCE
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <db.h>
#ifdef HPUX
#  define _XOPEN_SOURCE_EXTENDED
#  define _INCLUDE_XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
#else
#  include <netinet/in.h>
#endif

#include "tabe.h"
#define DB_VERSION (DB_VERSION_MAJOR*100000+DB_VERSION_MINOR*1000+DB_VERSION_PATCH)

static void tabeTsiYinDBClose(struct TsiYinDB *tsiyindb);
static int  tabeTsiYinDBRecordNumber(struct TsiYinDB *tsiyindb);
static int  tabeTsiYinDBStoreTsiYin(struct TsiYinDB *tsiyindb,
				    struct TsiYinInfo *tsiyin);
static int  tabeTsiYinDBLookupTsiYin(struct TsiYinDB *tsiyindb,
				     struct TsiYinInfo *tsiyin);
static int  tabeTsiYinDBCursorSet(struct TsiYinDB *tsiyindb,
				  struct TsiYinInfo *tsiyin, int set_range);
static int  tabeTsiYinDBCursorNext(struct TsiYinDB *tsiyindb,
				   struct TsiYinInfo *tsiyin);
static int  tabeTsiYinDBCursorPrev(struct TsiYinDB *tsiyindb,
				   struct TsiYinInfo *tsiyin);

struct TsiYinDBDataDB {
  unsigned long int  yinlen;
  unsigned long int  tsinum;
};

struct _tabe_ref_YDBpool {
    char *db_name;
    DB *dbp;
    int flags;
    int ref;
    struct _tabe_ref_YDBpool *next;
} *_tabe_rydb;

static int  TsiYinDBStoreTsiYinDB(struct TsiYinDB *tsiyindb,
				  struct TsiYinInfo *tsiyin);
static int  TsiYinDBLookupTsiYinDB(struct TsiYinDB *tsiyindb,
				   struct TsiYinInfo *tsiyin);
static void TsiYinDBPackDataDB(struct TsiYinInfo *tsiyin, DBT *dat);
static void TsiYinDBUnpackDataDB(struct TsiYinInfo *tsiyin, DBT *dat);

/*
 * open a TsiYinDB with the given type and name
 *
 * return pointer to TsiYinDB if success, NULL if failed
 *
 */
static DB *
tabe_tsiyinDB_DoOpen(const char *db_name, int flags)
{
  DB *dbp=NULL;

#if DB_VERSION >= 300000
  /* create a db handler */
  if ((errno = db_create(&dbp, NULL, 0)) != 0) {
    fprintf(stderr, "db_create: %s\n", db_strerror(errno));
    return (NULL);
  }
#endif

  if (flags & DB_FLAG_CREATEDB) {
    if (flags & DB_FLAG_READONLY) {
      return(NULL);
    }
    else {
#if DB_VERSION >= 401025
      errno = dbp->open(dbp, NULL, db_name, NULL, DB_BTREE, DB_CREATE, 0644);
#elif DB_VERSION >= 300000
      errno = dbp->open(dbp, db_name, NULL, DB_BTREE, DB_CREATE, 0644);
#else
      errno = db_open(db_name, DB_BTREE, DB_CREATE, 0644, NULL, NULL, &dbp);
#endif
    }
  }
  else {
    if (flags & DB_FLAG_READONLY) {
#if DB_VERSION >= 401025
      errno = dbp->open(dbp, NULL, db_name, NULL, DB_BTREE, DB_RDONLY, 0444);
#elif DB_VERSION >= 300000
      errno = dbp->open(dbp, db_name, NULL, DB_BTREE, DB_RDONLY, 0444);
#else
      errno = db_open(db_name, DB_BTREE, DB_RDONLY, 0444, NULL, NULL, &dbp);
#endif
    }
    else {
#if DB_VERSION >= 401025
      errno = dbp->open(dbp, NULL, db_name, NULL, DB_BTREE, 0, 0644);
#elif DB_VERSION >= 300000
      errno = dbp->open(dbp, db_name, NULL, DB_BTREE, 0, 0644);
#else
      errno = db_open(db_name, DB_BTREE, 0, 0644, NULL, NULL, &dbp);
#endif
    }
  }
  if (errno > 0) {
    fprintf(stderr, "tabeTsiYinDBOpen(): Can not open DB file %s (%s).\n",
	    db_name, strerror(errno));
    return(NULL);
  }
  if (errno < 0) {
    /* DB specific errno */
#if DB_VERSION >= 300000
    fprintf(stderr, "tabeTsiYinDBOpen(): %s.\n", db_strerror(errno));
#else
    fprintf(stderr, "tabeTsiYinDBOpen(): DB error opening DB File %s.\n",
	    db_name);
#endif
    return(NULL);
  }

  return dbp;
}

static struct _tabe_ref_YDBpool *
tabe_search_rydbpool(const char *db_name, int flags)
{
  struct _tabe_ref_YDBpool *rydbp = _tabe_rydb;

  while (rydbp) {
    if (strcmp(rydbp->db_name, db_name)==0 && rydbp->flags==flags)
      break;
    rydbp = rydbp->next;
  }
  return rydbp;
}

struct TsiYinDB *
tabeTsiYinDBOpen(int type, const char *db_name, int flags)
{
  struct TsiYinDB *tsiyindb=NULL;
  DB *dbp=NULL;

  switch(type) {
  case DB_TYPE_DB:
    tsiyindb = (struct TsiYinDB *)malloc(sizeof(struct TsiYinDB));
    if (!tsiyindb) {
      perror("tabeTsiYinDBOpen()");
      return(NULL);
    }
    memset(tsiyindb, 0, sizeof(struct TsiYinDB));
    tsiyindb->type = type;
    tsiyindb->flags = flags;

    tsiyindb->Close = tabeTsiYinDBClose;
    tsiyindb->RecordNumber = tabeTsiYinDBRecordNumber;
    tsiyindb->Put = tabeTsiYinDBStoreTsiYin;
    tsiyindb->Get = tabeTsiYinDBLookupTsiYin;
    tsiyindb->CursorSet = tabeTsiYinDBCursorSet;
    tsiyindb->CursorNext = tabeTsiYinDBCursorNext;
    tsiyindb->CursorPrev = tabeTsiYinDBCursorPrev;

    if (! (tsiyindb->flags & DB_FLAG_SHARED))
      dbp = tabe_tsiyinDB_DoOpen(db_name, tsiyindb->flags);
    else {
      struct _tabe_ref_YDBpool *rydbp;
      if ((rydbp = tabe_search_rydbpool(db_name, tsiyindb->flags))== NULL) {
	dbp = tabe_tsiyinDB_DoOpen(db_name, tsiyindb->flags);
	if (dbp != NULL) {
	  rydbp = malloc(sizeof(struct _tabe_ref_YDBpool));
	  rydbp->db_name = (char *)strdup(db_name);
	  rydbp->dbp = dbp;
	  rydbp->flags = flags;
	  rydbp->ref = 1;
	  rydbp->next = _tabe_rydb;
	  _tabe_rydb = rydbp;
	}
      }
      else {
	dbp = rydbp->dbp;
	rydbp->ref ++;
      }
    }

    if (dbp) {
      tsiyindb->db_name = (char *)strdup(db_name);
      tsiyindb->dbp = (void *)dbp;
    }
    else {
      free(tsiyindb);
      tsiyindb = NULL;
    }
    break;
  default:
    fprintf(stderr, "tabeTsiYinDBOpen(): Unknown DB type.\n");
    break;
  }

  return(tsiyindb);
}

/*
 * close and flush DB file
 */
static void
tabe_tsiyinDB_DoClose(struct TsiYinDB *tsiyindb)
{
  DB  *dbp;
  DBC *dbcp;

  switch(tsiyindb->type) {
  case DB_TYPE_DB:
    dbp = (DB *)tsiyindb->dbp;
    dbcp = (DBC *)tsiyindb->dbcp;
    if (dbcp) {
      dbcp->c_close(dbcp);
      dbcp = (void *)NULL;
    }
    if (dbp) {
      dbp->close(dbp, 0);
      dbp = (void *)NULL;
    }
    if (tsiyindb->db_name)
      free(tsiyindb->db_name);
    free(tsiyindb);
    return;
  default:
    fprintf(stderr, "tabeTsiYinDBClose(): Unknown DB type.\n");
    break;
  }
  return;
}

static void
tabeTsiYinDBClose(struct TsiYinDB *tsiyindb)
{
  if (! (tsiyindb->flags & DB_FLAG_SHARED))
    tabe_tsiyinDB_DoClose(tsiyindb);
  else {
    struct _tabe_ref_YDBpool *rydbp;
    rydbp = tabe_search_rydbpool(tsiyindb->db_name, tsiyindb->flags);
    if (rydbp) {
      rydbp->ref --;
      if (rydbp->ref == 0) {
	tabe_tsiyinDB_DoClose(tsiyindb);
	_tabe_rydb = rydbp->next;
	free(rydbp->db_name);
	free(rydbp);
      }
    }
  }
}

/*
 * returns the number of record in TsiYin DB
 */
static int
tabeTsiYinDBRecordNumber(struct TsiYinDB *tsiyindb)
{
  DB *dbp;
  DB_BTREE_STAT *sp;

  switch(tsiyindb->type) {
  case DB_TYPE_DB:
    dbp = (DB *)tsiyindb->dbp;
#if DB_VERSION >= 303011
    errno = dbp->stat(dbp, &sp, 0);
#else
    errno = dbp->stat(dbp, &sp, NULL, 0);
#endif
    if (!errno) {
#if DB_VERSION >= 300000
      return(sp->bt_ndata);  /* or sp->bt_nkeys? */
#else
      return(sp->bt_nrecs);
#endif
    }
    break;
  default:
    fprintf(stderr, "tabeTsiYinDBRecordNumber(): Unknown DB type.\n");
    break;
  }
  return(0);
}

/*
 * store TsiYin in designated DB
 *
 * return 0 if success, -1 if failed
 *
 */
static int
tabeTsiYinDBStoreTsiYin(struct TsiYinDB *tsiyindb, struct TsiYinInfo *tsiyin)
{
  int rval;

  if (tsiyindb->flags & DB_FLAG_READONLY) {
    fprintf(stderr, "tabeTsiDBStoreTsi(): writing a read-only DB.\n");
    return(-1);
  }

  switch(tsiyindb->type) {
  case DB_TYPE_DB:
    rval = TsiYinDBStoreTsiYinDB(tsiyindb, tsiyin);
    return(rval);
  default:
    fprintf(stderr, "tabeTsiYinDBStoreTsiYin(): Unknown DB type.\n");
    break;
  }

  return(-1);
}

/*
 * lookup TsiYin in designated DB
 *
 * return 0 if success, -1 if failed
 *
 */
static int
tabeTsiYinDBLookupTsiYin(struct TsiYinDB *tsiyindb, struct TsiYinInfo *tsiyin)
{
  int rval;

  switch(tsiyindb->type) {
  case DB_TYPE_DB:
    rval = TsiYinDBLookupTsiYinDB(tsiyindb, tsiyin);
    return(rval);
  default:
    fprintf(stderr, "tabeTsiYinDBLookupTsiYin(): Unknown DB type.\n");
    break;
  }

  return(-1);
}

static void
TsiYinDBPackDataDB(struct TsiYinInfo *tsiyin, DBT *dat)
{
  struct TsiYinDBDataDB *d;
  int datalen, tsilen;
  unsigned char *data;

  tsilen = tsiyin->yinlen * tsiyin->tsinum * 2;
  datalen = sizeof(struct TsiYinDBDataDB) + sizeof(unsigned char)*tsilen;
  data = (unsigned char *)malloc(sizeof(unsigned char)*datalen);
  memset(data, 0, sizeof(unsigned char)*datalen);
  d = (struct TsiYinDBDataDB *)data;

  /* convert to network byte order */
  d->yinlen = htonl(tsiyin->yinlen);
  d->tsinum = htonl(tsiyin->tsinum);
  memcpy(data+sizeof(struct TsiYinDBDataDB), tsiyin->tsidata,
         sizeof(unsigned char)*tsilen);

  dat->data = data;
  dat->size = datalen;
}

static void
TsiYinDBUnpackDataDB(struct TsiYinInfo *tsiyin, DBT *dat)
{
  int tsilen;
  struct TsiYinDBDataDB d;

  memset(&d, 0, sizeof(struct TsiYinDBDataDB));
  memcpy(&d, dat->data, sizeof(struct TsiYinDBDataDB));
  /* convert to system byte order */
  tsiyin->yinlen = ntohl(d.yinlen);
  tsiyin->tsinum = ntohl(d.tsinum);
  tsilen         = tsiyin->yinlen * tsiyin->tsinum * 2;

  if (tsiyin->tsidata) {
    free(tsiyin->tsidata);
    tsiyin->tsidata = (ZhiStr)NULL;
  }

  if (tsilen) {
    struct TsiYinDBDataDB *stmp;
    stmp = (struct TsiYinDBDataDB *)dat->data + 1;
    tsiyin->tsidata = (ZhiStr)malloc(sizeof(unsigned char)*tsilen);
    memcpy((void *)tsiyin->tsidata, (void *)stmp, sizeof(unsigned char)*tsilen);
  }
}

static int
TsiYinDBStoreTsiYinDB(struct TsiYinDB *tsiyindb, struct TsiYinInfo *tsiyin)
{
  DBT key, dat;
  DB *dbp;

  memset(&key, 0, sizeof(key));
  memset(&dat, 0, sizeof(dat));

  key.data = tsiyin->yin;
  key.size = sizeof(Yin)*tsiyin->yinlen;

  TsiYinDBPackDataDB(tsiyin, &dat);

  dbp = tsiyindb->dbp;
  if (tsiyindb->flags & DB_FLAG_OVERWRITE) {
    errno = dbp->put(dbp, NULL, &key, &dat, 0);
  }
  else {
    errno = dbp->put(dbp, NULL, &key, &dat, DB_NOOVERWRITE);
  }
  if (errno > 0) {
    fprintf(stderr, "TsiYinDBStoreTsiYinDB(): can not store DB. (%s)\n",
	    strerror(errno));
    return(-1);
  }
  if (errno < 0) {
    switch(errno) {
    case DB_KEYEXIST:
#ifdef MYDEBUG
      fprintf(stderr, "TsiYinDBStoreTsiYinDB(): tsiyin exist.\n");
#endif
      return(-1);
    default:
      fprintf(stderr, "TsiYinDBStoreTsiYinDB(): unknown DB error.\n");
      return(-1);
    }
  }

  free(dat.data);
  if (!(tsiyindb->flags & DB_FLAG_NOSYNC)) {
    dbp->sync(dbp, 0);
  }
  return(0);
}

static int
TsiYinDBLookupTsiYinDB(struct TsiYinDB *tsiyindb, struct TsiYinInfo *tsiyin)
{
  DBT key, dat;
  DB *dbp;

  memset(&key, 0, sizeof(key));
  memset(&dat, 0, sizeof(dat));

  key.data = tsiyin->yin;
  key.size = sizeof(Yin)*tsiyin->yinlen;

  dbp = tsiyindb->dbp;
  errno = dbp->get(dbp, NULL, &key, &dat, 0);
  if (errno > 0) {
    fprintf(stderr, "TsiYinDBLookupTsiYinDB(): can not lookup DB. (%s)\n",
	    strerror(errno));
    return(-1);
  }
  if (errno < 0) {
    switch(errno) {
    case DB_NOTFOUND:
#ifdef MYDEBUG
/*    fprintf(stderr, "TsiYinDBLookupTsiYinDB(): tsiyin does not exist.\n"); */
#endif
      return(-1);
    default:
      fprintf(stderr, "TsiYinDBLookupTsiYinDB(): unknown DB error.\n");
      return(-1);
    }
  }

  TsiYinDBUnpackDataDB(tsiyin, &dat);

  return(0);
}

static int
tabeTsiYinDBCursorSet(struct TsiYinDB *tsiyindb, struct TsiYinInfo *tsiyin,
		      int set_range)
{
  DB  *dbp;
  DBC *dbcp;
  DBT  key, dat;

  dbp  = tsiyindb->dbp;
  dbcp = tsiyindb->dbcp;
  if (dbcp) {
    dbcp->c_close(dbcp);
  }

#if DB_VERSION >= 206004
  dbp->cursor(dbp, NULL, &dbcp, 0);
#else
  dbp->cursor(dbp, NULL, &dbcp);
#endif
  tsiyindb->dbcp = dbcp;

  memset(&key, 0, sizeof(key));
  memset(&dat, 0, sizeof(dat));

  if (tsiyin->yinlen) {
    key.data = tsiyin->yin;
    key.size = tsiyin->yinlen * sizeof(Yin);
    if (set_range) {
      errno = dbcp->c_get(dbcp, &key, &dat, DB_SET_RANGE);

      if (tsiyin->yin) {
	free(tsiyin->yin);
	tsiyin->yin = (Yin *)NULL;
      }
      tsiyin->yin = (Yin *)malloc(key.size);
      memcpy(tsiyin->yin, key.data, key.size);
    }
    else
      errno = dbcp->c_get(dbcp, &key, &dat, DB_SET);
  }
  else {
    errno = dbcp->c_get(dbcp, &key, &dat, DB_FIRST);
  }
  if (errno > 0) {
    fprintf(stderr, "tabeTsiYinDBCursorSet(): error setting cursor. (%s)\n",
	    strerror(errno));
    return(-1);
  }
  if (errno < 0) {
    switch(errno) {
    default:
      fprintf(stderr, "tabeTsiYinDBCursorSet(): Unknown error.\n");
      return(-1);
    }
  }

  if (tsiyin->yin) {
    free(tsiyin->yin);
    tsiyin->yin = (Yin *)NULL;
  }
  tsiyin->yin = (Yin *)malloc(key.size);
  memcpy(tsiyin->yin, key.data, key.size);

  TsiYinDBUnpackDataDB(tsiyin, &dat);  

  return(0);
}

static int
tabeTsiYinDBCursorNext(struct TsiYinDB *tsiyindb, struct TsiYinInfo *tsiyin)
{
  DB  *dbp;
  DBC *dbcp;
  DBT  key, dat;

  dbp  = tsiyindb->dbp;
  dbcp = tsiyindb->dbcp;
  if (!dbcp) {
    return(-1);
  }

  memset(&key, 0, sizeof(key));
  memset(&dat, 0, sizeof(dat));

  errno = dbcp->c_get(dbcp, &key, &dat, DB_NEXT);
  if (errno < 0) {
    switch(errno) {
    case DB_NOTFOUND:
      return(-1);
    default:
      return(-1);
    }
  }

  if (tsiyin->yin) {
    free(tsiyin->yin);
    tsiyin->yin = (Yin *)NULL;
  }
  tsiyin->yin = (Yin *)malloc(key.size);
  memcpy(tsiyin->yin, key.data, key.size);

  TsiYinDBUnpackDataDB(tsiyin, &dat);

  return(0);
}

static int
tabeTsiYinDBCursorPrev(struct TsiYinDB *tsiyindb, struct TsiYinInfo *tsiyin)
{
  DB  *dbp;
  DBC *dbcp;
  DBT  key, dat;

  dbp  = tsiyindb->dbp;
  dbcp = tsiyindb->dbcp;
  if (!dbcp) {
    return(-1);
  }

  memset(&key, 0, sizeof(key));
  memset(&dat, 0, sizeof(dat));

  errno = dbcp->c_get(dbcp, &key, &dat, DB_PREV);
  if (errno < 0) {
    switch(errno) {
    case DB_NOTFOUND:
      return(-1);
    default:
      return(-1);
    }
  }

  if (tsiyin->yin) {
    free(tsiyin->yin);
    tsiyin->yin = (Yin *)NULL;
  }
  tsiyin->yin = (Yin *)malloc(key.size);
  memcpy(tsiyin->yin, key.data, key.size);

  TsiYinDBUnpackDataDB(tsiyin, &dat);

  return(0);
}
