/*
 * Copyright (c) 1999
 *      Computer Systems and Communication Lab,
 *      Institute of Information Science, Academia Sinica.
 *      All rights reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the
 *      Computer Systems and Communication Lab,
 *      Insitute of Information Science, Academia Sinica
 *      and its contributors.
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: gtkentry_glue.c,v 1.1 2000/12/09 09:14:17 thhsieh Exp $
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "bims.h"

int
bimsGTKEntryZhiSelection(widget, key, num, base, selection)
GtkWidget *widget;
GdkEventKey *key;
int num, *base;
gchar *selection;
{
#define ZHI_SELECTION_BUFSIZE 42
  int i;
  unsigned char buf[ZHI_SELECTION_BUFSIZE], tmp[20];

  if (key) {
    switch(key->keyval) {
    case GDK_Escape:
      return(11);
    case GDK_less:
      if (*base - 10 >= 0) {
	*base -= 10;
      }
      break;
    case GDK_greater:
      if (*base + 10 <= num) {
	*base += 10;
      }
      break;
    case GDK_0:
    case GDK_semicolon:
      return(10);
    case GDK_1:
    case GDK_a:
      return(1);
    case GDK_2:
    case GDK_s:
      return(2);
    case GDK_3:
    case GDK_d:
      return(3);
    case GDK_4:
    case GDK_f:
      return(4);
    case GDK_5:
    case GDK_g:
      return(5);
    case GDK_6:
    case GDK_h:
      return(6);
    case GDK_7:
    case GDK_j:
      return(7);
    case GDK_8:
    case GDK_k:
      return(8);
    case GDK_9:
    case GDK_l:
      return(9);
    default:
      break;
    }
  }

  memset(buf, ' ', sizeof(unsigned char)*ZHI_SELECTION_BUFSIZE);
  if (*base - 10 >= 0) {
    buf[0] = '<';
  }
  else {
    buf[0] = ' ';
  }
  for (i = 0; i < 10 && i+*base < num; i++) {
    if (i != 9) {
      sprintf(tmp, " %d", i+1);
    }
    else {
      sprintf(tmp, " 0");
    }
    strncpy(buf+sizeof(unsigned char)*(i*4+1), tmp, 2);

    strncpy(tmp, selection+(*base+i)*2, 2);
    tmp[2] = (unsigned char)NULL;
    strncpy(buf+sizeof(unsigned char)*(i*4+2+1), tmp, 2);
  }
  if (*base + 10 <= num) {
    buf[41] = '>';
  }
  else {
    buf[41] = ' ';
  }
  buf[42] = (unsigned char)NULL;

  gtk_entry_set_text(GTK_ENTRY(widget), buf);
  gtk_entry_set_position(GTK_ENTRY(widget), 0);

  return(0);
}

int
bimsGTKEntryFeedKey(widget, key)
GtkWidget *widget;
GdkEventKey *key;
{
  int state, rval, len, yinpos;
  unsigned long int bcid;
  gchar *entry_text, *itext, *zstring;
  gchar *selection, *result;
  int num_selection, base;

  g_return_val_if_fail(GTK_IS_ENTRY(widget), FALSE);
  bcid = (unsigned long int)widget;

  if (key->keyval == GDK_Up) {
    rval = bimsToggleTsiSelection(bcid);
    if (rval == BC_VAL_IGNORE) {
      return(TRUE);
    }
  }

  gtk_entry_set_text(GTK_ENTRY(widget), "");

  // bimsSetMaxLen(bcid, 5);
  bimsSetKeyMap(bcid, BC_KEYMAP_HSU);

  state = bimsQueryState(bcid);
  switch(state) {
  case BC_STATE_EDITING:
    rval = bimsFeedKey(bcid, (KeySym)key->keyval);
    switch (rval) {
    case BC_VAL_COMMIT:
      result = bimsFetchText(bcid, 1);
      printf("%s\n", result);
    case BC_VAL_ABSORB:
      yinpos = bimsQueryPos(bcid);
      itext = bimsQueryInternalText(bcid);
      zstring = bimsQueryZuYinString(bcid);
      
      len = strlen(itext) + strlen(zstring);
      entry_text = (gchar *)malloc(sizeof(gchar)*(len+1));
      memset(entry_text, 0, len+1);
      strncpy(entry_text, itext, sizeof(gchar)*yinpos*2);
      entry_text[yinpos*2] = (gchar)NULL;
      strcat(entry_text, zstring);
      strncpy(entry_text+sizeof(gchar)*(yinpos*2+strlen(zstring)),
	      itext+sizeof(gchar)*yinpos*2,
	      sizeof(gchar)*(strlen(itext)-yinpos*2));
      entry_text[len] = (gchar)NULL;

      gtk_entry_set_text(GTK_ENTRY(widget), entry_text);
      gtk_entry_set_position(GTK_ENTRY(widget), yinpos);
      free(itext);
      free(zstring);
      free(entry_text);
      return(TRUE);
    default:
      return(TRUE);
    }
    break;
  case BC_STATE_SELECTION_TSI:
    selection = bimsQuerySelectionText(bcid);
    num_selection = bimsQuerySelectionNumber(bcid);
    base = bimsQuerySelectionBase(bcid);
    rval = bimsGTKEntryZhiSelection(widget, key,
				    num_selection, &base, selection);
    bimsSetSelectionBase(bcid, base);
    if (!rval) {
      return(TRUE);
    }
    else {
      if (rval < 11) {
	bimsPindownByNumber(bcid, rval+base);
      }
      bimsToggleEditing(bcid);
      yinpos = bimsQueryPos(bcid);
      itext = bimsQueryInternalText(bcid);
      gtk_entry_set_text(GTK_ENTRY(widget), itext);
      gtk_entry_set_position(GTK_ENTRY(widget), yinpos);
      free(itext);
      return(FALSE);
    }
    break;
  default:
    break;
  }

  return(TRUE);
}
