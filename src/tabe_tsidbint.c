/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: tabe_tsidbint.c,v 1.9 2004/01/24 20:14:55 kcwu Exp $
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

static void tabeTsiDBClose(struct TsiDB *tsidb);
static int  tabeTsiDBRecordNumber(struct TsiDB *tsidb);
static int  tabeTsiDBStoreTsi(struct TsiDB *tsidb, struct TsiInfo *tsi);
static int  tabeTsiDBLookupTsi(struct TsiDB *tsidb, struct TsiInfo *tsi);
static int  tabeTsiDBCursorSet(struct TsiDB *tsidb, struct TsiInfo *tsi,
			       int set_range);
static int  tabeTsiDBCursorNext(struct TsiDB *tsidb, struct TsiInfo *tsi);
static int  tabeTsiDBCursorPrev(struct TsiDB *tsidb, struct TsiInfo *tsi);

struct TsiDBDataDB {
  unsigned long int  refcount;
  unsigned long int  yinnum;
/*
  unsigned char      reserved[32];
*/
};

struct _tabe_ref_DBpool {
    char *db_name;
    DB *dbp;
    int flags;
    int ref;
    struct _tabe_ref_DBpool *next;
} *_tabe_rdb;

static int  TsiDBStoreTsiDB(struct TsiDB *tsidb, struct TsiInfo *tsi);
static int  TsiDBLookupTsiDB(struct TsiDB *tsidb, struct TsiInfo *tsi);
static void TsiDBPackDataDB(struct TsiInfo *tsi, DBT *dat);
static void TsiDBUnpackDataDB(struct TsiInfo *tsi, DBT *dat, int unpack_yin);

/*
 * open a TsiDB with the given type and name
 *
 * return pointer to TsiDB if success, NULL if failed
 *
 */
static DB *
tabe_tsiDB_DoOpen(const char *db_name, int flags)
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
    fprintf(stderr, "tabeTsiDBOpen(): Can not open DB file %s (%s).\n",
	    db_name, strerror(errno));
    return(NULL);
  }
  if (errno < 0) {
    /* DB specific errno */
#if DB_VERSION >= 300000
    fprintf(stderr, "tabeTsiDBOpen(): %s.\n", db_strerror(errno));
#else
    fprintf(stderr, "tabeTsiDBOpen(): DB error opening DB File %s.\n", db_name);
#endif
    return(NULL);
  }

  return dbp;
}

static struct _tabe_ref_DBpool *
tabe_search_rdbpool(const char *db_name, int flags)
{
  struct _tabe_ref_DBpool *rdbp = _tabe_rdb;

  while (rdbp) {
    if (strcmp(rdbp->db_name, db_name)==0 && rdbp->flags==flags)
      break;
    rdbp = rdbp->next;
  }
  return rdbp;
}

struct TsiDB *
tabeTsiDBOpen(int type, const char *db_name, int flags)
{
  struct TsiDB *tsidb=NULL;
  DB *dbp=NULL;

  switch(type) {
  case DB_TYPE_DB:
    tsidb = (struct TsiDB *)malloc(sizeof(struct TsiDB));
    if (!tsidb) {
      perror("tabeTsiDBOpen()");
      return(NULL);
    }
    memset(tsidb, 0, sizeof(struct TsiDB));
    tsidb->type = type;
    tsidb->flags = flags;

    tsidb->Close = tabeTsiDBClose;
    tsidb->RecordNumber = tabeTsiDBRecordNumber;
    tsidb->Put = tabeTsiDBStoreTsi;
    tsidb->Get = tabeTsiDBLookupTsi;
    tsidb->CursorSet = tabeTsiDBCursorSet;
    tsidb->CursorNext = tabeTsiDBCursorNext;
    tsidb->CursorPrev = tabeTsiDBCursorPrev;

    if (! (tsidb->flags & DB_FLAG_SHARED))
      dbp = tabe_tsiDB_DoOpen(db_name, tsidb->flags);
    else {
      struct _tabe_ref_DBpool *rdbp;
      if ((rdbp = tabe_search_rdbpool(db_name, tsidb->flags))== NULL) {
	dbp = tabe_tsiDB_DoOpen(db_name, tsidb->flags);
	if (dbp != NULL) {
	  rdbp = malloc(sizeof(struct _tabe_ref_DBpool));
          rdbp->db_name = (char *)strdup(db_name);
	  rdbp->dbp = dbp;
	  rdbp->flags = flags;
	  rdbp->ref = 1;
	  rdbp->next = _tabe_rdb;
	  _tabe_rdb = rdbp;
	}
      }
      else {
	dbp = rdbp->dbp;
	rdbp->ref ++;
      }
    }

    if (dbp) {
      tsidb->db_name = (char *)strdup(db_name);
      tsidb->dbp = (void *)dbp;
    }
    else {
      free(tsidb);
      tsidb = NULL;
    }
    break;
  default:
    fprintf(stderr, "tabeTsiDBOpen(): Unknown DB type.\n");
    break;
  }

  return(tsidb);
}

