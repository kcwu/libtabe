/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: tsiadd.c,v 1.2 2001/01/12 15:38:46 thhsieh Exp $
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

#define BUF_SIZE 4096

void
usage(void)
{
  printf("Usage: tsiadd -d <TsiDB> [-f input file -r -y]\n");
  printf("   -d <TsiDB>     \t path to TsiDB\n");
  printf("   -f <input file>\t input file in plain text (default: stdin)\n");
  printf("   -r             \t incorporate reference count (default: not)\n");
  printf("   -y             \t incorporate yin data (default: not)\n");
  printf("   -v             \t verbose output for debugging\n");
  exit(0);
}

int
skip_comment(unsigned char *buf)
{
  unsigned char *s = buf;

  while(*s != '\0') {
    if (*s == '#') {
      *s = '\0';
      break;
    }
    else if ((*s >= 0xA1 && *s <= 0xFE) &&
	     ((*(s+1) >= 0x40 && *(s+1) <= 0x7E) ||
	      (*(s+1) >= 0xA1 && *(s+1) <= 0xFE))) {
      s += 2;
    }
    else {
      s ++;
    }
  }
  return (*buf == '\0') ? -1 : 0;
}

int
n_embeded(char **s)
{
  int cnt=0;

  (*s) ++;
  while (**s != ']' && **s != '\0') {
    (*s) += 2;
    if (**s == ',') {
      cnt ++;
      (*s) ++;
    }
  }
  (*s) ++;
  return (cnt+1);
}

int
yincpy_embeded(char *yincp, char *preyin, char **s)
{
  int len=0;

  while (*preyin != '\0') {
    yincp[len] = *preyin;
    preyin ++;
    len ++;
  }
  while (**s != ',' && **s != ']' && **s != '\0') {
    yincp[len] = **s;
    (*s) ++;
    len ++;
  }
  yincp[len] = '\0';
  return len;
}

char **
embeded_expand(char *preyin, char *s, int *n_yin)
{
  char **ylist, **yltmp, *s1=s, *s2=s, *s3, *buf, mypre[BUF_SIZE];
  int nlist, nlist2, i, j, yinlen, len, my_nlist, n_idx=0;

  nlist2 = nlist = n_embeded(&s1);
  ylist = malloc(sizeof(char *) * nlist);

  s2 ++;
  yinlen = strlen(s) + strlen(preyin);
  for (i=0; i<nlist; i++) {
    buf = malloc(sizeof(char) * (yinlen+1));
    len = yincpy_embeded(buf, preyin, &s2);
    s2 ++;
    strncat(buf, s1, yinlen-len);
    buf[yinlen] = '\0';

    s3 = buf;
    while (*s3 != '\0' && *s3 != '[') {
      s3 += 2;
    }
    if (*s3 == '[') {
      if (s3 > buf) {
	strncpy(mypre, buf, (unsigned int)(s3-buf));
	mypre[(unsigned int)(s3-buf)] = '\0';
      }
      else {
	mypre[0] = '\0';
      }
      yltmp = embeded_expand(mypre, s3, &my_nlist);
      nlist2 = nlist2 - 1 + my_nlist;
      ylist = realloc(ylist, sizeof(char *) * nlist2);
      for (j=0; j<my_nlist; j++) {
	ylist[n_idx++] = yltmp[j];
      }
      free(buf);
      free(yltmp);
    }
    else {
      ylist[n_idx++] = buf;
    }
  }
  *n_yin = nlist2;
  return ylist;
}

