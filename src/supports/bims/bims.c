/*
 * Copyright (c) 1999, Computer Systems and Communication Lab,
 *                     Institute of Information Science, Academia Sinica.
 * Copyright 1999, Pai-Hsiang Hsiao.
 *      All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * . Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * . Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * . Neither the name of the Computer Systems and Communication Lab
 *   nor the names of its contributors may be used to endorse or
 *   promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: bims.c,v 1.7 2001/09/22 10:52:06 thhsieh Exp $
 */
#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bims.h"

/* primary tsi and tsiyin database */
static struct TsiDB    *tdb = (struct TsiDB *)NULL;
static struct TsiYinDB *ydb = (struct TsiYinDB *)NULL;

/* allow the use of multiple database */
static struct TsiDB    **tdb_pool = (struct TsiDB **)NULL;
static struct TsiYinDB **ydb_pool = (struct TsiYinDB **)NULL;
static int               len_pool = 0;

static int bimsTsiDBPoolSearch(struct TsiInfo *ti);
static int bimsTsiYinDBPoolSearch(struct TsiYinInfo *ty);

static int  bimsZuYinContextCheck(struct ZuYinContext *zc);
static int  bimsZuYinContextInput(struct ZuYinContext *zc, int index);
static int  bimsEten26ZuYinContextInput(struct ZuYinContext *zc, int index);
static int  bimsHsuZuYinContextInput(struct ZuYinContext *zc, int index);
static void bimsZuYinContextClear(struct ZuYinContext *zc);

static int  bimsVerifyPindown(struct bimsContext *bc,
			      struct TsiYinInfo *ty, int yinoff, int index);

static void bimsContextSmartEdit(struct bimsContext *bc);

static ZuYinIndex bimsZozyKeyToZuYinIndex(int key);
static ZuYinIndex bimsEtenKeyToZuYinIndex(int key);
static ZuYinIndex bimsEten26KeyToZuYinIndex(int key);
static ZuYinIndex bimsHsuKeyToZuYinIndex(int key);

/*
 * initialize the bims subsystem
 *
 * The two databases are opened as primary databases, and will be
 * updated.  All other databases opened via bimsDBPoolAppend() or
 * bimsDBPoolPrepend() will be opened as readonly.
 *
 * tdb and ydb must be non-zero throughout the system.
 */
int
bimsInit(char *tsidb_name, char *yindb_name)
{
  if (!tsidb_name || !yindb_name) {
    return(-1);
  }

  if (!tdb) {
    tdb = tabeTsiDBOpen(DB_TYPE_DB, tsidb_name, DB_FLAG_OVERWRITE);
    /* fallback to readonly mode */
    if (!tdb) {
      tdb = tabeTsiDBOpen(DB_TYPE_DB, tsidb_name, DB_FLAG_READONLY);
    }
    if (!tdb) {
      return(-1);
    }
  }
  if (!ydb) {
    ydb = tabeTsiYinDBOpen(DB_TYPE_DB, yindb_name, DB_FLAG_OVERWRITE);
    /* fallback to readonly mode */
    if (!ydb) {
      ydb = tabeTsiYinDBOpen(DB_TYPE_DB, yindb_name, DB_FLAG_READONLY);
    }
    if (!ydb) {
      tdb->Close(tdb);
      return(-1);
    }
  }

  return(0);
}

/*
 * destroy the bims subsystem
 */
void
bimsDestroy(void)
{
  int i;

  /* close the primary database */
  tdb->Close(tdb);
  tdb = NULL;
  ydb->Close(ydb);
  ydb = NULL;

  /* close all other databases, if any */
  if (len_pool > 0) {
    tdb_pool[0] = NULL;
    ydb_pool[0] = NULL;
  }
  for (i = 1; i < len_pool; i++) {
    if (tdb_pool[i]) {
      (tdb_pool[i])->Close(tdb_pool[i]);
      tdb_pool[i] = NULL;
    }
    if (ydb_pool[i]) {
      (ydb_pool[i])->Close(ydb_pool[i]);
      ydb_pool[i] = NULL;
    }
  }

  free(tdb_pool);
  tdb_pool = NULL;
  free(ydb_pool);
  ydb_pool = NULL;
  len_pool = 0;
}

/*
 * Append a pair of tsi and tsiyin db into the pool
 */
int
bimsDBPoolAppend(char *tsidb_name, char *yindb_name)
{
  struct TsiDB *t;
  struct TsiYinDB *y;
  int len = 0;

  /* check and open both database */
  if (!tsidb_name || !yindb_name) {
    return(-1);
  }

  t = tabeTsiDBOpen(DB_TYPE_DB, tsidb_name, DB_FLAG_READONLY);
  if (!t) {
    return(-1);
  }

  y = tabeTsiYinDBOpen(DB_TYPE_DB, yindb_name, DB_FLAG_READONLY);
  if (!y) {
    t->Close(t);
    return(-1);
  }

  /* append them to the pool */
  if (len_pool == 0) {
    len = 2;  /* the primary plus this new one */
    tdb_pool = (struct TsiDB **)calloc(len, sizeof(struct TsiDB *));
    ydb_pool = (struct TsiYinDB **)calloc(len, sizeof(struct TsiYinDB *));

    if (!tdb_pool || !ydb_pool) {
      t->Close(t);
      y->Close(y);
      return (-1);
    }

    /* put primary in */
    tdb_pool[0] = tdb;
    ydb_pool[0] = ydb;
    /* append the new one */
    tdb_pool[1] = t;
    ydb_pool[1] = y;
  }
  else {
    void *tmp;

    len = len_pool + 1;  /* the primary plus this new one */
    tmp = (void *)realloc(tdb_pool, len*sizeof(struct TsiDB *));
    if (!tmp) {
      t->Close(t);
      y->Close(y);
      return (-1);
    }
    tdb_pool = (struct TsiDB **)tmp;
    tmp = (void *)realloc(ydb_pool, len*sizeof(struct TsiYinDB *));
    if (!tmp) {
      t->Close(t);
      y->Close(y);
      return (-1);
    }
    ydb_pool = (struct TsiYinDB **)tmp;

    /* append the new one */
    tdb_pool[len_pool] = t;
    ydb_pool[len_pool] = y;
  }

  len_pool = len;

  return (0);
}

/*
 * Prepend a pair of tsi and tsiyin db into the pool
 */
int
bimsDBPoolPrepend(char *tsidb_name, char *yindb_name)
{
  struct TsiDB *t;
  struct TsiYinDB *y;
  int len = 0;

  /* check and open both database */
  if (!tsidb_name || !yindb_name) {
    return(-1);
  }

  t = tabeTsiDBOpen(DB_TYPE_DB, tsidb_name, DB_FLAG_READONLY);
  if (!t) {
    return(-1);
  }

  y = tabeTsiYinDBOpen(DB_TYPE_DB, yindb_name, DB_FLAG_READONLY);
  if (!y) {
    t->Close(t);
    return(-1);
  }

  /* prepend them to the pool */
  if (len_pool == 0) {
    len = 2;  /* the primary plus this new one */
    tdb_pool = (struct TsiDB **)calloc(len, sizeof(struct TsiDB *));
    ydb_pool = (struct TsiYinDB **)calloc(len, sizeof(struct TsiYinDB *));

    if (!tdb_pool || !ydb_pool) {
      t->Close(t);
      y->Close(y);
      return (-1);
    }

    /* put primary in */
    tdb_pool[1] = tdb;
    ydb_pool[1] = ydb;
    /* prepend the new one */
    tdb_pool[0] = t;
    ydb_pool[0] = y;
  }
  else {
    char *tmp;

    len = len_pool + 1;  /* the primary plus this new one */
    tmp = (char *)realloc(tdb_pool, len*sizeof(struct TsiDB *));
    if (!tmp) {
      t->Close(t);
      y->Close(y);
      return (-1);
    }

    /* memmove is not destructive */
    memmove(tmp+sizeof(struct TsiDB *), tmp, len_pool*sizeof(struct TsiDB *));
    tdb_pool = (struct TsiDB **)tmp;
    tmp = (void *)realloc(ydb_pool, len*sizeof(struct TsiYinDB *));
    if (!tmp) {
      t->Close(t);
      y->Close(y);
      return (-1);
    }
    ydb_pool = (struct TsiYinDB **)tmp;

    /* prepend the new one */
    tdb_pool[0] = t;
    ydb_pool[0] = y;
  }

  len_pool = len;

  return (0);
}

/*
 * remove a pair of tsi and tsiyin db from the pool
 *
 * Note that the len_pool will not be affected, just the entries
 * become invalid.
 */
int
bimsDBPoolDelete(char *tsidb_name, char *yindb_name)
{
  int i, j, rval;

  rval = 0;
  if (len_pool == 0) {
    return (0);
  }

  for (i = 0; i < len_pool; i++) {
    if (tdb_pool && tdb_pool[i]) {
      if (!strcmp(tdb_pool[i]->db_name, tsidb_name)) {
	tdb_pool[i]->Close(tdb_pool[i]);
	/* only remove the first one found */
	tdb_pool[i] = NULL;
	break;
      }
    }
  }

  for (j = 0; j < len_pool; j++) {
    if (ydb_pool && ydb_pool[j]) {
      if (!strcmp(ydb_pool[j]->db_name, yindb_name)) {
	ydb_pool[j]->Close(ydb_pool[j]);
	/* only remove the first one found */
	ydb_pool[j] = NULL;
	break;
      }
    }
  }

  if (i != j) {
    fprintf(stderr, "bimsDBPoolDelete: remove dbs that are not in pair\n");
  }

  return (rval);
}

int
bimsReturnDBPool(struct TsiDB **tsidb, struct TsiYinDB **yindb)
{
  int i, cnt=0;

  if (len_pool > 0) {
    *tsidb = malloc(sizeof(struct TsiDB *) * len_pool);
    *yindb = malloc(sizeof(struct TsiYinDB *) * len_pool);
    for (i=0; i<len_pool; i++) {
      if (tdb_pool[i] != NULL && ydb_pool[i] != NULL) {
        tsidb[cnt] = tdb_pool[i];
        yindb[cnt] = ydb_pool[i];
        cnt ++;
      }
    }
  }
  else if (tdb != NULL && ydb != NULL) {
    *tsidb = malloc(sizeof(struct TsiDB *));
    *yindb = malloc(sizeof(struct TsiYinDB *));
    *tsidb = tdb;
    *yindb = ydb;
    cnt = 1;
  }
  return cnt;
}