/*
 * close and flush DB file
 */
static void
tabe_tsiDB_DoClose(struct TsiDB *tsidb)
{
  DB  *dbp;
  DBC *dbcp;

  switch(tsidb->type) {
  case DB_TYPE_DB:
    dbp = (DB *)tsidb->dbp;
    dbcp = (DBC *)tsidb->dbcp;
    /* close it and reset the DB pointer */
    if (dbcp) {
      dbcp->c_close(dbcp);
      dbcp = (void *)NULL;
    }
    if (dbp) {
      dbp->close(dbp, 0);
      dbp = (void *)NULL;
    }
    if (tsidb->db_name)
      free(tsidb->db_name);
    free(tsidb);
    return;
  default:
    fprintf(stderr, "tabeTsiDBClose(): Unknown DB type.\n");
    break;
  }
  return;
}

static void
tabeTsiDBClose(struct TsiDB *tsidb)
{
  if (! (tsidb->flags & DB_FLAG_SHARED))
    tabe_tsiDB_DoClose(tsidb);
  else {
    struct _tabe_ref_DBpool *rdbp;
    rdbp = tabe_search_rdbpool(tsidb->db_name, tsidb->flags);
    if (rdbp) {
      rdbp->ref --;
      if (rdbp->ref == 0) {
	tabe_tsiDB_DoClose(tsidb);
	_tabe_rdb = rdbp->next;
	free(rdbp->db_name);
	free(rdbp);
      }
    }
  }
}

/*
 * returns the number of record in Tsi DB
 */
static int
tabeTsiDBRecordNumber(struct TsiDB *tsidb)
{
  DB *dbp;
  DB_BTREE_STAT *sp;

  switch(tsidb->type) {
  case DB_TYPE_DB:
    dbp = (DB *)tsidb->dbp;
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
    fprintf(stderr, "tabeTsiDBRecordNumber(): Unknown DB type.\n");
    break;
  }
  return(0);
}

/*
 * store Tsi in designated DB
 *
 * return 0 if success, -1 if failed
 *
 */
static int
tabeTsiDBStoreTsi(struct TsiDB *tsidb, struct TsiInfo *tsi)
{
  int rval;

  if (tsidb->flags & DB_FLAG_READONLY) {
    fprintf(stderr, "tabeTsiDBStoreTsi(): writing a read-only DB.\n");
    return(-1);
  }

  switch(tsidb->type) {
  case DB_TYPE_DB:
    rval = TsiDBStoreTsiDB(tsidb, tsi);
    return(rval);
  default:
    fprintf(stderr, "tabeTsiDBStoreTsi(): Unknown DB type.\n");
    break;
  }

  return(-1);
}

/*
 * lookup Tsi in designated DB
 *
 * return 0 if success, -1 if failed
 *
 */
static int
tabeTsiDBLookupTsi(struct TsiDB *tsidb, struct TsiInfo *tsi)
{
  int rval;

  switch(tsidb->type) {
  case DB_TYPE_DB:
    rval = TsiDBLookupTsiDB(tsidb, tsi);
    return(rval);
  default:
    fprintf(stderr, "tabeTsiDBLookupTsi(): Unknown DB type.\n");
    break;
  }

  return(-1);
}

