/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 * Copyright 1999, Chih-Hao Tsai, All Rights Reserved.
 * Copyright 1999, Shian-Hua Lin, All Rights Reserved.
 *
 * $Id: tabe_tsi.c,v 1.1 2000/12/09 09:14:12 thhsieh Exp $
 *
 */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tabe.h"

/*
 * given a Tsi, loaded with all possible TsiYins
 */
int
tabeTsiInfoLookupPossibleTsiYin(struct TsiInfo *tsi)
{
  struct ZhiInfo *zhi;
  int len = strlen((char *)tsi->tsi)/2;
  Yin *yin, *yindata;
  int rval, i, j;
  int limit, index, valid, num;

  /* allocate enough Zhi structure */
  zhi = (struct ZhiInfo *)malloc(sizeof(struct ZhiInfo)*len);
  memset(zhi, 0, sizeof(struct ZhiInfo)*len);

  for (i = 0; i < len; i++) {
    zhi[i].code = tabeZhiToZhiCode(tsi->tsi+i*2);
    rval = tabeZhiInfoLookupYin(zhi+i);
    if (rval < 0) {
      fprintf(stderr,
	      "tabeTsiInfoLookupPossibleTsiYin():%s: a Zhi with no Yins.\n",
	      tsi->tsi);
    }
  }

  yin = (Yin *)malloc(sizeof(Yin)*len);
  yindata = (Yin *)NULL;
  num = 0;
  limit = 1;
  for (i = 0; i < len; i++) {  /* calculate maxmium combinations */
    limit = limit * 4;
  }
  for (i = 0; i < limit; i++) {
    valid = 1;
    memset(yin, 0, sizeof(Yin)*len);
    for (j = 0; j < len; j++) {
      index = (i >> j*2)%4;
      if (zhi[j].yin[index] == 0) {
	valid = 0;
	break;
      }
      yin[j] = zhi[j].yin[index];
    }
    if (valid) {
      yindata = (Yin *)realloc(yindata, sizeof(Yin)*len*(num+1));
      memcpy(yindata+(len*num), yin, sizeof(Yin)*len);
      num++;
    }
  }

  if (tsi->yinnum && tsi->yindata) {
    free(tsi->yindata);
  }
  tsi->yinnum = num;
  tsi->yindata = yindata;

  free(zhi);

  return(num);
}

/*
 * implement of simplx maximum matching segmentation algorithm
 *
 * return 0 if success, -1 if error
 */
int
tabeChunkSegmentationSimplex(struct TsiDB *tsidb, struct ChunkInfo *chunk)
{
  int tsihead, tsilen;
  int rval;
  struct TsiInfo tsi;
  int len = strlen((char *)chunk->chunk);
  ZhiStr buf;


  buf = (ZhiStr)malloc(sizeof(unsigned char)*(len+1));

  for (tsihead = 0; tsihead <= len-2;) {
    for (tsilen = len - tsihead; tsihead + tsilen <= len; tsilen-=2) {
      memset(&tsi, 0, sizeof(tsi));
      tsi.tsi = buf;
      strncpy((char *)buf, (char *)chunk->chunk+tsihead, tsilen);
      buf[tsilen] = (unsigned char)NULL;
      if (tsilen == 2) { /* we assume signle-character a word */
	rval = 0;
      }
      else {
	rval = tsidb->Get(tsidb, &tsi);
      }
      if (!rval) {
	chunk->tsi = (struct TsiInfo *)
	  realloc(chunk->tsi, sizeof(struct TsiInfo)*(chunk->num_tsi+1));
	memcpy(chunk->tsi+chunk->num_tsi, &tsi, sizeof(tsi));
	chunk->tsi[chunk->num_tsi].tsi = (ZhiStr)
	  malloc(sizeof(unsigned char)*(tsilen+1));
	strcpy((char *)chunk->tsi[chunk->num_tsi].tsi, (char *)buf);
	chunk->num_tsi++;
	tsihead += tsilen;
	break;
      }
    }
  }

  return(0);
}

struct complex_mmseg {
  int s1, s2, s3;
  int len;                /* maximum matching
			     (Chen & Liu 1992) */
  double avg_word_len;    /* largest average word length
			     (Chen & Liu 1992) */
  double smallest_var;    /* smallest variance of word lengths
			     (Chen & Liu 1992) */
  double largest_sum;     /* largest sum of degree of morhemic freedom of 
			     one-character words (Tsai 1996) */
};