/*
 * maintain a in-use BC list and a free BC list,
 * suggested by Tung-Han Hsieh
 */
static struct bimsContext *freebc = (struct bimsContext *)NULL;
static struct bimsContext *bucket = (struct bimsContext *)NULL;

/*
 * given a bc ID, return the bims client context
 *
 * this function always return valid bims client context,
 * unless bimsInit() does not called prior to the call to it
 */
struct bimsContext *
bimsGetBC(unsigned long int bcid)
{
  struct bimsContext *bc;

  if (!bucket && !freebc) { /* bucket is not initialized yet */
    bucket = (struct bimsContext *)
      malloc(sizeof(struct bimsContext));
    memset(bucket, 0, sizeof(struct bimsContext));
    bc = bucket;
  }
  else { /* find it in bucket */
    bc = bucket;
    while (bc) {
      if (bc->bcid == bcid) {
	break;
      }
      bc = bc->next;
    }
  }

  if (bc == NULL) { /* not found in bucket */
    if (freebc) { /* try get one from freebc */
      bc = freebc;
      freebc = freebc->next;
      bc->next = bucket;
      bucket = bc;
    }
    else { /* ok, then we allocate one for it */
      bc = (struct bimsContext *)
	malloc(sizeof(struct bimsContext));
      memset(bc, 0, sizeof(struct bimsContext));
      bc->next = bucket;
      bucket = bc;
    }
  }

  bc->bcid = bcid;

#define AUTO_UPDATE
#ifdef AUTO_UPDATE
  bc->updatedb = 1;
#endif

  return(bc);
}

/*
 * reclaim the context for bims client
 */
void
bimsFreeBC(unsigned long int bcid)
{
  struct bimsContext *prev, *bc, *fbc;
  int i;

  prev = (struct bimsContext *)NULL;
  bc = bucket;

  while(bc) {
    if (bc->bcid == bcid) { /* locate the BC */
      fbc = bc;
      if (prev) {
	prev->next = fbc->next;
      }
      else {
	bucket = fbc->next;
      }
      /* put it onto freebc */
      fbc->next = freebc;
      freebc = fbc;

      /* clear the BC to initial state */
      fbc->yinlen = 0;
      if (fbc->yin) {
	free(fbc->yin);
      }
      fbc->yin = (Yin *)NULL;
      fbc->yinpos = 0;
      if (fbc->internal_text) {
	free(fbc->internal_text);
      }
      fbc->internal_text = (unsigned char *)NULL;
      if (fbc->pindown) {
	free(fbc->pindown);
      }
      fbc->pindown = (ZhiCode *)NULL;
      if (fbc->tsiboundary) {
	free(fbc->tsiboundary);
      }
      fbc->tsiboundary = (int*)NULL;
      fbc->state = BC_STATE_EDITING;
      fbc->bcid = 0;
      memset(&(fbc->zc), 0, sizeof(fbc->zc));
      if ((fbc->zsel).str) {
        free(*(fbc->zsel.str));
	free((fbc->zsel).str);
      }
      memset(&(fbc->zsel), 0, sizeof(fbc->zsel));
      if (fbc->ysinfo) {
        for (i = 0; i < fbc->num_ysinfo; i++) {
  	  if ((fbc->ysinfo[i]).yindata) {
	    free((fbc->ysinfo[i]).yindata);
	  }
        }
	free(fbc->ysinfo);
      }
      fbc->ysinfo = (struct YinSegInfo *)NULL;
    }
    prev = bc;
    bc = bc->next;
  }
}

#define TSI_UNION_RESULTS
/*
 * return 0 if the Tsi exist in either the primary database, or the
 * pool of database.
 *
 * The TsiInfo structure can be filled with the sum of all reference
 * counts from matching entries found in the databases.
 *
 * Note: the implication of only reporting sum of refcount, without
 *       union the corresponding yindata is unknown.
 */
static int
bimsTsiDBPoolSearch(struct TsiInfo *ti)
{
  int rval, i;
#ifdef TSI_UNION_RESULTS
  struct TsiInfo tmp;
#endif

  rval = -1;

  if (len_pool == 0 && tdb == NULL) {
    return (-1);
  }

  if (len_pool == 0) {
    rval = tdb->Get(tdb, ti);
    return (rval);
  }
  else {
#ifdef TSI_UNION_RESULTS
    /* initialize the data structure */
    memset(&tmp, 0, sizeof(tmp));
    tmp.refcount = -1;

    for (i = 0; i < len_pool; i++) {
      if (tdb_pool && tdb_pool[i]) {
	rval = (tdb_pool[i])->Get(tdb_pool[i], ti);
	if (rval == 0) {
	  if (tmp.refcount < 0) {
	    tmp.refcount = ti->refcount;
	  }
	  else {
	    tmp.refcount += ti->refcount;
	  }
	}
      }
    }
    if (tmp.refcount >= 0) {
      ti->refcount = tmp.refcount;
      return (0);
    }
    else {
      return (-1);
    }
#else
    for (i = 0; i < len_pool; i++) {
      if (tdb_pool && tdb_pool[i]) {
	rval = (tdb_pool[i])->Get(tdb_pool[i], ti);
	if (rval == 0) {
          return (rval);
	}
      }
    }
    return (-1);
#endif
  }
}

#define TSIYIN_UNION_RESULTS
/*
 * return 0 if the TsiYin exist in either the primary database, or the
 * pool of database.
 *
 * The TsiYinInfo structure can be filled with the union of all tsi's
 * from matching entries found in the databases.
 */
static int
bimsTsiYinDBPoolSearch(struct TsiYinInfo *ty)
{
  int rval, i;
#ifdef TSIYIN_UNION_RESULTS
  struct TsiYinInfo tmp;
  unsigned char *foo;
#endif

  if (len_pool == 0 && ydb == NULL) {
    return (-1);
  }

  if (len_pool == 0) {
    rval = ydb->Get(ydb, ty);
    return (rval);
  }
  else {
#ifdef TSIYIN_UNION_RESULTS
    /* initialize the data structure */
    memset(&tmp, 0, sizeof(tmp));
    tmp.yin = (Yin *)calloc(tmp.yinlen, sizeof(Yin));
    if (!tmp.yin) { return (-1); }
    memcpy(tmp.yin, ty->yin, tmp.yinlen*sizeof(Yin));
    tmp.yinlen = ty->yinlen;

    for (i = 0; i < len_pool; i++) {
      if (ydb_pool && ydb_pool[i]) {
	rval = (ydb_pool[i])->Get(ydb_pool[i], ty);
	if (rval == 0) {
	  /* got to combine the results */
	  foo = realloc(tmp.tsidata, (tmp.yinlen*2)*(tmp.tsinum+ty->tsinum));
	  if (!foo) { return (-1); }
	  memcpy(foo+(tmp.yinlen*2)*tmp.tsinum, ty->tsidata,
		 (tmp.yinlen*2)*ty->tsinum);
	  tmp.tsidata = foo;
	  tmp.tsinum += ty->tsinum;
	}
      }
    }
    if (tmp.tsinum > 0) {
      /* move the results over */
      if (ty->tsinum > 0) {
	free(ty->tsidata);
      }
      ty->tsinum = tmp.tsinum;
      ty->tsidata = tmp.tsidata;
      return (0);
    }
    else {
      return (-1);
    }
#else
    for (i = 0; i < len_pool; i++) {
      if (ydb_pool && ydb_pool[i]) {
	rval = (ydb_pool[i])->Get(ydb_pool[i], ty);
	if (rval == 0) {
	  return(rval);
	}
      }
    }
    return (-1);
#endif
  }
}

/*
 * return 0 if the ZuYinContext is valid,
 *       -1 if it's not valid yet.
 */
static int
bimsZuYinContextCheck(struct ZuYinContext *zc)
{
  Yin yin;
  ZhiStr str;

  /*
   * reference:
   *
   * the ZuYin encoding system used here conforms to
   * libtabe's ZuYin encoding system.
   */
  yin = 0;
  if (zc->index[0]) {
    yin |= (zc->index[0] << 9);
  }
  if (zc->index[1]) {
    yin |= ((zc->index[1]-21) << 7);
  }
  if (zc->index[2]) {
    yin |= ((zc->index[2]-24) << 3);
  }
  if (zc->index[3] > 38) {
    yin |= (zc->index[3]-37);
  }
  zc->yin = yin;

  str = tabeYinLookupZhiList(yin);
  if (str) {
    free(str);
    return(0);
  }
  else {
    return(-1);
  }
}

/*
 * a ZuYin input method finite state machine
 *
 * return 0 if it's in one of it's terminal state
 *       -1 if it's not.
 */
static int
bimsZuYinContextInput(struct ZuYinContext *zc, int index)
{
  int i, rval;
  ZhiStr str;

  if (index >  0 && index < 22) {
    zc->index[0] = index;
  }
  if (index > 21 && index < 25) {
    zc->index[1] = index;
  }
  if (index > 24 && index < 38) {
    zc->index[2] = index;
  }
  if (index > 37 && index < 43) {
    zc->index[3] = index;
  }

  memset(zc->string, 0, sizeof(zc->string));
  if (zc->index[0] || zc->index[1] || zc->index[2]) {
    for (i = 0; i < 4; i++) {
      str = tabeZuYinIndexToZuYinSymbol(zc->index[i]);
      if (str) {
	strcat((char *)zc->string, (char *)str);
      }
    }
  }

  rval = -1;
  if (zc->index[3]) {
    rval = bimsZuYinContextCheck(zc);
  }

  return(rval);
}

/*
 * reset the finite state machine
 */
static void
bimsZuYinContextClear(struct ZuYinContext *zc)
{
  memset(zc, 0, sizeof(struct ZuYinContext));
}

/*
 * given a yin, the function pick up the best character for that.
 *
 * return the Zhi if the yin is valid, return NULL otherwise.
 */