static void
TsiDBPackDataDB(struct TsiInfo *tsi, DBT *dat)
{
  struct TsiDBDataDB *d;
  int datalen, i, yinlen;
  unsigned char *data;

  yinlen = tsi->yinnum * strlen((char *)tsi->tsi)/2;
  datalen = sizeof(struct TsiDBDataDB) + sizeof(Yin)*yinlen;
  data = (unsigned char *)malloc(sizeof(unsigned char)*datalen);
  memset(data, 0, sizeof(unsigned char)*datalen);
  d = (struct TsiDBDataDB *)data;

  /* convert to network byte order */
  d->refcount = htonl(tsi->refcount);
  d->yinnum   = htonl(tsi->yinnum);
  for (i = 0; i < yinlen; i++) {
    ((Yin *)(data+sizeof(struct TsiDBDataDB)))[i] = htons(tsi->yindata[i]);
  }

  dat->data = data;
  dat->size = datalen;
}

static void
TsiDBUnpackDataDB(struct TsiInfo *tsi, DBT *dat, int unpack_yin)
{
  int i, yinlen;
  struct TsiDBDataDB d;

  memset(&d, 0, sizeof(struct TsiDBDataDB));
  memcpy(&d, dat->data, sizeof(struct TsiDBDataDB));
  /* convert to system byte order */
  tsi->refcount = ntohl(d.refcount);
  if (! unpack_yin)
    return;
  tsi->yinnum = ntohl(d.yinnum);
  yinlen      = tsi->yinnum*(strlen((char *)tsi->tsi)/2);

  if (tsi->yindata) {
    free(tsi->yindata);
    tsi->yindata = (Yin *)NULL;
  }

  if (yinlen) {
#ifdef HPUX
/*
 *  In HP-UX ANSI C compiler, it cannot handle the
 *	void *a = (void *)b + (int)offset;
 *
 *  it will report:
 *  error 1539: Cannot do arithmetic with pointers to objects of unknown size.
 *
 *  so we have to use the following trick to avoid errors.
 *
 *					by T.H.Hsieh <thhsieh@linux.org.tw>
 */
    struct TsiDBDataDB *stmp;
    stmp = (struct TsiDBDataDB *)dat->data + 1;
    tsi->yindata = (Yin *)malloc(sizeof(Yin)*yinlen);
    memcpy((void *)tsi->yindata, (void *)stmp, sizeof(Yin)*yinlen);
#else
    tsi->yindata = (Yin *)malloc(sizeof(Yin)*yinlen);
    memcpy(tsi->yindata, dat->data+sizeof(struct TsiDBDataDB),
	   sizeof(Yin)*yinlen);
#endif
  }

  /* convert to system byte order */
  for (i = 0; i < yinlen; i++) {
    tsi->yindata[i] = ntohs(tsi->yindata[i]);
  }
}

static int
TsiDBStoreTsiDB(struct TsiDB *tsidb, struct TsiInfo *tsi)
{
  DBT key, dat;
  DB *dbp;

  memset(&key, 0, sizeof(key));
  memset(&dat, 0, sizeof(dat));

  key.data = tsi->tsi;
  key.size = strlen((char *)tsi->tsi);

  TsiDBPackDataDB(tsi, &dat);

  dbp = tsidb->dbp;
  if (tsidb->flags & DB_FLAG_OVERWRITE) {
    errno = dbp->put(dbp, NULL, &key, &dat, 0);
  }
  else {
    errno = dbp->put(dbp, NULL, &key, &dat, DB_NOOVERWRITE);
  }
  if (errno > 0) {
    fprintf(stderr, "TsiDBStoreTsiDB(): can not store DB. (%s)\n",
	    strerror(errno));
    return(-1);
  }
  if (errno < 0) {
    switch(errno) {
    case DB_KEYEXIST:
#ifdef MYDEBUG
      fprintf(stderr, "TsiDBStoreTsiDB(): tsi exist.\n");
#endif
      return(-1);
    default:
      fprintf(stderr, "TsiDBStoreTsiDB(): unknown DB error.\n");
      return(-1);
    }
  }

  free(dat.data);
  if (!(tsidb->flags & DB_FLAG_NOSYNC)) {
    dbp->sync(dbp, 0);
  }
  return(0);
}

