/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: tabe_util.c,v 1.1 2000/12/09 09:14:12 thhsieh Exp $
 *
 */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "tabe.h"
#include "tabe_charref.h"

/*
 * return TRUE if it's a big5 code, FALSE otherwise
 */
int
tabeZhiIsBig5Code(Zhi zhi)
{
  if (!zhi) {
    return(FALSE);
  }

  if (zhi[0] < 0xa1 || zhi[0] > 0xf9) {
    return(FALSE);
  }

  if ((zhi[1] < 0x40 || zhi[1] > 0x7e) &&
      (zhi[1] < 0xa1 || zhi[1] > 0xfe)) {
    return(FALSE);
  }

  return(TRUE);
}

/*
 * Convert Zhi to ZhiCode
 */
ZhiCode
tabeZhiToZhiCode(Zhi zhi)
{
  if (!zhi) {
    return(0);
  }
  return((zhi[0]*256+zhi[1]) & 0xFFFF);
}

/*
 * Convert ZhiCode to Zhi
 */
Zhi
tabeZhiCodeToZhi(ZhiCode code)
{
  Zhi zhi;

  zhi = (Zhi)malloc(sizeof(unsigned char)*3);
  zhi[0] = code/256;
  zhi[1] = code%256;
  zhi[2] = (unsigned char)NULL;

  return(zhi);
}

/*
 * Converts ZhiCode to Packed Big5 Code
 * (5401 frequently used characters +
 *  7652 less frequently used characters +
 *     7 Eten extented characters
 *    37 ZuYin symbol characters)
 *
 * returns Packed Big5 Code if it's included in the 13060 characters,
 * otherwise, returns -1
 */
int
tabeZhiCodeToPackedBig5Code(ZhiCode code)
{
  int high, low;
  int index;

  high = code/256;
  low  = code%256;

  index = -1;

  if (high >= 0xa4 && high < 0xc6) {
    if (low >= 0x40 && low <= 0x7e) {
      index = (high-0xa4)*157+(low-0x40);
    }
    if (low >= 0xa1 && low <= 0xfe) {
      index = (high-0xa4)*157+63+(low-0xa1);
    }
  }

  if (high == 0xc6) {
    if (low >= 0x40 && low <= 0x7e) {
      index = (high-0xa4)*157+(low-0x40);
    }
  }

  if (high >= 0xc9 && high < 0xf9) {
    if (low >= 0x40 && low <= 0x7e) {
      index = (high-0xc9)*157+(low-0x40)+5401;
    }
    if (low >= 0xa1 && low <= 0xfe) {
      index = (high-0xc9)*157+63+(low-0xa1)+5401;
    }
  }

  if (high == 0xf9) {
    if (low >= 0x40 && low <= 0x7e) {
      index = (high-0xc9)*157+(low-0x40)+5401;
    }
    if (low >= 0xa1 && low <= 0xdc) {
      index = (high-0xc9)*157+63+(low-0xa1)+5401;
    }
  }

  if (high == 0xa3) {	/* These are ZuYin symbol characters. */
    if (low >= 0x74 && low <= 0x7e) {
      index = 13060 + (low-0x74);
    }
    if (low >= 0xa1 && low <= 0xba) {
      index = 13060 + 11 + (low-0xa1);
    }
  }

  return(index);
}

/*
 * Given the Code, returns the reference counts.
 *
 * returns frequency if success, 0 if failed
 */
unsigned long int
tabeZhiCodeLookupRefCount(ZhiCode code)
{
  int pcode;

  pcode = tabeZhiCodeToPackedBig5Code(code);
  if (pcode < 0) {
    return(0);
  }

  return(_CharRef[pcode]);
}