static Zhi
bimsYinChooseZhi(Yin yin)
{
  ZhiStr str, z;
  ZhiCode code;
  int len, i, idx;
  unsigned long int max, refcount;
  struct TsiInfo zhi;
  char zhi_buf[5];
  
  str = tabeYinLookupZhiList(yin);
  if (!str) {
    return(NULL);
  }
  len = strlen((char *)str)/2;
  max = 0;
  idx = 0;
  zhi.tsi = (ZhiStr)zhi_buf;
  zhi.refcount = 0;
  zhi.yinnum = 0;
  zhi.yindata = NULL;

  for (i = 0; i < len; i++) {
    code = tabeZhiToZhiCode(str+i*2);
    refcount = tabeZhiCodeLookupRefCount(code);
    if (refcount > max) {
      max = refcount;
      idx = i;
      zhi_buf[0] = str[i*2];
      zhi_buf[1] = str[i*2+1];
      zhi_buf[2] = '\0';
    }
  }

  z = (Zhi)malloc(sizeof(unsigned char)*3);
  tabeTsiInfoLookupZhiYin(tdb, &zhi);
  if (zhi.yinnum > 1)			/* zhi has more than one yin */
    strncpy((char *)z, (char *)str, 2);
  else
    strncpy((char *)z, (char *)str+idx*2, 2);
  z[2] = (unsigned char)NULL;
  if (zhi.yindata)
    free(zhi.yindata);

  return(z);
}

/*
 * verify if the pindown character is present
 */
static int
bimsVerifyPindown(struct bimsContext *bc, struct TsiYinInfo *ty, 
		  int yinoff, int index)
{
  int i, j;
  int found;
  unsigned char z[3];

  found = 0;
  for (j = 0; j < ty->yinlen; j++) {
    if (bc->pindown[yinoff+j] > 0) { /* pin down exists */
      found = 1;
    }
  }

  if (!found) { /* no pindown, no further investigation is required */
    return(0);
  }

  if (index < 0) {
    for (i = 0; i < ty->tsinum; i++) {
      for (j = 0; j < ty->yinlen; j++) {
	if (bc->pindown[yinoff+j] > 0) { /* pin down exists */
	  z[0] = bc->pindown[yinoff+j]/256;
	  z[1] = bc->pindown[yinoff+j]%256;
	  z[2] = (unsigned char)NULL;
	  if (strncmp((char *)z, (char *)ty->tsidata+((i*ty->yinlen)+j)*2, 2)) {
	    break;
	  }
	}
      }
      if (j == ty->yinlen) {
	return(0);
      }
    }
  }
  else {
    i = index;
    for (j = 0; j < ty->yinlen; j++) {
      if (bc->pindown[yinoff+j] > 0) { /* pin down exists */
	z[0] = bc->pindown[yinoff+j]/256;
	z[1] = bc->pindown[yinoff+j]%256;
	z[2] = (unsigned char)NULL;
	if (strncmp((char *)z, (char *)ty->tsidata+((i*ty->yinlen)+j)*2, 2)) {
	  break;
	}
      }
    }
    if (j == ty->yinlen) {
      return(0);
    }
  }

  return(-1);
}

/*
 * the internal structure for used within bimsContextDP
 */
struct smart_com {
  int    s1, s2, s3;
  int    len;           /* maximum matching */
  double avg_word_len;  /* largest average word length */
  double smallest_var;  /* smallest variance of word length */
  double largest_sum;   /* the sum of frequency */
};

/*
 * the most nasty algorithm on the world
 * how could one read this code?
 *
 * I can not remember why I called it bimsContextDP() instead of
 * other names.
 */
static int
bimsContextDP(struct bimsContext *bc)
{
  struct YinSegInfo *ysinfo = (struct YinSegInfo *)NULL;
  int num_ysinfo = 0;
  struct smart_com *comb = (struct smart_com *)NULL;
  int i, j, k, z, rval;
  int yinhead, len, ncomb = 0;
  int ncand, *cand;
  int *tmpcand, tmpncand;
  struct TsiYinInfo ty;
  struct TsiInfo tsi;
  int maxcount;
  int max_int, index;
  double max_double;
#define TMP_BUFFER 80 /* this should be far enough for this implementaion */
  Yin yin[TMP_BUFFER];
  unsigned char tmp[TMP_BUFFER];

  if (bc->yinlen == 0) {
    return(0);
  }

  len = bc->yinlen;
  yinhead = 0;
  memset(yin, 0, sizeof(Yin)*TMP_BUFFER);
  memset(&tsi, 0, sizeof(tsi));
  memset(tmp, 0, sizeof(unsigned char)*TMP_BUFFER);
  tsi.tsi = tmp;

  while (len > yinhead) {
    /* if it's a one-character word */
    if (len == yinhead + 1) {
      ysinfo = (struct YinSegInfo *)
	realloc(ysinfo, sizeof(struct YinSegInfo)*(num_ysinfo+1));
      ysinfo[num_ysinfo].yinoff = yinhead;
      ysinfo[num_ysinfo].yinlen = 1;
      ysinfo[num_ysinfo].yindata = (Yin *)malloc(sizeof(Yin));
      ysinfo[num_ysinfo].yindata[0] = bc->yin[yinhead+1];
      num_ysinfo++;
      break;
    }
    /* if it's a two-character word */
    if (len == yinhead + 2) {
      ysinfo = (struct YinSegInfo *)
	realloc(ysinfo, sizeof(struct YinSegInfo)*(num_ysinfo+1));
      ysinfo[num_ysinfo].yinoff = yinhead;
      ysinfo[num_ysinfo].yinlen = 2;
      ysinfo[num_ysinfo].yindata = (Yin *)malloc(sizeof(Yin)*2);
      memcpy(ysinfo[num_ysinfo].yindata, bc->yin+yinhead, sizeof(Yin)*2);
      num_ysinfo++;
      if(!bc->tsiboundary[yinhead+1]) {
	/* done duplicate a two-character word */
	memset(&ty, 0, sizeof(ty));
	ty.yinlen = 2;
	ty.yin = ysinfo[num_ysinfo-1].yindata;
	rval = bimsTsiYinDBPoolSearch(&ty);
	if (!rval) {
	  /* tsiyin exists, verify if it has pindown character */
 	  if (!bimsVerifyPindown(bc, &ty, yinhead, -1)) {
	    break;
	  }
	}
      }
      /* no such tsiyin, handle word by word */
      ysinfo = (struct YinSegInfo *)
	realloc(ysinfo, sizeof(struct YinSegInfo)*(num_ysinfo+1));
      ysinfo[num_ysinfo].yinoff = yinhead+1;
      ysinfo[num_ysinfo-1].yinlen = 1;
      ysinfo[num_ysinfo].yinlen = 1;
      ysinfo[num_ysinfo].yindata = (Yin *)malloc(sizeof(Yin)*2);
      ysinfo[num_ysinfo].yindata[0] = ysinfo[num_ysinfo-1].yindata[1];
      num_ysinfo++;
      break;
    }
    for (i = len-yinhead; i > 0; i--) {
      for (z = 1; z < i; z++) {
        if (bc->tsiboundary[yinhead+z]) {
          break;
        }
      }
      if (z != i) {
        continue;
      }
      memset(&ty, 0, sizeof(ty));
      ty.yinlen = i;
      memcpy(yin, bc->yin+yinhead, sizeof(Yin)*i);
      ty.yin = yin;
      rval = bimsTsiYinDBPoolSearch(&ty);
      if (rval < 0) {
	continue;
      }
      if (bimsVerifyPindown(bc, &ty, yinhead, -1)) {
	continue;
      }
      for (j = len-yinhead-i; j >= 0; j--) {
	if (j > 0) {
          for(z = 1; z < j; z++) {
            if(bc->tsiboundary[yinhead+i+z]) {
              break;
            }
          }
          if(z != j) {
            continue;
          }
	  memset(&ty, 0, sizeof(ty));
	  ty.yinlen = j;
	  memcpy(yin, bc->yin+yinhead+i, sizeof(Yin)*j);
	  ty.yin = yin;
	  rval = bimsTsiYinDBPoolSearch(&ty);
	  if (rval < 0) {
	    continue;
	  }
	  if (bimsVerifyPindown(bc, &ty, yinhead+i, -1)) {
	    continue;
	  }
	}
	for (k = len-yinhead-i-j; k >= 0; k--) {
	  if (k > 0 && j == 0) {
	    continue;
	  }
	  if (k > 0) {
            for(z = 1; z < k; z++) {
              if(bc->tsiboundary[yinhead+i+j+z]) {
                break;
              }
            }
	    if(z != k) {
	      continue;
	    }
	    memset(&ty, 0, sizeof(ty));
	    ty.yinlen = k;
	    memcpy(yin, bc->yin+yinhead+i+j, sizeof(Yin)*k);
	    ty.yin = yin;
	    rval = bimsTsiYinDBPoolSearch(&ty);
	    if (rval < 0) {
	      continue;
	    }
	    if (bimsVerifyPindown(bc, &ty, yinhead+i+j, -1)) {
	      continue;
	    }
	  }
	  comb = (struct smart_com *)
	    realloc(comb, sizeof(struct smart_com)*(ncomb+1));
	  comb[ncomb].s1 = yinhead;
	  comb[ncomb].s2 = yinhead+i;
	  comb[ncomb].s3 = yinhead+i+j;
	  comb[ncomb].len = i+j+k;
	  comb[ncomb].avg_word_len = 0;
	  comb[ncomb].smallest_var = 0;
	  comb[ncomb].largest_sum = 0;
	  ncomb++;
	}
      }
    }

    /* rule 1: largest sum of three-tsi */
    max_int = 0;
    index = 0;
    maxcount = 0;
    for (i = 0; i < ncomb; i++) {
      if (comb[i].len > max_int) {
	index = i;
	max_int = comb[i].len;
	maxcount = 1;
      }
      else if (comb[i].len == max_int) {
        maxcount++;
      }
    }
    ncand = 0;
    cand = (int *)malloc(sizeof(int)*maxcount);
    for (i = 0; i < ncomb; i++) {
      if (comb[i].len == max_int) {
	cand[ncand] = i;
	ncand++;
      }
    }

    /* resolved by rule 1 */
    if (ncand == 1) {
      index = cand[0];
    }
    else { /* ambiguity */
      /* rule 2: largest average word length */
      max_double = 0;
      maxcount = 0;
      for (i = 0; i < ncand; i++) {
	index = cand[i];
	comb[index].avg_word_len = 0;
	j = 0;
	k = comb[index].s2 - comb[index].s1;
	if (k > 0) {
	  j++;
	  comb[index].avg_word_len += k;
	}
	k = comb[index].s3 - comb[index].s2;
	if (k > 0) {
	  j++;
	  comb[index].avg_word_len += k;
	}
	k = (comb[index].len + comb[index].s1) - comb[index].s3;
	if (k > 0) {
	  j++;
	  comb[index].avg_word_len += k;
	}

	comb[index].avg_word_len /= j;
	if (comb[index].avg_word_len > max_double) {
	  max_double = comb[index].avg_word_len;
	  maxcount = 1;
	}
	else if (comb[index].avg_word_len == max_double) {
	  maxcount++;
	}
      }

      tmpncand = 0;
      tmpcand = (int *)malloc(sizeof(int)*maxcount);
      for (i = 0; i < ncand; i++) {
	index = cand[i];
	if (comb[index].avg_word_len == max_double) {
	  tmpcand[tmpncand] = index;
	  tmpncand++;
	}
      }

      ncand = tmpncand;
      free(cand);
      cand = tmpcand;
      tmpcand = (int *)NULL;

      /* resolved by rule 2 */
      if (ncand == 1) {
	index = cand[0];
      }
      else { /* ambiguity */
	/* rule 3: smallest variance of word length */
	max_double = 1000; /* this is misleading */
	maxcount = 0;
	for (i = 0; i < ncand; i++) {
	  index = cand[i];
	  comb[index].smallest_var = 0;
	  j = 0;
	  k = (comb[index].s3-comb[index].s2) -
	    (comb[index].s2-comb[index].s1);
	  comb[index].smallest_var += abs(k);
	  k = (comb[index].len+comb[index].s1-comb[index].s3) -
	    (comb[index].s3-comb[index].s2);
	  comb[index].smallest_var += abs(k);
	  k = (comb[index].len+comb[index].s1-comb[index].s3) -
	    (comb[index].s2-comb[index].s1);
	  comb[index].smallest_var += abs(k);
	  
	  comb[index].smallest_var /= 3;
	  if (comb[index].smallest_var < max_double) {
	    max_double = comb[index].smallest_var;
	    maxcount = 1;
	  }
	  else if (comb[index].smallest_var == max_double) {
	    maxcount++;
	  }
	}
	
	tmpncand = 0;
	tmpcand = (int *)malloc(sizeof(int)*maxcount);
	for (i = 0; i < ncand; i++) {
	  index = cand[i];
	  if (comb[index].smallest_var == max_double) {
	    tmpcand[tmpncand] = index;
	    tmpncand++;
	  }
	}
	
	ncand = tmpncand;
	free(cand);
	cand = tmpcand;
	tmpcand = (int *)NULL;
	
	/* resolved by rule 3 */
	if (ncand == 1) {
	  index = cand[0];
	}
	else { /* ambiguity */
	  int max_ref;
	  /* rule 4: largest sum of tsi ref count */
	  max_double = 0;
	  maxcount = 0;
	  for (i = 0; i < ncand; i++) {
	    index = cand[i];
	    comb[index].largest_sum = 0;
	    k = (comb[index].s2-comb[index].s1);
	    if (k > 1) {
	      memset(&ty, 0, sizeof(ty));
	      ty.yinlen = k;
	      memcpy(yin, bc->yin+comb[index].s1, sizeof(Yin)*k);
	      ty.yin = yin;
	      rval = bimsTsiYinDBPoolSearch(&ty);
	      if (!rval) {
		max_ref = 0;
		tsi.tsi[ty.yinlen*2] = (unsigned char)NULL;
		for (j = 0; j < ty.tsinum; j++) {
		  strncpy((char *)tsi.tsi, 
			  (char *)ty.tsidata+(j*ty.yinlen)*2, ty.yinlen*2);
		  rval = bimsTsiDBPoolSearch(&tsi);
		  if (tsi.refcount > max_ref) {
		    max_ref = tsi.refcount;
		  }
		}
		comb[index].largest_sum += max_ref;
	      }
	    }
	    k = (comb[index].s3-comb[index].s2);
	    if (k > 1) {
	      memset(&ty, 0, sizeof(ty));
	      ty.yinlen = k;
	      memcpy(yin, bc->yin+comb[index].s2, sizeof(Yin)*k);
	      ty.yin = yin;
	      rval = bimsTsiYinDBPoolSearch(&ty);
	      if (!rval) {
		max_ref = 0;
		tsi.tsi[ty.yinlen*2] = (unsigned char)NULL;
		for (j = 0; j < ty.tsinum; j++) {
		  strncpy((char *)tsi.tsi, 
			  (char *)ty.tsidata+(j*ty.yinlen)*2, ty.yinlen*2);
		  rval = bimsTsiDBPoolSearch(&tsi);
		  if (tsi.refcount > max_ref) {
		    max_ref = tsi.refcount;
		  }
		}
		comb[index].largest_sum += max_ref;
	      }
	    }
	    k = (comb[index].len+comb[index].s1-comb[index].s3);
	    if (k > 1) {
	      memset(&ty, 0, sizeof(ty));
	      ty.yinlen = k;
	      memcpy(yin, bc->yin+comb[index].s3, sizeof(Yin)*k);
	      ty.yin = yin;
	      rval = bimsTsiYinDBPoolSearch(&ty);
	      if (!rval) {
		max_ref = 0;
		tsi.tsi[ty.yinlen*2] = (unsigned char)NULL;
		for (j = 0; j < ty.tsinum; j++) {
		  strncpy((char *)tsi.tsi, 
			  (char *)ty.tsidata+(j*ty.yinlen)*2, ty.yinlen*2);
		  rval = bimsTsiDBPoolSearch(&tsi);
		  if (tsi.refcount > max_ref) {
		    max_ref = tsi.refcount;
		  }
		}
		comb[index].largest_sum += max_ref;
	      }
	    }

	    if (comb[index].largest_sum > max_double) {
	      max_double = comb[index].largest_sum;
	      maxcount = 1;
	    }
	    else if (comb[index].largest_sum == max_double) {
	      maxcount++;
	    }
	  }
	  
	  tmpncand = 0;
	  tmpcand = (int *)malloc(sizeof(int)*maxcount);
	  for (i = 0; i < ncand; i++) {
	    index = cand[i];
	    if (comb[index].largest_sum == max_double) {
	      tmpcand[tmpncand] = index;
	      tmpncand++;
	    }
	  }
	  
	  ncand = tmpncand;
	  free(cand);
	  cand = tmpcand;
	  tmpcand = (int *)NULL;
	  
	  if (ncand == 1) {
	    index = cand[0];
	  }
	  else {
            ysinfo = (struct YinSegInfo *)
              realloc(ysinfo, sizeof(struct YinSegInfo)*(num_ysinfo+1));
            ysinfo[num_ysinfo].yinoff = yinhead;
            ysinfo[num_ysinfo].yinlen = 1;
            ysinfo[num_ysinfo].yindata = (Yin *)malloc(sizeof(Yin));
            ysinfo[num_ysinfo].yindata[0] = bc->yin[yinhead+1];
            num_ysinfo++;
	    yinhead++;
	    continue;
	  }
	}
      }
    }

    ysinfo = (struct YinSegInfo *)
      realloc(ysinfo, sizeof(struct YinSegInfo)*(num_ysinfo+1));
    ysinfo[num_ysinfo].yinoff = yinhead;
    ysinfo[num_ysinfo].yinlen = comb[index].s2-comb[index].s1;
    ysinfo[num_ysinfo].yindata =
      (Yin *)malloc(sizeof(Yin)*(comb[index].s2-comb[index].s1));
    memcpy(ysinfo[num_ysinfo].yindata, bc->yin+yinhead,
	   sizeof(Yin)*(comb[index].s2-comb[index].s1));
    num_ysinfo++;

    yinhead += comb[index].s2 - comb[index].s1;

    free(comb);
    comb = (struct smart_com *)NULL;
    ncomb = 0;
    free(cand);
  }

  bc->ysinfo = ysinfo;
  bc->num_ysinfo = num_ysinfo;
  return(num_ysinfo);
}