/*
 * implement of complex maximum matching segmentation algorithm
 * as described in
 * http://casper.beckman.uiuc.edu/~c-tsai4/chinese/wordseg/mmseg.html
 *
 * return 0 if success, -1 if error
 */
int
tabeChunkSegmentationComplex(struct TsiDB *tsidb, struct ChunkInfo *chunk)
{
  struct complex_mmseg *comb = (struct complex_mmseg *)NULL;
  int i, j, k, rval;
  int tsihead, len = strlen((char *)chunk->chunk), ncomb = 0;
  int ncand, *cand;
  int *tmpcand, tmpncand;
  ZhiStr buf;
  ZhiCode code;
  struct TsiInfo tsi;
  int max_int, index;
  double max_double;
  int verbose = 0;

  if (len == 0) {
    return(0);
  }

  buf = (ZhiStr)malloc(sizeof(unsigned char)*(len+1));
  tsihead = 0;

  while (len > tsihead) {
    /*
     * if it's a one-character word, why bother segmentation?
     */
    if (len == tsihead + 2) {
      chunk->tsi = (struct TsiInfo *)
	realloc(chunk->tsi, sizeof(struct TsiInfo)*(chunk->num_tsi+1));
      chunk->tsi[chunk->num_tsi].tsi = 
	(unsigned char *)strdup(chunk->chunk+tsihead);
      chunk->tsi[chunk->num_tsi].yindata = (Yin *)NULL;
      tsidb->Get(tsidb, chunk->tsi+chunk->num_tsi);
      chunk->num_tsi++;
      return(0);
    }
    /*
     * when it's a two-character word, why bother complex segmentation?
     * use simplex instead.
     */
    if (len == tsihead + 4) {
      strncpy((char *)buf, (char *)chunk->chunk+tsihead, 4);
      buf[4] = (unsigned char)NULL;
      memset(&tsi, 0, sizeof(tsi));
      tsi.tsi = buf;
      rval = tsidb->Get(tsidb, &tsi);
      if (rval < 0) { /* not found in DB, return two one-character word */
	chunk->tsi = (struct TsiInfo *)
	  realloc(chunk->tsi, sizeof(struct TsiInfo)*(chunk->num_tsi+2));

	chunk->tsi[chunk->num_tsi].tsi = (ZhiStr)
	  malloc(sizeof(unsigned char)*3);
	strncpy((char *)chunk->tsi[chunk->num_tsi].tsi, 
		(char *)chunk->chunk+tsihead, 2);
	chunk->tsi[chunk->num_tsi].tsi[2] = (unsigned char)NULL;
	chunk->tsi[chunk->num_tsi].yindata = (Yin *)NULL;
	tsidb->Get(tsidb, chunk->tsi+chunk->num_tsi);

	chunk->tsi[chunk->num_tsi+1].tsi = (ZhiStr)
	  malloc(sizeof(unsigned char)*3);
	strncpy((char *)chunk->tsi[chunk->num_tsi+1].tsi, 
		(char *)chunk->chunk+tsihead+2, 2);
	chunk->tsi[chunk->num_tsi+1].tsi[2] = (unsigned char)NULL;
	chunk->tsi[chunk->num_tsi+1].yindata = (Yin *)NULL;
	tsidb->Get(tsidb, chunk->tsi+chunk->num_tsi+1);

	chunk->num_tsi += 2;
      }
      else { /* found in DB, return one two-character word */
	chunk->tsi = (struct TsiInfo *)
	  realloc(chunk->tsi, sizeof(struct TsiInfo)*(chunk->num_tsi+1));
	chunk->tsi[chunk->num_tsi].tsi = 
	  (unsigned char *)strdup(chunk->chunk+tsihead);
	chunk->tsi[chunk->num_tsi].yindata = (Yin *)NULL;
	tsidb->Get(tsidb, chunk->tsi+chunk->num_tsi);
	chunk->num_tsi++;
      }
      return(0);
    }

    for (i = len-tsihead; i > 0; i-=2) {
      strncpy((char *)buf, (char *)chunk->chunk+tsihead, i);
      buf[i] = (unsigned char)NULL;
      memset(&tsi, 0, sizeof(tsi));
      tsi.tsi = buf;
      if (i != 2) { /* we assume signle-character a tsi */
	rval = tsidb->Get(tsidb, &tsi);
	if (rval < 0) {
	  continue;
	}
      }
      for (j = len-tsihead-i; j >= 0; j-=2) {
	if (j > 0) {
	  strncpy((char *)buf, (char *)chunk->chunk+tsihead+i, j);
	  buf[j] = (unsigned char)NULL;
	  memset(&tsi, 0, sizeof(tsi));
	  tsi.tsi = buf;
	  if (j != 2) { /* we assume signle-character a tsi */
	    rval = tsidb->Get(tsidb, &tsi);
	    if (rval < 0) {
	      continue;
	    }
	  }
	}
	for (k = len-tsihead-i-j; k >= 0; k-=2) {
	  if (k > 0) {
	    strncpy((char *)buf, (char *)chunk->chunk+tsihead+i+j, k);
	    buf[k] = (unsigned char)NULL;
	    memset(&tsi, 0, sizeof(tsi));
	    tsi.tsi = buf;
	    if (k != 2) { /* we assume signle-character a tsi */
	      rval = tsidb->Get(tsidb, &tsi);
	      if (rval < 0) {
		continue;
	      }
	    }
	  }
	  if (k > 0 && j == 0) {
	    continue;
	  }
	  comb = (struct complex_mmseg *)
	    realloc(comb, sizeof(struct complex_mmseg)*(ncomb+1));
	  comb[ncomb].s1 = tsihead;
	  comb[ncomb].s2 = tsihead+i;
	  comb[ncomb].s3 = tsihead+i+j;
	  comb[ncomb].len = i+j+k;
	  ncomb++;
	}
      }
    }

    /* rule 1: largest sum of three-tsi */
    max_int = 0;
    for (i = 0; i < ncomb; i++) {
      if (comb[i].len > max_int) {
	max_int = comb[i].len;
      }
    }
    ncand = 0;
    cand = (int *)NULL;
    for (i = 0; i < ncomb; i++) {
      if (comb[i].len == max_int) {
	if (verbose) {
	  printf("rule 1(%d/%d): %d %d %d %d\n", i, ncand,
		     comb[i].s1, comb[i].s2, comb[i].s3, comb[i].len);
	}
	cand = (int *)realloc(cand, sizeof(int)*(ncand+1));
	cand[ncand] = i;
	ncand++;
      }
    }

    /* resolved by rule 1 */
    if (ncand == 1) {
      index = cand[0];
    }
    else { /* ambiguity */
      if (verbose) {
	printf("->rule 2: %d\n", ncand);
      }
      /* rule 2: largest average word length */
      max_double = 0;
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
	}
	if (verbose) {
	  printf("rule 2(%d/%d): %f %f %d %d %d %d\n", index, ncand,
		 comb[index].avg_word_len, max_double,
		 comb[index].s1, comb[index].s2, comb[index].s3,
		 comb[index].len);
	}
      }

      tmpncand = 0;
      tmpcand = (int *)NULL;
      for (i = 0; i < ncand; i++) {
	index = cand[i];
	if (comb[index].avg_word_len == max_double) {
	  tmpcand = (int *)realloc(tmpcand, sizeof(int)*(tmpncand+1));
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
	if (verbose) {
	  printf("->rule 3: %d\n", ncand);
	}
	/* rule 3: smallest variance of word length */
	max_double = 1000;  /* this is misleading */
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
	  }
	  if (verbose) {
	    printf("rule 3(%d/%d): %f %f %d %d %d %d\n", index, ncand,
		   comb[index].smallest_var, max_double,
		   comb[index].s1, comb[index].s2, comb[index].s3,
		   comb[index].len);
	  }
	}

	tmpncand = 0;
	tmpcand = (int *)NULL;
	for (i = 0; i < ncand; i++) {
	  index = cand[i];
	  if (comb[index].smallest_var == max_double) {
	    tmpcand = (int *)realloc(tmpcand, sizeof(int)*(tmpncand+1));
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
	  if (verbose) {
	    printf("->rule 4: %d\n", ncand);
	  }
	  /*
	   * rule 4: largest sum of degree of morphemic
	   *         freedom of one-character words
	   */
	  max_double = 0;
	  for (i = 0; i < ncand; i++) {
	    index = cand[i];
	    comb[index].largest_sum = 0;
	    k = (comb[index].s2-comb[index].s1);
	    if (k == 2) {
	      code = tabeZhiToZhiCode(chunk->chunk+comb[index].s1);
	      comb[index].largest_sum += tabeZhiCodeLookupRefCount(code);
	    }
	    k = (comb[index].s3-comb[index].s2);
	    if (k == 2) {
	      code = tabeZhiToZhiCode(chunk->chunk+comb[index].s2);
	      comb[index].largest_sum += tabeZhiCodeLookupRefCount(code);
	    }
	    k = (comb[index].len+comb[index].s1-comb[index].s3);
	    if (k == 2) {
	      code = tabeZhiToZhiCode(chunk->chunk+comb[index].s3);
	      comb[index].largest_sum += tabeZhiCodeLookupRefCount(code);
	    }

	    if (comb[index].largest_sum > max_double) {
	      max_double = comb[index].largest_sum;
	    }
	    if (verbose) {
	      printf("rule 4(%d/%d): %f %f %d %d %d %d\n", index, ncand,
		     comb[index].largest_sum, max_double,
		     comb[index].s1, comb[index].s2, comb[index].s3,
		     comb[index].len);
	    }
	  }

	  tmpncand = 0;
	  tmpcand = (int *)NULL;
	  for (i = 0; i < ncand; i++) {
	    index = cand[i];
	    if (comb[index].largest_sum == max_double) {
	      tmpcand = (int *)realloc(tmpcand, sizeof(int)*(tmpncand+1));
	      tmpcand[tmpncand] = index;
	      tmpncand++;
	    }
	  }

	  ncand = tmpncand;
	  free(cand);
	  cand = tmpcand;
	  tmpcand = (int *)NULL;

	  /* resolved by rule 4 */
	  if (ncand == 1) {
	    index = cand[0];
	  }
	  else { /* ambiguity */
	    /* well, use the first one */
	    index = cand[0];
	  }
	}
      }
    }

    chunk->tsi = (struct TsiInfo *)
      realloc(chunk->tsi, sizeof(struct TsiInfo)*(chunk->num_tsi+1));
    chunk->tsi[chunk->num_tsi].tsi = (ZhiStr)
      malloc(sizeof(unsigned char)*(comb[index].s2-comb[index].s1+1));
    strncpy((char *)chunk->tsi[chunk->num_tsi].tsi, 
	    (char *)chunk->chunk+tsihead, comb[index].s2-comb[index].s1);
    chunk->tsi[chunk->num_tsi].tsi[comb[index].s2-comb[index].s1] =
      (unsigned char)NULL;
    chunk->tsi[chunk->num_tsi].yindata = (Yin *)NULL;
    tsidb->Get(tsidb, chunk->tsi+chunk->num_tsi);
    chunk->num_tsi++;

    tsihead += comb[index].s2 - comb[index].s1;

    free(comb);
    comb = (struct complex_mmseg *)NULL;
    ncomb = 0;
    free(cand);
  }

  return(0);
}