static int
TsiDBLookupTsiDB(struct TsiDB *tsidb, struct TsiInfo *tsi)
{
  DBT key, dat;
  DB *dbp;

  memset(&key, 0, sizeof(key));
  memset(&dat, 0, sizeof(dat));

  key.data = tsi->tsi;
  key.size = strlen((char *)tsi->tsi);

  dbp = tsidb->dbp;
  errno = dbp->get(dbp, NULL, &key, &dat, 0);
  if (errno > 0) {
    fprintf(stderr, "TsiDBLookupTsiDB(): can not lookup DB. (%s)\n",
	    strerror(errno));
    return(-1);
  }
  if (errno < 0) {
    switch(errno) {
    case DB_NOTFOUND:
#ifdef MYDEBUG
      fprintf(stderr, "TsiDBLookupTsiDB(): tsi does not exist.\n");
#endif
      return(-1);
    default:
      fprintf(stderr, "TsiDBLookupTsiDB(): unknown DB error.\n");
      return(-1);
    }
  }

  TsiDBUnpackDataDB(tsi, &dat, !(tsidb->flags & DB_FLAG_NOUNPACK_YIN));

  return(0);
}

static int
tabeTsiDBCursorSet(struct TsiDB *tsidb, struct TsiInfo *tsi, int set_range)
{
  DB  *dbp;
  DBC *dbcp;
  DBT  key, dat;

  dbp  = tsidb->dbp;
  dbcp = tsidb->dbcp;
  if (dbcp) {
    dbcp->c_close(dbcp);
  }

#if DB_VERSION >= 206004
  dbp->cursor(dbp, NULL, &dbcp, 0);
#else
  dbp->cursor(dbp, NULL, &dbcp);
#endif
  tsidb->dbcp = dbcp;

  memset(&key, 0, sizeof(key));
  memset(&dat, 0, sizeof(dat));

  if (strlen((char *)tsi->tsi)) {
    key.data = tsi->tsi;
    key.size = strlen((char *)tsi->tsi);
    if (set_range) {
      errno = dbcp->c_get(dbcp, &key, &dat, DB_SET_RANGE);

     /* we depends on the caller to allocate buffer large enough */
      *((char *)tsi->tsi) = '\0';
      strncat((char *)tsi->tsi, (char *)key.data, key.size);
    }
    else
      errno = dbcp->c_get(dbcp, &key, &dat, DB_SET);
  }
  else {
    errno = dbcp->c_get(dbcp, &key, &dat, DB_FIRST);
  }
  if (errno > 0) {
    fprintf(stderr, "tabeTsiDBCursorSet(): error setting cursor. (%s)\n",
	    strerror(errno));
    return(-1);
  }
  if (errno < 0) {
    switch(errno) {
    default:
      fprintf(stderr, "tabeTsiDBCursorSet(): Unknown error.\n");
      return(-1);
    }
  }

  /* we depends on the caller to allocate enough large buffer */
  *((char *)tsi->tsi) = '\0';
  strncat((char *)tsi->tsi, (char *)key.data, key.size);
  TsiDBUnpackDataDB(tsi, &dat, !(tsidb->flags & DB_FLAG_NOUNPACK_YIN));

  return(0);
}

static int
tabeTsiDBCursorNext(struct TsiDB *tsidb, struct TsiInfo *tsi)
{
  DB  *dbp;
  DBC *dbcp;
  DBT  key, dat;

  dbp  = tsidb->dbp;
  dbcp = tsidb->dbcp;
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

  /* we depends on the caller to allocate buffer large enough */
  *((char *)tsi->tsi) = '\0';
  strncat((char *)tsi->tsi, (char *)key.data, key.size);

  TsiDBUnpackDataDB(tsi, &dat, !(tsidb->flags & DB_FLAG_NOUNPACK_YIN));

  return(0);
}

static int
tabeTsiDBCursorPrev(struct TsiDB *tsidb, struct TsiInfo *tsi)
{
  DB  *dbp;
  DBC *dbcp;
  DBT  key, dat;

  dbp  = tsidb->dbp;
  dbcp = tsidb->dbcp;
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

  /* we depends on the caller to allocate enough large buffer */
  *((char *)tsi->tsi) = '\0';
  strncat((char *)tsi->tsi, (char *)key.data, key.size);

  TsiDBUnpackDataDB(tsi, &dat, !(tsidb->flags & DB_FLAG_NOUNPACK_YIN));

  return(0);
}