/*
 * this function decides which zhi among available selection is the best
 */
static void
bimsContextSmartEdit(struct bimsContext *bc)
{
  struct YinSegInfo *ysinfo;
  struct TsiYinInfo ty;
  struct TsiInfo tsi;
  int num_ysinfo, ref_index;
  unsigned long int max_ref;
  int i, j, rval;
  Zhi z;
#define TMP_BUFFER 80
  unsigned char tmp[TMP_BUFFER];
  int idebug = 0;

  if (bc->no_smart_ed || (!tdb || !ydb)) {
    return;
  }

  if (bc->ysinfo) {
    for (i = 0; i < bc->num_ysinfo; i++) {
      if ((bc->ysinfo[i]).yindata) {
        free((bc->ysinfo[i]).yindata);
      }
    }
    free(bc->ysinfo);
  }
  bc->num_ysinfo = 0;
  bc->ysinfo = (struct YinSegInfo *)NULL;

  num_ysinfo = bimsContextDP(bc);
  ysinfo = bc->ysinfo;

  if (bc->internal_text) {
    free(bc->internal_text);
  }
  bc->internal_text = (unsigned char *)
    malloc(sizeof(unsigned char)*(bc->yinlen*2+1));
  memset(bc->internal_text, 0, sizeof(unsigned char)*(bc->yinlen*2+1));
  memset(&tsi, 0, sizeof(tsi));
  memset(tmp, 0, TMP_BUFFER);
  tsi.tsi = tmp;

  for (i = 0; i < num_ysinfo; i++) {
    if (idebug) {
      printf("%d (%d %d) ", i, ysinfo[i].yinoff, ysinfo[i].yinlen);
    }
    if (ysinfo[i].yinlen == 1) {
      if (bc->pindown[bc->ysinfo[i].yinoff] > 0) {
	bc->internal_text[2*ysinfo[i].yinoff] =
	  bc->pindown[ysinfo[i].yinoff] / 256;
	bc->internal_text[2*ysinfo[i].yinoff+1] =
	  bc->pindown[ysinfo[i].yinoff] % 256;
      }
      else {
	z = bimsYinChooseZhi(bc->yin[ysinfo[i].yinoff]);
	strncpy((char *)bc->internal_text+2*ysinfo[i].yinoff, (char *)z, 2);
	free(z);
      }
    }
    else {
      memset(&ty, 0, sizeof(ty));
      ty.yinlen = ysinfo[i].yinlen;
      ty.yin = ysinfo[i].yindata;
      rval = bimsTsiYinDBPoolSearch(&ty);
      if (rval < 0) { /* weird */
	fprintf(stderr, "Weird I!\n");
      }
      else {
	max_ref = 0;
        ref_index = 0;

	tsi.tsi[ty.yinlen*2] = (unsigned char)NULL;	
	for (j = 0; j < ty.tsinum; j++) { /* find one with higest ref count */
	  if (bimsVerifyPindown(bc, &ty, ysinfo[i].yinoff, j)) {
	    continue;
	  }
	  strncpy((char *)tsi.tsi, 
		  (char *)ty.tsidata+(j*ty.yinlen)*2, ty.yinlen*2);
	  rval = bimsTsiDBPoolSearch(&tsi);
	  if (rval < 0) { /* weird, too */
	    fprintf(stderr, "Weird II!\n");
	    continue;
	  }
	  else {
            if (idebug) {
	      printf("[%s %ld] ", tsi.tsi, tsi.refcount);
            }
	    if (tsi.refcount >= max_ref) {
	      ref_index = j;
	      max_ref = tsi.refcount;
	    }
	  }
	}

	strncpy((char *)bc->internal_text+2*ysinfo[i].yinoff,
		(char *)ty.tsidata+(ref_index*ty.yinlen)*2, ty.yinlen*2);
      }
    }
  }
  if (idebug) {
    printf("\n");
  }
}