/*
 * implement of backward maximum matching segmentation algorithm
 * proposed by Lin Shian-Hua, 1999.
 *
 * return 0 if success, -1 if error
 */
int
tabeChunkSegmentationBackward(struct TsiDB *tsidb, struct ChunkInfo *chunk)
{
  int tsitail, tsilen;
  int rval, i;
  struct TsiInfo tsi, *tmptsi;
  int len = strlen((char *)chunk->chunk);
  ZhiStr buf;

  buf = (ZhiStr)malloc(sizeof(unsigned char)*(len+1));

  for (tsitail = len; tsitail > 0;) {
    for (tsilen = len; tsilen >= 2; tsilen-=2) {
      memset(&tsi, 0, sizeof(tsi));
      tsi.tsi = buf;
      strncpy((char *)buf, (char *)chunk->chunk+(tsitail-tsilen), tsilen);
      buf[tsilen] = (unsigned char)NULL;
      if (tsilen == 2) { /* we assume signle-character a word */
	rval = 0;
      }
      else {
	rval = tsidb->Get(tsidb, &tsi);
      }
      if (!rval) {
	chunk->tsi = (struct TsiInfo *)
	  realloc(chunk->tsi, sizeof(struct TsiInfo)*(chunk->num_tsi+1));
	memcpy(chunk->tsi+chunk->num_tsi, &tsi, sizeof(tsi));
	chunk->tsi[chunk->num_tsi].tsi = (ZhiStr)
	  malloc(sizeof(unsigned char)*(tsilen+1));
	strcpy((char *)chunk->tsi[chunk->num_tsi].tsi, (char *)buf);
	chunk->num_tsi++;
	tsitail -= tsilen;
	len -= tsilen;
	break;
      }
    }
  }

  tmptsi = (struct TsiInfo *)malloc(sizeof(struct TsiInfo)*chunk->num_tsi);
  for (i = 0; i < chunk->num_tsi; i++) {
    tmptsi[i] = chunk->tsi[(chunk->num_tsi-1)-i];
  }
  free(chunk->tsi);
  chunk->tsi = tmptsi;

  return(0);
}