void
tsiyin_expand(char *yin, int yinlen, char *tmpyin, int verbose)
{
  char *s=tmpyin, preyin[BUF_SIZE];
  char yinbuf[BUF_SIZE], *s1=yinbuf, *s2=yinbuf;
  char **ylist;
  int n_yin, i, len_tot=0, found=0, get_white=0;

  while (*s != '\0' && *s != '\n') {
    get_white = 0;
    switch (*s) {
    case  ' ':  *s1 = (char)0xA1;
		*(s1+1) = (char)0x40;
		s ++;
		s1 += 2;
		/* then skip the following white spaces */
		while (*s) {
		  if (*s == ' ') {
		    s ++;
		  }
		  else if (*s == (char)0xA1 && *(s+1) == (char)0x40) {
		    s += 2;
		  }
		  else {
		    break;
		  }
		}
		get_white = 1;
		break;
    case  '2':  *s1 = (char)0xA3;
		*(s1+1) = (char)0xBD;
		s ++;
		s1 += 2;
		break;
    case  '3':  *s1 = (char)0xA3;
		*(s1+1) = (char)0xBE;
		s ++;
		s1 += 2;
		break;
    case  '4':  *s1 = (char)0xA3;
		*(s1+1) = (char)0xBF;
		s ++;
		s1 += 2;
		break;
    case  '0':
    case  '5':  *s1 = (char)0xA3;
		*(s1+1) = (char)0xBB;
		s ++;
		s1 += 2;
		break;
    case  '[':  *s1 = *s;
		if (found == 0) {
		  s2 = s1;
		  found = 1;
		}
		s ++;
		s1 ++;
		break;
    case  ',':  *s1 = *s;
		s ++;
		s1 ++;
		break;
    case  ']':  *s1 = *s;
		s ++;
		s1 ++;
		break;
    default:	*s1 = *s;
		*(s1+1) = *(s+1);
		s += 2;
		s1 += 2;
		if (*(s1-2) == (char)0xA1 && *(s1-1) == (char)0x40) {
		  /* then skip the following white spaces */
		  while (*s) {
		    if (*s == ' ') {
		      s ++;
		    }
		    else if (*s == (char)0xA1 && *(s+1) == (char)0x40) {
		      s += 2;
		    }
		    else {
		      break;
		    }
		  }
		  get_white = 1;
		}
		break;
    }
  }
  if (get_white == 1) {
    *(s1-2) = '\0';
  }
  else {
    *s1 = '\0';
  }
  if (verbose) {
    printf("%s\n", yinbuf);
  }

  s = s2;
  if (*s == '[') {
    if (s > yinbuf) {
      strncpy(preyin, yinbuf, (unsigned int)(s - yinbuf));
      preyin[(unsigned int)(s - yinbuf)] = '\0';
    }
    else {
      preyin[0] = '\0';
    }
    ylist = embeded_expand(preyin, s, &n_yin);

    strncpy(yin, ylist[0], yinlen-1);
    len_tot = strlen(ylist[0]);
    free(ylist[0]);
    for (i=1; i<n_yin; i++) {
      strncat(yin, "¡@", yinlen-len_tot-1);
      strncat(yin, ylist[i], yinlen-len_tot-3);
      len_tot += (strlen(ylist[i]) + 2);
      free(ylist[i]);
    }
    free(ylist);
  }
  else {
    strncpy(yin, s, yinlen);
  }
  yin[yinlen-1] = '\0';
}