/*
 * without this, the bims is useless
 *
 * KEY          ACTION
 * XK_Escape    clear ZuYinContext
 * XK_Left      move the internal cursor one zhi left
 * XK_Right     move the internal cursor one zhi right
 * XK_Backspace
 * XK_Delete    delete the zhi in front of the internal cursor
 * XK_Tab       switch the tsi boundary
 * XK_Return    does nothing so far, client may request for string
 * others       depends on the key mapping the client uses
 *
 *
 * the return value and the required action on the client side
 *
 * BC_VAL_ABSORB     do nothing, bims have done all
 * BC_VAL_IGNORE     bims does not process the key at all
 * BC_VAL_COMMIT     bims internal buffer overflow
 * BC_VAL_ERROR      error occurs
 */
int
bimsFeedKey(unsigned long int bcid, KeySym key)
{
  struct bimsContext *bc;
  int index, rval;
  int i;

  /* bc will always return */
  bc = bimsGetBC(bcid);

  switch(key) {
  case XK_Escape:
    if (bc->zc.index[0] || bc->zc.index[1] || bc->zc.index[2]) {
      bimsZuYinContextClear(&(bc->zc));
      return(BC_VAL_ABSORB);
    }
    return(BC_VAL_IGNORE);
  case XK_Home:
    if (bc->yinpos > 0) {
      bc->yinpos=0;
      return(BC_VAL_ABSORB);
    }
    return(BC_VAL_IGNORE);
  case XK_Left:
    if (bc->yinpos > 0) {
      bc->yinpos--;
      return(BC_VAL_ABSORB);
    }
    return(BC_VAL_IGNORE);
  case XK_End:
    if (bc->yinpos < bc->yinlen) {
      bc->yinpos=bc->yinlen;
      return(BC_VAL_ABSORB);
    }
    return(BC_VAL_IGNORE);
  case XK_Right:
    if (bc->yinpos < bc->yinlen) {
      bc->yinpos++;
      return(BC_VAL_ABSORB);
    }
    return(BC_VAL_IGNORE);
  case XK_BackSpace:
  case XK_Delete:
    if (strlen((char *)bc->zc.string) > 0) { /* modifying ZuYin FSM */
      for (i = 3; i >= 0; i--) {
	if (bc->zc.index[i]) {
	  bc->zc.index[i] = 0;
	  break;
	}
      }
      bimsZuYinContextInput(&(bc->zc), 0);
      return(BC_VAL_ABSORB);
    }
    else { /* delete editing buffer */
      if (bc->yinlen > 0) {
        if ((key==XK_BackSpace && bc->yinpos==0) || 
            (key==XK_Delete && bc->yinpos==bc->yinlen)) return(BC_VAL_IGNORE);
        if (key==XK_Delete) bc->yinpos++;
  	if (bc->yinpos < bc->yinlen) { /* not the last character */

	  memmove(bc->yin+(bc->yinpos-1), bc->yin+(bc->yinpos),
		  sizeof(Yin)*(bc->yinlen-(bc->yinpos-1)));
	  memmove(bc->internal_text+(bc->yinpos-1)*2,
		  bc->internal_text+(bc->yinpos)*2,
		  sizeof(unsigned char)*2*(bc->yinlen-(bc->yinpos-1)));
	  memmove(bc->pindown+(bc->yinpos-1),
		  bc->pindown+(bc->yinpos),
		  sizeof(ZhiCode)*(bc->yinlen-(bc->yinpos-1)));
	  memmove(bc->tsiboundary+(bc->yinpos-1),
	  	  bc->tsiboundary+(bc->yinpos),
	  	  sizeof(int)*(bc->yinlen-(bc->yinpos-1)));
	}
	else { /* the last character */
	  bc->internal_text[(bc->yinlen-1)*2] = (unsigned char)NULL;
	}
	bc->yinlen -= 1;
	bc->yinpos -= 1;
        bimsContextSmartEdit(bc);
        return(BC_VAL_ABSORB);
      }
    }
    return(BC_VAL_IGNORE);
  case XK_Tab:
    if(strlen((char *)bc->zc.string) == 0 && bc->yinlen >0 &&
       bc->yinlen!=bc->yinpos) {
      bc->tsiboundary[bc->yinpos] = (! bc->tsiboundary[bc->yinpos]);
      bimsContextSmartEdit(bc);
      return(BC_VAL_ABSORB);
    }
    return(BC_VAL_IGNORE);
  case XK_Return:
    return(BC_VAL_ABSORB);
  default:
    rval = -1;
    switch (bc->keymap) {
    case BC_KEYMAP_ZO:
    case BC_KEYMAP_ETEN:
      if (bc->keymap == BC_KEYMAP_ZO) {
	index = bimsZozyKeyToZuYinIndex(key);
      }
      else { /* BC_KEYMAP_ETEN */
	index = bimsEtenKeyToZuYinIndex(key);
      }
      if (index) {
	rval = bimsZuYinContextInput(&(bc->zc), index);
      }
      else {
        return(BC_VAL_IGNORE);
      }
      break;
    case BC_KEYMAP_ETEN26:
      index = bimsEten26KeyToZuYinIndex(key);
      if (index) {
	rval = bimsEten26ZuYinContextInput(&(bc->zc), index);
      }
      else {
        return(BC_VAL_IGNORE);
      }
      break;
    case BC_KEYMAP_HSU:
      index = bimsHsuKeyToZuYinIndex(key);
      if (index) {
	rval = bimsHsuZuYinContextInput(&(bc->zc), index);
      }
      else if (key=='q') {
        char *buf;
        int j;
        ZhiCode z;

        i=bc->yinpos;
        if (i == 0)
	  return(BC_VAL_ABSORB);
	else if (i==bc->yinlen) 
	  i--;
	buf=(char *)tabeYinLookupZhiList(bc->yin[i]);
	if (!bc->pindown[i])
	  bc->pindown[i]=tabeZhiToZhiCode(bc->internal_text+i*2);
	for (j=0; j<3 && buf[j*2]; j++) {
	  z=tabeZhiToZhiCode((Zhi)buf+j*2);
	  if (z==bc->pindown[i]) break;
	}
	if (buf[j*2]) j++;
	if (j>=3 || !buf[j*2]) j=0;
	bc->pindown[i]=tabeZhiToZhiCode((Zhi)buf+j*2);
	free(buf);
        bimsContextSmartEdit(bc);
        return(BC_VAL_ABSORB);
      } 
      else 
	return(BC_VAL_IGNORE);
      break;
    default:
      return(BC_VAL_IGNORE);
    }
    if (!rval) { /* we get a character */
      bc->yin = (Yin *)realloc(bc->yin, sizeof(Yin)*(bc->yinlen+1));
      memmove(bc->yin+(bc->yinpos+1), bc->yin+(bc->yinpos),
	      sizeof(Yin)*(bc->yinlen-bc->yinpos));
      bc->yin[bc->yinpos] = bc->zc.yin;
      bc->pindown = (ZhiCode *)realloc(bc->pindown,
				       sizeof(ZhiCode)*(bc->yinlen+1));
      memmove(bc->pindown+(bc->yinpos+1), bc->pindown+(bc->yinpos),
	      sizeof(ZhiCode)*(bc->yinlen-bc->yinpos));
      bc->pindown[bc->yinpos] = 0;
      bc->tsiboundary = (int *)realloc(bc->tsiboundary,
				       sizeof(int)*(bc->yinlen+1));
      memmove(bc->tsiboundary+(bc->yinpos+1), bc->tsiboundary+(bc->yinpos),
	      sizeof(int)*(bc->yinlen-bc->yinpos));
      bc->tsiboundary[bc->yinpos] = 0;
      bc->yinlen++;
      bc->yinpos++;
      bimsZuYinContextClear(&(bc->zc));
      bimsContextSmartEdit(bc);
    }
    if (bc->maxlen && bc->yinlen > bc->maxlen) {
      return(BC_VAL_COMMIT);
    }
    else {
      return(BC_VAL_ABSORB);
    }
    return(BC_VAL_IGNORE);
  }

  return(BC_VAL_IGNORE);
}


/*
 * toggle into tsi selection mode
 */
int
bimsToggleTsiSelection(unsigned long int bcid)
{
  struct bimsContext *bc;
  struct TsiYinInfo ty;
  unsigned char **list=NULL, *str;
  int i, j, yl, rval, len=0, num=0, oldlen=0;

  bc = bimsGetBC(bcid);
  if (bc->no_smart_ed || (!tdb || !ydb)) {
    return(BC_VAL_IGNORE);
  }
  if (bc->yinlen == 0 || bc->yinpos > bc->yinlen) {
    return(BC_VAL_IGNORE);
  }

  if (bc->zsel.str) {
    free(*(bc->zsel.str));
    free(bc->zsel.str);
  }
  memset(&(bc->zsel), 0, sizeof(bc->zsel));
  bc->zsel.str = NULL;
  
  i = bc->yinpos;
  if (i == bc->yinlen && i > 0) i--;

  ty.tsidata = 0;
  for (yl = 2; yl < 5 && i+yl <= bc->yinlen; yl++) {
    ty.yinlen = yl;
    ty.yin = &bc->yin[i];
    rval = bimsTsiYinDBPoolSearch(&ty);
    if (rval) continue;
    if (ty.tsinum) {
      num += ty.tsinum;
      if (list) {
        list = realloc(list, (num+1)*sizeof(char*));
        oldlen = len;
        len += (ty.tsinum*(yl*2+1));
        list[0] = realloc(list[0], len);
      } else {
        list = malloc((num+1)*sizeof(char*));
        len=ty.tsinum*(yl*2+1);
        list[0] = malloc(len);
      }
      for (j=0; j < ty.tsinum; j++)
      {
        memcpy(list[0]+oldlen, ty.tsidata+(j*yl*2), yl*2);
        oldlen+=(yl*2);
        list[0][oldlen++]=0;
      }
    }
  }
  if (list) {
    str=list[0];
    for (j = 0; j < num; str++) {
      if (*str == 0) list[++j] = str+1;
    }
    bc->zsel.str = list;
    bc->zsel.num = num;
    bc->zsel.base = 0;
    bc->state = BC_STATE_SELECTION_TSI;
    return(0);
  }    

  return(BC_VAL_IGNORE);
}


