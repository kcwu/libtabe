/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 * Copyright 1999, Yung-Ching Hsiao, All Rights Reserved.
 *
 * $Id: tabe_yin.c,v 1.1 2000/12/09 09:14:12 thhsieh Exp $
 *
 */

/*
 * Functions to deal with Yin are defined in this file.
 *
 */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tabe.h"

#include "tabe_zhi2yin.h"
#include "tabe_yin2zhi.h"

static int num_of_zyt_entries =
           sizeof(_ZhiYinTable)/sizeof(struct ZhiYin);

static int num_of_yzt_entries =
           sizeof(_YinZhiTable)/sizeof(struct YinZhi);

static int yzt_threshold = 0;

/*
 * Given a Zhi, looks up Yins.
 *
 * returns 0 if success, -1 if not found
 *
 */
int
tabeZhiInfoLookupYin(struct ZhiInfo *z)
{
  int index;
  int num, i;

  index = tabeZhiCodeToPackedBig5Code(z->code);
  if (index >= 0 && index < num_of_zyt_entries) {
    if (!_ZhiYinTable[index].yindata[0]) { /* no Yin defined */
      return(-1);
    }
    num = (_ZhiYinTable[index].yindata[0]) & 0xC000;
    num = num >> 14;
    num += 1;
    for (i = 0; i < num; i++) {
      z->yin[i] = _ZhiYinTable[index].yindata[i] & 0x3FFF;
    }
    for (i = num; i < 4; i++) {
      z->yin[i] = 0;
    }
    return(0);
  }
  else {
    return(-1);
  }
}

/*
 * Given a Yin, find entry in _YinZhiTable using binary search.
 *
 * returns entry if success, NULL if not found
 *
 */
static const struct YinZhi *
tabeYinToYinZhi(Yin yin)
{
  int index, step;
  int loop;

  if (!yzt_threshold) {
    while (1 << yzt_threshold < num_of_yzt_entries) {
      yzt_threshold++;
    }
  }

  index = num_of_yzt_entries/2;
  step = (index+1)/2;

  loop = 0;

  while(1) {
    if (yin == _YinZhiTable[index].yin) {
      return(_YinZhiTable+index);
    }
    if (loop > yzt_threshold) {
      break;
    }
    if (yin > _YinZhiTable[index].yin) {
      index += step;
      if (index > num_of_yzt_entries) {
	index = num_of_yzt_entries-1;
      }
    }
    else {
      index -= step;
      if (index < 0) {
	index = 0;
      }
    }
    step = ((step+1)/2) > 0 ? ((step+1)/2) : 1;
    loop++;
  }

  return(NULL);
}

/*
 * Given a Yin, packs all the Big5 Zhi in a string.
 *
 * returns the string if success, NULL if not found
 *
 */
ZhiStr
tabeYinLookupZhiList(Yin yin)
{
  int i;
  ZhiStr str;
  Zhi tmp;
  const struct YinZhi *yz;

  yz = tabeYinToYinZhi(yin);
  if (!yz) {
    return(NULL);
  }
  str = (ZhiStr)malloc(sizeof(unsigned char)*(yz->num*2+1));
  memset(str, 0, (yz->num*2+1));
  for (i = 0; i < yz->num; i++) {
    tmp = tabeZhiCodeToZhi(yz->codelist[i]);
    strcat((char *)str, (char *)tmp);
    free(tmp);
  }

  return(str);
}

/*
 * Given a Yin, converted into Zuyin symbol string.
 *
 * returns string at any case
 *
 */
ZhiStr
tabeYinToZuYinSymbolSequence(Yin yin)
{
  ZhiStr str;
  Zhi sym;
  int key;

  str = (ZhiStr)malloc(sizeof(unsigned char)*9); /* at most 8 characters */
  memset(str, 0, 9);

  key = yin & 0x3E00;
  key = key >> 9;
  sym = tabeZuYinIndexToZuYinSymbol(key);
  if (sym) {
    strcat((char *)str, (char *)sym);
  }

  key = yin & 0x180;
  key = key >> 7;
  if (key) {
    sym = tabeZuYinIndexToZuYinSymbol(key+21);
    if (sym) {
      strcat((char *)str, (char *)sym);
    }
  }

  key = yin & 0x78;
  key = key >> 3;
  if (key) {
    sym = tabeZuYinIndexToZuYinSymbol(key+24);
    if (sym) {
      strcat((char *)str, (char *)sym);
    }
  }

  key = yin & 0x7;
  if (key) {
    sym = tabeZuYinIndexToZuYinSymbol(key+37);
    if (sym) {
      strcat((char *)str, (char *)sym);
    }
  }

  return(str);
}

/*
 * Given a string of ZuYin Symbols, converts to Yin.
 *
 * returns Yin if success, 0 if failed
 */
Yin
tabeZuYinSymbolSequenceToYin(ZhiStr str)
{
  int i, len, num;
  Yin yin;
  const struct YinZhi *yz;

  if (!str) {
    return(0);
  }

  len = strlen((char *)str);

  yin = 0;
  for (i = 0; i < 4; i++) {   /* all four positions are inspected */
    if (i*2 < len) {
      num = tabeZuYinSymbolToZuYinIndex(str+i*2);
    }
    else {
      num = 0;
    }
    if (num > 0 && num < 22) {
      yin |= num << 9;
    }
    if (num > 21 && num < 25) {
      yin |= (num - 21) << 7;
    }
    if (num > 24 && num < 38) {
      yin |= (num - 24) << 3;
    }
    if (num > 38 && num < 43) {
      yin |= (num - 37);
    }
  }

  yz = tabeYinToYinZhi(yin);
  if (yz) {
    return(yin);
  }
  else {
    return(0);
  }
}
