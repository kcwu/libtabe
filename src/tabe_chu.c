/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: tabe_chu.c,v 1.1 2000/12/09 09:14:11 thhsieh Exp $
 *
 */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tabe.h"

static void
tabeChunkInfoFree(struct ChunkInfo *chunk)
{
  int i;

  if (chunk->chunk) {
    free(chunk->chunk);
  }

  for (i = 0; i < chunk->num_tsi; i++) {
    if ((chunk->tsi+i)->yindata) {
      free((chunk->tsi+i)->yindata);
    }
    free((chunk->tsi+i)->tsi);
  }

  free(chunk->tsi);
}

/*
 * find chunk from string
 *
 * chunk stores in `chunk'
 *
 * returns the end of first chunk if success, NULL if failed
 */
static ZhiStr
tabeChunkGet(ZhiStr string, ZhiStr *chunk)
{
  int i, len, rval;
  ZhiCode code;
  int start;

  len = strlen((char *)string);
  if (len == 0) {
    return(NULL);
  }
  start = -1;

  for (i = 0; i < len;) {
    code = tabeZhiToZhiCode(string+i);
    /* we use this function to decide whether it's a Big5 char */
    rval = tabeZhiCodeToPackedBig5Code(code);
    if (rval < 0) {
      if (start >= 0) { /* a chunk terminates */
	break;
      }
      if (tabeZhiIsBig5Code(string+i) == TRUE) {
        i += 2;
      }
      else {
        i++;
      }
    }
    else {
      if (start < 0) { /* a chunk starts */
	start = i;
      }
      i += 2;
    }
  }

  if (start >= 0) {
    *chunk = (ZhiStr)malloc(sizeof(unsigned char)*((i-start)+1));
    memset(*chunk, 0, sizeof(unsigned char)*((i-start)+1));
    strncpy((char *)*chunk, (char *)string+start, i-start);
    return(string+i);
  }
  else {
    return(NULL);
  }
}

/*
 * convert a string of Big5 characters to Big5 chunk.
 *
 * return 0 if success, -1 if failed.
 *
 */
int
tabeChuInfoToChunkInfo(struct ChuInfo *chu)
{
  int i;
  ZhiStr p, c;

  if (chu->num_chunk) {
    for (i = 0; i < chu->num_chunk; i++) {
      tabeChunkInfoFree(chu->chunk+i);
    }
    free(chu->chunk);
    chu->num_chunk = 0;
    chu->chunk = (struct ChunkInfo *)NULL;
  }

  p = chu->chu;

  while(1) {
    p = tabeChunkGet(p, &c);
    if (!p) {
      break;
    }
    else {
      chu->chunk = (struct ChunkInfo *)
	realloc(chu->chunk, sizeof(struct ChunkInfo)*(chu->num_chunk+1));
      (chu->chunk[chu->num_chunk]).chunk = c;
      (chu->chunk[chu->num_chunk]).num_tsi = 0;
      (chu->chunk[chu->num_chunk]).tsi = (struct TsiInfo *)NULL;
      chu->num_chunk++;
    }
  }

  return(0);
}