/*
 * toggle into zhi selection mode
 */
int
bimsToggleZhiSelection(unsigned long int bcid)
{
  struct bimsContext *bc;
  int i,len;
  unsigned char *str,**list;

  bc = bimsGetBC(bcid);
  if (bc->yinlen == 0 || bc->yinpos > bc->yinlen) {
    return(BC_VAL_IGNORE);
  }

  if (bc->zsel.str) {
    free(*(bc->zsel.str));
    free(bc->zsel.str);
  }
  memset(&(bc->zsel), 0, sizeof(bc->zsel));
  bc->zsel.str = NULL;
  
  i = bc->yinpos;
  if (i == bc->yinlen && i > 0) i--;

  /* Get zhi list */
  str = tabeYinLookupZhiList(bc->yin[i]);
  len = strlen((char *)str)/2;
  list = malloc(sizeof(char*)*(len+1));
  if (len) {
    list[0] = malloc(3*len);
    for (i = 0; i < len; i++) {
      list[i][0] = str[i*2];
      list[i][1] = str[i*2+1];
      list[i][2] = 0;
      list[i+1] = list[i]+3;
    }
    list[i] = NULL;
  } else {
    list[0] = NULL;
  }
  free(str);
  bc->zsel.str = list;
  bc->zsel.num = len;
  bc->zsel.base = 0;
  bc->state = BC_STATE_SELECTION_ZHI;

  return(0);
}

/*
 * toggle into editing mode
 */
int
bimsToggleEditing(unsigned long int bcid)
{
  struct bimsContext *bc;

  bc = bimsGetBC(bcid);
  bc->state = BC_STATE_EDITING;
  if (bc->zsel.str) {
    free(*(bc->zsel.str));
    free(bc->zsel.str);
  }
  memset(&(bc->zsel), 0, sizeof(bc->zsel));
  bc->zsel.str = NULL;

  return(0);
}

/*
 * toggle into smart editing mode
 */
int
bimsToggleSmartEditing(unsigned long int bcid)
{
  struct bimsContext *bc;

  bc = bimsGetBC(bcid);
  bc->no_smart_ed = 0;
  return(0);
}

/*
 * toggle into no smart editing mode
 */
int
bimsToggleNoSmartEditing(unsigned long int bcid)
{
  struct bimsContext *bc;

  bc = bimsGetBC(bcid);
  bc->no_smart_ed = 1;
  return(0);
}

/*
 * toggle into db update mode
 */
int
bimsToggleUpdate(unsigned long int bcid)
{
  struct bimsContext *bc;

  bc = bimsGetBC(bcid);
  bc->updatedb = 1;
  return(0);
}

/*
 * toggle into no db update mode
 */
int
bimsToggleNoUpdate(unsigned long int bcid)
{
  struct bimsContext *bc;

  bc = bimsGetBC(bcid);
  bc->updatedb = 0;
  return(0);
}

/*
 * pindown a Zhi
 */
int
bimsPindown(unsigned long int bcid, ZhiCode z)
{
  struct bimsContext *bc;
  int i;

  bc = bimsGetBC(bcid);
  i = bc->yinpos;
  if (i == bc->yinlen && i > 0) i--;
  bc->pindown[i] = z;
  bimsContextSmartEdit(bc);

  return(0);
}

/*
 * pindown a Zhi by number
 */
int
bimsPindownByNumber(unsigned long int bcid, int sel)
{
  struct bimsContext *bc;
  int i;
  unsigned char *str;

  bc = bimsGetBC(bcid);
  i = bc->yinpos;
  if (i == bc->yinlen && i > 0) i--;
  str = bc->zsel.str[sel];
  for (;*str;) {
    bc->pindown[i] =
      (*str)*256 + *(str+1);
    bc->tsiboundary[i] = 0;
    i++;
    str += 2;
  }
  if (i != bc->yinlen)
    bc->tsiboundary[i] = 1;
  if (bc->yinpos != 0) {
    if (bc->yinpos == bc->yinlen)
       bc->tsiboundary[bc->yinpos-1] = 1;
    else
       bc->tsiboundary[bc->yinpos] = 1;
  }
  bimsContextSmartEdit(bc);

  return(0);
}

/*
 * set the selection base
 */
int
bimsSetSelectionBase(unsigned long int bcid, int base)
{
  struct bimsContext *bc;

  bc = bimsGetBC(bcid);
  bc->zsel.base = base;

  return(0);
}

/*
 * set the maxlen
 */
int
bimsSetMaxLen(unsigned long int bcid, int maxlen)
{
  struct bimsContext *bc;

  bc = bimsGetBC(bcid);
  bc->maxlen = maxlen;

  return(0);
}

/*
 * fetch some text from head
 */
unsigned char *
bimsFetchText(unsigned long int bcid, int len)
{
  struct bimsContext *bc;
  unsigned char *str;
  int newlen, i, j, rval;
  struct TsiInfo ti;
  struct TsiYinInfo yi;

  bc = bimsGetBC(bcid);
  newlen = (bc->yinlen < len) ? bc->yinlen : len;

  /* see if we need to/can update DBs */
  if (bc->updatedb &&
      !(tdb->flags & DB_FLAG_READONLY) && !(ydb->flags & DB_FLAG_READONLY)) {
    int ylen, yoff;

    for (i = 0; i < bc->num_ysinfo; i++) {
      memset(&ti, 0, sizeof(ti));
      memset(&yi, 0, sizeof(yi));

      ylen = bc->ysinfo[i].yinlen;
      yoff = bc->ysinfo[i].yinoff;
      /*
       * update Tsi DB
       */
      if (ti.tsi) { free(ti.tsi); }
      ti.tsi = (ZhiStr)calloc(ylen*2+1, sizeof(char));
      if (!ti.tsi) { break; }
      strncpy((char *)ti.tsi, (char *)bc->internal_text+(yoff*2), ylen*2);
      rval = tdb->Get(tdb, &ti);
      if (rval == 0) {
	for (j = 0; j < ti.yinnum; j++) {
	  if (!memcmp(ti.yindata+(j*ylen), bc->yin+yoff, ylen*sizeof(Yin))) {
	    /* found a match yin in tsi db*/
	    break;
	  }
	}

	/* yin not found in tsi db */
	if (j >= ti.yinnum) {
	  /* not found, add this new one */
	  ti.yindata=(Yin*)realloc(ti.yindata,(ti.yinnum+1)*ylen*sizeof(Yin));
	  memcpy(ti.yindata+(ti.yinnum)*ylen*sizeof(Yin),
		 bc->yin+yoff, ylen*sizeof(Yin));
	  ti.yinnum += 1;

	  /* update yin DB here */
	  yi.yin = (Yin *)calloc(ylen, sizeof(Yin));
	  if (yi.yin) {
	    memcpy(yi.yin, bc->yin+yoff, ylen*sizeof(Yin));
	    yi.yinlen = ylen;
	    rval = ydb->Get(ydb, &yi);
	    if (rval == 0) {
	      /* see if the tsi is already in the yin db */
	      for (j = 0; j < yi.tsinum; j++) {
		if (!memcmp(yi.tsidata+j*ylen*2, ti.tsi, ylen*2)) {
		  /* found a match tsi in yin db */
		  break;
		}
	      }

	      if (j >= yi.tsinum) {
		/* tsi not found in yin db, rare case */
		yi.tsidata = (ZhiStr)realloc(yi.tsidata,
					     (yi.tsinum+1)*ylen*2+1);
		memcpy(yi.tsidata+ylen*2, ti.tsi, ylen*2);
		yi.tsinum += 1;
		ydb->Put(ydb, &yi);
	      }
	      else {
		/* tsi found in yin db */
		/* do nothing */
	      }
	    }
	    else {
	      yi.tsidata = (ZhiStr)calloc(ylen*2+1, sizeof(char));
	      memcpy(yi.tsidata, ti.tsi, ylen*2*sizeof(char));
	      yi.yinlen = ylen;
	      yi.tsinum = 1;
	      ydb->Put(ydb, &yi);
	    }
	  }
	}
	else {
	  /* yin found in tsi db, update refcount */
	  ti.refcount += 1;
	}
      }
      else {
	/* tsi not found in tsi db, add this one */
	ti.yindata = (Yin *)calloc(ylen, sizeof(Yin));
	memcpy(ti.yindata, bc->yin+yoff, ylen*sizeof(Yin));
	ti.yinnum = 1;
	ti.refcount = 1;
      }
      /* don't care whether it's success or not */
      rval = tdb->Put(tdb, &ti);
      free(ti.tsi);
      free(ti.yindata);
    }
  }

  str = (unsigned char *)malloc(sizeof(unsigned char)*(newlen*2+1));
  strncpy((char *)str, (char *)bc->internal_text, newlen*2);
  str[newlen*2] = (unsigned char)NULL;

  memmove(bc->yin, bc->yin+newlen, sizeof(Yin)*(bc->yinlen-newlen));
  bc->yinpos = bc->yinpos > newlen ? bc->yinpos - newlen : 0;
  memmove(bc->internal_text, bc->internal_text+newlen*2,
	  sizeof(unsigned char)*((bc->yinlen-newlen)*2+1));
  memmove(bc->pindown, bc->pindown+len, sizeof(ZhiCode)*(bc->yinlen-newlen));
  memmove(bc->tsiboundary, bc->tsiboundary+len,
	  sizeof(int)*(bc->yinlen-newlen));
  bc->yinlen -= newlen;

  bimsContextSmartEdit(bc);

  return(str);
}

int
bimsSetKeyMap(unsigned long int bcid, int keymap)
{
  struct bimsContext *bc;

  if (keymap < 0 || keymap >= BC_KEYMAP_LAST) {
    return(-1);
  }
  bc = bimsGetBC(bcid);
  bc->keymap = keymap;

  return(0);
}

/*
 * return the state of the bc
 */
int
bimsQueryState(unsigned long int bcid)
{
  struct bimsContext *bc;

  bc = bimsGetBC(bcid);
  return(bc->state);
}

