/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: tabe_zuyin.c,v 1.1 2000/12/09 09:14:16 thhsieh Exp $
 *
 */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "tabe.h"

#define NUM_OF_ZUYIN_SYMBOL 42

static ZuYinSymbol ZuyinSymbol[] = {
  NULL,
  "£t", "£u", "£v", "£w", "£x", "£y", "£z", "£{", "£|", "£}",
  "£~", "£¡", "£¢", "££", "£¤", "£¥", "£¦", "£§", "£¨", "£©",
  "£ª",  /* 21 */             /* 1-21 */
  "£¸", "£¹", "£º", /* 24 */   /* 1-3  */
  "£«", "£¬", "£­", "£®", "£¯", "£°", "£±", "£²", "£³", "£´",
  "£µ", "£¶", "£·", /* 37 */   /* 1-13 */
  "£¼", "£½", "£¾", "£¿", "£»", /* 1-5  */
};

/*
 * return the ZuYin symbol represents the ZuYin number
 */
const ZuYinSymbol
tabeZuYinIndexToZuYinSymbol(ZuYinIndex idx)
{
  if (idx <= 0 || idx > NUM_OF_ZUYIN_SYMBOL) {
    return(NULL);
  }
  return(ZuyinSymbol[idx]);
}


/*
 * return index of the ZuYin symbol
 */
ZuYinIndex
tabeZuYinSymbolToZuYinIndex(ZuYinSymbol sym)
{
  ZhiCode code = tabeZhiToZhiCode(sym); /* a little bit confusing */
  int idx;

  for (idx = 1; idx <= NUM_OF_ZUYIN_SYMBOL; idx++) {
    if (code == tabeZhiToZhiCode(ZuyinSymbol[idx])) { /* more confusing */
      return(idx);
    }
  }

  return(0);
}

static int ZozyKeyMap[] = {
  0,
  '1', 'q', 'a', 'z', '2', 'w', 's', 'x', 'e', 'd',
  'c', 'r', 'f', 'v', '5', 't', 'g', 'b', 'y', 'h',
  'n', 'u', 'j', 'm', '8', 'i', 'k', ',', '9', 'o',
  'l', '.', '0', 'p', ';', '/', '-', ' ', '6', '3',
  '4', '7',
};

ZuYinIndex
tabeZozyKeyToZuYinIndex(int key)
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
