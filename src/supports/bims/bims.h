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
 * $Id: bims.h,v 1.4 2001/09/20 00:30:24 thhsieh Exp $
 *
 */
#ifndef __BIMS_H__
#define __BIMS_H__

#include <tabe.h>
#include <X11/X.h>
#include <X11/Xproto.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>
#undef  XK_MISCELLANY

/* context for ZuYin input method finite state machine */
struct ZuYinContext {
  Yin           yin;
  int           index[4];
  unsigned char string[9];
};

/* properties for Zhi Selection state */
struct ZhiSelection {
  int           num;   /* number of available selection */
  ZhiStr       *str;   /* the selection string          */
  int           base;  /* the base of current selection */
};

/* description for segmentation */
struct YinSegInfo {
  int           yinoff;
  int           yinlen;
  Yin          *yindata;
};


/* context for a bims client */
struct bimsContext {
  int                  yinlen;
  int                  maxlen;
  Yin                 *yin;
  int                  yinpos;
  unsigned char       *internal_text;  /* text: internal text           */
  ZhiCode             *pindown;        /* flag: indicating that the Zhi
				                is pinned down          */
  int                 *tsiboundary;    /* flag: indicating the Tsi
						boundary		*/
  int                  state;          /* editing or zhi selection mode */
  unsigned long int    bcid;           /* bimsContext Identifier        */
  int                  keymap;         /* the type of keymap it uses    */

  struct ZuYinContext  zc;             /* ZuYin Context                 */
  struct ZhiSelection  zsel;           /* Zhi Selection Context         */
  int                  num_ysinfo;
  struct YinSegInfo   *ysinfo;         /* Yin Segmentation Info         */
  int		       no_smart_ed;    /* Smart-editing mode off	*/

  int                  updatedb;       /* should update db for usage    */
  struct bimsContext  *next;           /* link pointer                  */
};

/* state for each bims client */
enum {
  BC_STATE_EDITING,
  BC_STATE_SELECTION_TSI,
  BC_STATE_SELECTION_ZHI,
  BC_STATE_LAST
};

/* keymap for each bims client */
enum {
  BC_KEYMAP_ZO,
  BC_KEYMAP_ETEN,
  BC_KEYMAP_ETEN26,
  BC_KEYMAP_HSU,
  BC_KEYMAP_LAST
};

/* basic bims functions */
int                 bimsInit   (char *tsidb, char *yindb);
void                bimsDestroy(void);
struct bimsContext *bimsGetBC  (unsigned long int bcid);
void                bimsFreeBC (unsigned long int bcid);

/* use of multiple database */
int                 bimsDBPoolAppend(char *tsidb, char *yindb);
int                 bimsDBPoolPrepend(char *tsidb, char *yindb);
int                 bimsDBPoolDelete(char *tsidb, char *yindb);
int                 bimsReturnDBPool(struct TsiDB **tsidb,
                                     struct TsiYinDB **yindb);

int                 bimsFeedKey(unsigned long int bcid, KeySym key);

/*
 * return value of bimsFeedKey
 *
 * majorly due to conformance with Xcin
 * proposed by Tung-Han Hsieh
 */
enum {
  BC_VAL_COMMIT,
  BC_VAL_IGNORE,
  BC_VAL_ABSORB,
  BC_VAL_ERROR,
  BC_VAL_LAST
};

/* other bims client operations */
int                 bimsToggleZhiSelection  (unsigned long int bcid);
int                 bimsToggleTsiSelection  (unsigned long int bcid);
int                 bimsToggleEditing       (unsigned long int bcid);
int                 bimsToggleSmartEditing  (unsigned long int bcid);
int                 bimsToggleNoSmartEditing(unsigned long int bcid);
int                 bimsToggleUpdate        (unsigned long int bcid);
int                 bimsToggleNoUpdate      (unsigned long int bcid);
int                 bimsPindown             (unsigned long int bcid,
					     ZhiCode z);
int                 bimsPindownByNumber     (unsigned long int bcid,
					     int sel);
int                 bimsSetSelectionBase    (unsigned long int bcid,
					     int base);
int                 bimsSetMaxLen           (unsigned long int bcid,
					     int maxlen);
unsigned char      *bimsFetchText           (unsigned long int bcid,
					     int len);
int                 bimsSetKeyMap           (unsigned long int bcid,
					     int keymap);

int                 bimsQueryState          (unsigned long int bcid);
int                 bimsQueryPos            (unsigned long int bcid);
int                *bimsQueryYinSeg         (unsigned long int bcid);
unsigned char      *bimsQueryInternalText   (unsigned long int bcid);
unsigned char      *bimsQueryZuYinString    (unsigned long int bcid);
unsigned char      *bimsQueryLastZuYinString(unsigned long int bcid);
int                 bimsQuerySelectionNumber(unsigned long int bcid);
unsigned char     **bimsQuerySelectionText  (unsigned long int bcid);
int                 bimsQuerySelectionBase  (unsigned long int bcid);

#endif /* __BIMS_H__ */