/*
 * return the position of the respecting bc
 */
int
bimsQueryPos(unsigned long int bcid)
{
  struct bimsContext *bc;
  int yinpos;

  bc = bimsGetBC(bcid);
  yinpos = bc->yinpos;

  return(yinpos);
}

/*
 * returns the yin segmentation(grouping) of current input.
 */
int *
bimsQueryYinSeg(unsigned long int bcid)
{
  struct bimsContext *bc;
  int *data;
  int i;

  bc = bimsGetBC(bcid);
  data = malloc(sizeof(int) * (bc->num_ysinfo + 1));
  data[0] = bc->num_ysinfo;
  for (i = 1; i <= bc->num_ysinfo; i++)
  {
    data[i] = bc->ysinfo[i-1].yinlen;
  }
  return data;
}

/*
 * return the internal string of the respecting bc
 */
unsigned char *
bimsQueryInternalText(unsigned long int bcid)
{
  struct bimsContext *bc;
  unsigned char *str;

  bc = bimsGetBC(bcid);
  if (!bc->internal_text) {
    str = (unsigned char *)malloc(sizeof(unsigned char));
    str[0] = (unsigned char)NULL;
    return(str);
  }
  else {
    str = (unsigned char *)strdup(bc->internal_text);
    return(str);
  }
}

/*
 * return the internal ZuYin string of the respecting bc
 */
unsigned char *
bimsQueryZuYinString(unsigned long int bcid)
{
  struct bimsContext *bc;
  unsigned char *str;

  bc = bimsGetBC(bcid);
  str = (unsigned char *)strdup(bc->zc.string);

  return(str);
}

/*
 * return the last composed ZuYin string of the respecting bc
 */
unsigned char *
bimsQueryLastZuYinString(unsigned long int bcid)
{
  struct bimsContext *bc;
  unsigned char *zu_yin;
  Yin yin;
  unsigned int index[4], i;
  unsigned char *s = NULL;

  bc = bimsGetBC(bcid);
  i = bc->yinpos;
  if (i == bc->yinlen && i > 0) 
    i--;
  zu_yin = malloc(9);
  yin = bc->yin[i];

  index[0] = yin >> 9;
  index[1] = (yin >> 7) & 0x03;
  if (index[1])
    index[1] += 21;
  index[2] = (yin >> 3) & 0x0f;
  if (index[2])
    index[2] += 24;
  index[3] = yin & 0x07;
  if (index[3])
    index[3] += 37;
  else
    index[3] = 38;

  zu_yin[0] = '\0';
  for (i=0; i<4; i++) {
    s = tabeZuYinIndexToZuYinSymbol(index[i]);
    if (s)  
      strcat((char *)zu_yin, (char *)s);
  }
  return zu_yin;
}

/*
 * return the number of available selection
 */
int
bimsQuerySelectionNumber(unsigned long int bcid)
{
  struct bimsContext *bc;

  bc = bimsGetBC(bcid);
  return(bc->zsel.num);
}

/*
 * return the base of selection
 */
int
bimsQuerySelectionBase(unsigned long int bcid)
{
  struct bimsContext *bc;

  bc = bimsGetBC(bcid);
  return(bc->zsel.base);
}

/*
 * return the string of selection
 */
unsigned char **
bimsQuerySelectionText(unsigned long int bcid)
{
  struct bimsContext *bc;
  unsigned char **str;

  bc = bimsGetBC(bcid);
  str = bc->zsel.str;

  return(str);
}

#include <ctype.h>

#define NUM_OF_ZUYIN_SYMBOL 42

static int ZozyKeyMap[] = {
  0,
  '1', 'q', 'a', 'z', '2', 'w', 's', 'x', 'e', 'd',
  'c', 'r', 'f', 'v', '5', 't', 'g', 'b', 'y', 'h',
  'n', 'u', 'j', 'm', '8', 'i', 'k', ',', '9', 'o',
  'l', '.', '0', 'p', ';', '/', '-', ' ', '6', '3',
  '4', '7',
};

static ZuYinIndex
bimsZozyKeyToZuYinIndex(int key)
{
  int idx;

  key = tolower(key);

  for (idx = 1; idx <= NUM_OF_ZUYIN_SYMBOL; idx++) {
    if (key == ZozyKeyMap[idx]) {
      return(idx);
    }
  }

  return(0);
}

static int EtenKeyMap[] = {
  0,
  'b', 'p', 'm', 'f', 'd', 't', 'n', 'l', 'v', 'k',
  'h', 'g', '7', 'c', ',', '.', '/', 'j', ';', '\'',
  's', 'e', 'x', 'u', 'a', 'o', 'r', 'w', 'i', 'q',
  'z', 'y', '8', '9', '0', '-', '=', ' ', '2', '3',
  '4', '1',
};

static ZuYinIndex
bimsEtenKeyToZuYinIndex(int key)
{
  int idx;

  key = tolower(key);

  for (idx = 1; idx <= NUM_OF_ZUYIN_SYMBOL; idx++) {
    if (key == EtenKeyMap[idx]) {
      return(idx);
    }
  }

  return(0);
}

static int Eten26KeyMap[] = {
  0,
  'b', 'p', 'm', 'f', 'd', 't', 'n', 'l', 'v', 'k',
  'h', 'g', 'v', 'c', 'g', 'y', 'c', 'j', 'q', 'w',
  's', 'e', 'x', 'u', 'a', 'o', 'r', 'w', 'i', 'q',
  'z', 'p', 'm', 'n', 't', 'l', 'h', ' ', 'd', 'f',
  'j', 'k',
};

static ZuYinIndex
bimsEten26KeyToZuYinIndex(int key)
{
  int idx;

  key = tolower(key);

  for (idx = 1; idx <= NUM_OF_ZUYIN_SYMBOL; idx++) {
    if (key == Eten26KeyMap[idx]) {
      return(idx);
    }
  }

  return(0);
}

/*
 * a Eten 26 ZuYin input method finite state machine
 *
 * return 0 if it's in one of it's terminal state
 *       -1 if it's not.
 */