void
archive(struct TsiDB *db, FILE *fp, int ref, int tsiyin, int verbose)
{
  struct TsiInfo *tsi;
  int rval, i, j, l, m, n, len;
  unsigned char buf[BUF_SIZE], yin[BUF_SIZE], tmpyin[BUF_SIZE], *p, *q;
  unsigned long int refcount;

  rval = db->RecordNumber(db);
  if (rval < 0) {
    fprintf(stderr, "tsiadd: wrong DB format.\n");
    usage();
  }

  tsi = (struct TsiInfo *)malloc(sizeof(struct TsiInfo));
  tsi->tsi = (ZhiStr)malloc(sizeof(unsigned char)*80);
  memset(tsi->tsi, 0, 80);
  tsi->refcount = 0;
  tsi->yinnum = 0;
  tsi->yindata = (Yin *)NULL;

  memset(buf, 0, BUF_SIZE);
  memset(yin, 0, BUF_SIZE);

  i = j = 0;
  while (1) {
    if (!fgets((char *)buf, BUF_SIZE-1, fp)) {
      break;
    }
    if (skip_comment(buf) == -1)
	continue;

    i++;
    yin[0] = (unsigned char)NULL;
    if (!ref) {
      buf[strlen((char *)buf)-1] = (unsigned char)NULL;
      strcpy((char *)tsi->tsi, (char *)buf);
    }
    else {
      tmpyin[0] = '\0';
      sscanf((char *)buf, "%20s %ld", (char *)tsi->tsi, &refcount);
      tsi->refcount = refcount;
      p = buf;
/*
 *  Skip the following 2 words.
 */
      while (*p && (*p != ' ' && *p != '\t')) {
	p ++;
      }
      p ++;
      while (*p && (*p != ' ' && *p != '\t')) {
	p ++;
      }
/*
 *  Skip all white spaces & 0xA140
 */
      while (*p) {
	if (*p == ' ' || *p == '\t') {
	  p ++;
	}
	else if (*p == (unsigned char)0xA1 && *(p+1) == (unsigned char)0x40) {
	  p += 2;
	}
	else {
	  break;
	}
      }
      tsiyin_expand((char *)yin, BUF_SIZE, (char *)p, verbose);
    }

    len = strlen((char *)tsi->tsi)/2;
    if (strlen((char *)yin)) {
      l = 0;
      p = yin;
      while ((p = (unsigned char *)strstr((char *)p, "¡@"))) {
        p++;
        l++;
      }
      tsi->yinnum = (l+1)/len;
      tsi->yindata = (Yin *)realloc(tsi->yindata, sizeof(Yin)*(l+1));
      p = yin;
      for (m = 0; m < tsi->yinnum; m++) {
        for (n = 0; n < len; n++) {
           q = (unsigned char *)strstr((char *)p, "¡@");
           if (q) {
             strncpy((char *)tmpyin, (char *)p, q-p);
             tmpyin[q-p] = (unsigned char)NULL;
             tsi->yindata[m*len+n] = tabeZuYinSymbolSequenceToYin(tmpyin);
             p = q+2;
           }
           else {
             strncpy((char *)tmpyin, (char *)p, strlen((char *)p));
             tmpyin[strlen((char *)p)] = (unsigned char)NULL;
             tsi->yindata[m*len+n] = tabeZuYinSymbolSequenceToYin(tmpyin);
             break;
           }
        }
      }
    }
    else {
      tabeTsiInfoLookupPossibleTsiYin(tsi);
      if (tsi->yinnum > 1) {
        tsi->yinnum = 0;
        free(tsi->yindata);
        tsi->yindata = NULL;
      }
    }
    db->flags |= DB_FLAG_OVERWRITE;
    rval = db->Put(db, tsi);
    db->flags ^= DB_FLAG_OVERWRITE;
    if (!rval) {
      j++;
    }
    if (tsi->yinnum) {
      free(tsi->yindata);
      tsi->yindata = NULL;
    }
  }

  printf("There're %d queries, %d added.\n", i, j);
  db->Close(db);
}

int
main(int argc, char **argv)
{
  int ch;
  int ref, tsiyin, verbose;
  FILE *fp;
  struct TsiDB *db;
extern char *optarg;
extern int optind, opterr, optopt;

  char *db_name, *op_name;

  db_name = op_name = (char *)NULL;
  ref = 0;
  tsiyin = 0;
  verbose = 0;

  while ((ch = getopt(argc, argv, "d:f:ryv")) != -1) {
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
      case 'v':
	verbose = 1;
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

  db = tabeTsiDBOpen(DB_TYPE_DB, db_name, DB_FLAG_CREATEDB);
  if (!db) {
    usage();
  }

  if (op_name) {
    fp = fopen(op_name, "r");
    archive(db, fp, ref, tsiyin, verbose);
    fclose(fp);
  }
  else {
    archive(db, stdin, ref, tsiyin, verbose);
  }

  return(0);
}