static int
bimsEten26ZuYinContextInput(struct ZuYinContext *zc, int index)
{
  int i, rval;
  ZhiStr str;

  /* override rules for some keys */
  switch (index) {
  case 2:	/* p */
    if (zc->index[1] == 0) {
      if (zc->index[0] != 0 && zc->index[0] != 1 && zc->index[0] != 7 &&
	  zc->index[0] !=12 && zc->index[0] !=13 && zc->index[0] !=14) {
	index = 32;
      }
      else if (zc->index[0] == 12) {
	zc->index[0] = 15;
	index = 32;
      }
      else if (zc->index[0] == 14) {
	zc->index[0] = 17;
	index = 32;
      }
    }
    else if (zc->index[1] == 22) {
      index = 32;
    }
    break;

  case 3:	/* m */
    if (zc->index[1] == 0) {
      if (zc->index[0] != 0 && zc->index[0] !=12 && zc->index[0] !=13 && 
	  zc->index[0] !=14) {
	index = 33;
      }
      else if (zc->index[0] == 12) {
	zc->index[0] = 15;
	index = 33;
      }
      else if (zc->index[0] == 14) {
	zc->index[0] = 17;
	index = 33;
      }
    }
    else {
      index = 33;
    }
    break;

  case 6:	/* t */
    if (zc->index[1] == 0) {
      if (zc->index[0] != 0 && zc->index[0] !=12 && zc->index[0] !=13 &&
	  zc->index[0] !=14) {
	index = 35;
      }
      else if (zc->index[0] ==12) {
	zc->index[0] = 15;
	index = 35;
      }
      else if (zc->index[0] ==14) {
	zc->index[0] = 17;
	index = 35;
      }
    }
    else if (zc->index[1] == 22 || zc->index[1] == 23) {
      index = 35;
    }
    break;

  case 7:	/* n */
    if (zc->index[1] == 0) {
      if (zc->index[0] == 1 || zc->index[0] == 2 || zc->index[0] == 3 ||
          zc->index[0] == 4 || zc->index[0] == 9 || zc->index[0] ==10 ||
          zc->index[0] ==11 || zc->index[0] ==15 || zc->index[0] ==16 ||
          zc->index[0] ==17 || zc->index[0] ==18 || zc->index[0] ==19 ||
          zc->index[0] ==20 || zc->index[0] ==21) {
	index = 34;
      }
      else if (zc->index[0] ==12) {
	zc->index[0] = 15;
	index = 34;
      }
      else if (zc->index[0] ==14) {
	zc->index[0] = 17;
	index = 34;
      }
    }
    else {
      index = 34;
    }
    break;

  case 8:	/* l */
    if (zc->index[1] == 0) {
      if (zc->index[0] != 0 && zc->index[0] !=12 && zc->index[0] !=13 &&
	  zc->index[0] != 14) {
	index = 36;
      }
      else if (zc->index[0] ==12) {
	zc->index[0] = 15;
	index = 36;
      }
      else if (zc->index[0] ==14) {
	zc->index[0] = 17;
	index = 36;
      }
    }
    else {
      index = 36;
    }
    break;

  case 9:	/* v */
    if (zc->index[1] == 22 || zc->index[1] == 24) {
      index = 13;
    }
    break;

  case 19:	/* q */
    if (zc->index[1] == 0) {
      if (zc->index[0] == 1 || zc->index[0] == 2 || zc->index[0] == 3 ||
	  zc->index[0] == 4 || zc->index[0] == 5 || zc->index[0] == 7 ||
	  zc->index[0] == 8 || zc->index[0] == 9 || zc->index[0] ==11 ||
	  zc->index[0] ==17 || zc->index[0] ==19) {
	index = 30;
      }
      else if (zc->index[0] ==14) {
	zc->index[0] = 17;
	index = 30;
      }
    }
    else if (zc->index[1] == 23) {
      index = 30;
    }
    break;

  case 20:	/* w */
    if (zc->index[1] != 0) {
      index = 28;
    }
    break;

  case 22:
  case 24:
    if (zc->index[0] == 9) {
      zc->index[0] = 13;
    }
    if (zc->index[0] == 15) {
      zc->index[0] = 12;
    }
    else if (zc->index[0] == 17) {
      zc->index[0] = 14;
    }
    break;

  case 23:
  case 25:
  case 27:
  case 29:
  case 31:
    if (zc->index[1] == 0) {
      if (zc->index[0] == 12) {
	zc->index[0] = 15;
      }
      else if (zc->index[0] == 14) {
	zc->index[0] = 17;
      }
    }
    break;

  case 4:	/* f */
    if (zc->index[1] == 0 && zc->index[2] == 0) {
      if (zc->index[0] ==15 || zc->index[0] ==16 || zc->index[0] ==17 ||
	  zc->index[0] ==20) {
	index = 39;
      }
      else if (zc->index[0] == 6) {
	zc->index[0] = 0;
	zc->index[2] = 35;
	index = 39;
      }
      else if (zc->index[0] == 11) {
	zc->index[0] = 0;
	zc->index[2] = 37;
	index = 39;
      }
      else if (zc->index[0] == 12) {
	zc->index[0] = 15;
	index = 39;
      }
      else if (zc->index[0] == 14) {
	zc->index[0] = 17;
	index = 39;
      }
      else if (zc->index[0] == 19) {
	zc->index[0] = 0;
	zc->index[2] = 30;
	index = 39;
      }
    }
    else {
      index = 39;
    }
    break;

  case 5:	/* d */
    if (zc->index[1] == 0 && zc->index[2] == 0) { 
      if (zc->index[0] == 17 || zc->index[0] == 19) {
	index = 42;
      }
      else if (zc->index[0] == 14) {
	zc->index[0] = 17;
	index = 42;
      }
      else if (zc->index[0] == 7) {
	zc->index[0] = 0;
	zc->index[2] = 34;
	index = 42;
      }
    }
    else {
      index = 42;
    }
    break;

  case 18:	/* j */
    if (zc->index[1] == 0 && zc->index[2] == 0) { 
      if (zc->index[0] == 15 || zc->index[0] == 16 || zc->index[0] == 17 ||
	  zc->index[0] == 19 || zc->index[0] == 20 || zc->index[0] == 21) {
	index = 40;
      }
      else if (zc->index[0] == 2) {
	zc->index[0] = 0;
	zc->index[2] = 32;
	index = 40;
      }
      else if (zc->index[0] == 3) {
	zc->index[0] = 0;
	zc->index[2] = 33;
	index = 40;
      }
      else if (zc->index[0] == 7) {
	zc->index[0] = 0;
	zc->index[2] = 34;
	index = 40;
      }
      else if (zc->index[0] == 11) {
	zc->index[0] = 0;
	zc->index[2] = 37;
	index = 40;
      }
      else if (zc->index[0] == 12) {
	zc->index[0] = 15;
	index = 40;
      }
      else if (zc->index[0] == 14) {
	zc->index[0] = 17;
	index = 40;
      }
    }
    else {
      index = 40;
    }
    break;

  case 10:	/* k */
    if (zc->index[1] == 0 && zc->index[2] == 0) { 
      if (zc->index[0] == 15 || zc->index[0] == 16 || zc->index[0] == 17 ||
	  zc->index[0] == 18 || zc->index[0] == 19 || zc->index[0] == 20 ||
	  zc->index[0] == 21) {
	index = 41;
      }
      else if (zc->index[0] == 2) {
	zc->index[0] = 0;
	zc->index[2] = 32;
	index = 41;
      }
      else if (zc->index[0] == 3) {
	zc->index[0] = 0;
	zc->index[2] = 33;
	index = 41;
      }
      else if (zc->index[0] == 6) {
	zc->index[0] = 0;
	zc->index[2] = 35;
	index = 41;
      }
      else if (zc->index[0] == 7) {
	zc->index[0] = 0;
	zc->index[2] = 34;
	index = 41;
      }
      else if (zc->index[0] == 11) {
	zc->index[0] = 0;
	zc->index[2] = 37;
	index = 41;
      }
      else if (zc->index[0] == 12) {
	zc->index[0] = 15;
	index = 41;
      }
      else if (zc->index[0] == 14) {
	zc->index[0] = 17;
	index = 41;
      }
    }
    else {
      index = 41;
    }
    break;

  case 38:	/* space */
    if (zc->index[1] == 0 && zc->index[2] == 0) {
      if (zc->index[0] == 2) {
	zc->index[0] = 0;
	zc->index[2] = 32;
      }
      else if (zc->index[0] == 7) {
	zc->index[0] = 0;
	zc->index[2] = 34;
      }
      else if (zc->index[0] == 3) {
	zc->index[0] = 0;
	zc->index[2] = 33;
      }
      else if (zc->index[0] == 11) {
	zc->index[0] = 0;
	zc->index[2] = 37;
      }
      else if (zc->index[0] == 12) {
	zc->index[0] = 15;
      }
      else if (zc->index[0] == 14) {
	zc->index[0] = 17;
      }
    }
    break;
  }

  if (index >  0 && index < 22) {
    zc->index[0] = index;
  }
  if (index > 21 && index < 25) {
    zc->index[1] = index;
  }
  if (index > 24 && index < 38) {
    zc->index[2] = index;
  }
  if (index > 37 && index < 43) {
    zc->index[3] = index;
  }

  memset(zc->string, 0, sizeof(zc->string));
  if (zc->index[0] || zc->index[1] || zc->index[2]) {
    for (i = 0; i < 4; i++) {
      str = tabeZuYinIndexToZuYinSymbol(zc->index[i]);
      if (str) {
	strcat((char *)zc->string, (char *)str);
      }
    }
  }

  rval = -1;
  if (zc->index[3]) {
    rval = bimsZuYinContextCheck(zc);
  }

  return(rval);
}

static int HsuKeyMap[] = {
  0,
  'b', 'p', 'm', 'f', 'd', 't', 'n', 'l', 'g', 'k',
  'h', 'j', 'v', 'c', 'j', 'v', 'c', 'r', 'z', 'a',
  's', 'e', 'x', 'u', 'y', 'h', 'g', 'e', 'i', 'a',
  'w', 'o', 'm', 'n', 'k', 'l', 'l', ' ', 'd', 'f',
  'j', 's',
};

static ZuYinIndex
bimsHsuKeyToZuYinIndex(int key)
{
  int idx;

  key = tolower(key);

  for (idx = 1; idx <= NUM_OF_ZUYIN_SYMBOL; idx++) {
    if (key == HsuKeyMap[idx]) {
      return(idx);
    }
  }

  return(0);
}

/*
 * a Hsu ZuYin input method finite state machine
 *
 * return 0 if it's in one of it's terminal state
 *       -1 if it's not.
 */
static int
bimsHsuZuYinContextInput(struct ZuYinContext *zc, int index)
{
  int i, rval;
  ZhiStr str;

  /* override rules for some keys */
  if (zc->index[2]) {
    switch (index) {
    case 4:  /* F:3 */
      index = 40;
      break;
    case 5:  /* D:2 */
      index = 39;
      break;
    case 21: /* S:5 */
      index = 42;
      break;
    case 12: /* J:4 */
      index = 41;
      break;
    }
  }
  else if (zc->index[1]) {
    switch (index) {
    case 3:  /* M */
      index = 33;
      break;
    case 4:  /* F */
      index = 40;
      break;
    case 5:  /* D */
      index = 39;
      break;
    case 7:  /* N */
      index = 34;
      break;
    case 8:  /* L */
      index = 36;
      break;
    case 9:  /* G */
      index = 27;
      break;
    case 10: /* K */
      index = 35;
      break;
    case 11: /* H */
      index = 26;
      break;
    case 12: /* J */
      index = 41;
      break;
    case 20: /* A */
      index = 30;
      break;
    case 21: /* S */
      index = 42;
      break;
    case 22: /* E */
      index = 28;
      break;
    case 36: /* L */
      index = 41;
      break;
    default:
      break;
    }
  }
  else if (zc->index[0]) {
    switch (index) {
    case 3:  /* M */
      index = 33;
      break;
    case 4:  /* F */
      index = 40;
      break;
    case 5:  /* D */
      index = 39;
      break;
    case 7:  /* N */
      index = 34;
      break;
    case 8: /* L */
      index = 36;
      break;
    case 9:  /* G */
      index = 27;
      break;
    case 10: /* K */
      index = 35;
      break;
    case 11: /* H */
      index = 26;
      break;
    case 12: /* J */
      index = 41;
      break;
    case 20: /* A */
      index = 30;
      break;
    case 21: /* S */
      index = 42;
      break;
    default:
      break;
    }
  }

  if (index >  0 && index < 22) {
    zc->index[0] = index;
  }
  if (index > 21 && index < 25) {
    zc->index[1] = index;
  }
  if (index > 24 && index < 38) {
    zc->index[2] = index;
  }
  if (index > 37 && index < 43) {
    zc->index[3] = index;
  }

  /* alternative based on the second index */
  /* rules suggested by joe@os.nctu.edu.tw */
  if (zc->index[0] == 12 || zc->index[0] == 15) {
    zc->index[0] = (zc->index[1] == 22 || zc->index[1] == 24) ? 12 : 15;
  }
  if (zc->index[0] == 13 || zc->index[0] == 16) {
    zc->index[0] = (zc->index[1] == 22 || zc->index[1] == 24) ? 13 : 16;
  }
  if (zc->index[0] == 14 || zc->index[0] == 17) {
    zc->index[0] = (zc->index[1] == 22 || zc->index[1] == 24) ? 14 : 17;
  }
  if (zc->index[0] == 9) {
    zc->index[0] = (zc->index[1] == 22 || zc->index[1] == 24) ? 12 : 9;
  }

  /* alternative based on the forth index */
  if (zc->index[0] && !zc->index[1] && !zc->index[2] &&
      index > 37) {
    switch(zc->index[0]) {
    case 3:
      zc->index[0] = 0;
      zc->index[2] = 33;
      break;
    case 7:
      zc->index[0] = 0;
      zc->index[2] = 34;
      break;
    case 8:
      zc->index[0] = 0;
      zc->index[2] = 37;
      break;
    case 9:
      zc->index[0] = 0;
      zc->index[2] = 27;
      break;
    case 10:
      zc->index[0] = 0;
      zc->index[2] = 35;
      break;
    case 11:
      zc->index[0] = 0;
      zc->index[2] = 26;
      break;
   /* case 20:
      zc->index[0] = 0;
      zc->index[2] = 30;
      break; */
    }
  }

  memset(zc->string, 0, sizeof(zc->string));
  if (zc->index[0] || zc->index[1] || zc->index[2]) {
    for (i = 0; i < 4; i++) {
      str = tabeZuYinIndexToZuYinSymbol(zc->index[i]);
      if (str) {
	strcat((char *)zc->string, (char *)str);
      }
    }
  }

  rval = -1;
  if (zc->index[3]) {
    rval = bimsZuYinContextCheck(zc);
  }

  return(rval);
}
