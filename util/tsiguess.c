/*
 * Copyright 2001, TaBE Project, All Rights Reserved.
 * 
 * $Id: tsiguess.c,v 1.6 2003/05/07 14:14:13 kcwu Exp $  
 * 
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "tabe.h"
#include "version.h"

#define CHU_SIZE        1024
#define CHUNK_SIZE      1048576

/* big5 variant, http://m2000.idv.tw/informer/big5 */

#define IS_BIG5HI(c) ( ((c) >= 0x81 && (c) <=0xFE) ? 1 : 0 )
                                                                                
#define IS_BIG5LO(c) ( ( ((c) >= 0x40 && (c) <=0x7E ) ||             \
                         ((c) >= 0xA1 && (c) <=0xFE )    ) ? 1 : 0 )
 
#define BIG5_TO_HEX (hi,lo) ( (hi) * 256 + (lo) )
                                                                                
#define IS_BIG5_SPACE(hex) ((hex)==0xA140)
#define IS_BIG5_SYMBOL(hex) ((((hex) >= 0xA141 && (hex) <= 0xA3BF ) ||\
                              ((hex) >= 0xF9DD && (hex) <= 0xF9FE )   \
                            ) ? 1 : 0 )
                                                                                
#define IS_BIG5_JP(hex) ( ( (hex) >=0xC6E7 && (hex) <=0xC7F2 ) ? 1 : 0 )

int verbose;

void
usage(void)
{
	printf("tsiguess: libtabe-%s\n", RELEASE_VER);
	printf("Usage: tsiguess -d <TsiDB> \n");
	printf("   -d <TsiDB>     \t path to TsiDB\n");
	printf("   -u <TsiDB>     \t path to user's TsiDB\n");
	printf("   -v             \t verbose\n");
	exit(0);
}

void 
tabeTsiInfoShow(struct TsiInfo *tsi)
{
	int i=0,j=0,len=0 ;
	ZuYinSymbolSequence zs = NULL;

	len = strlen ( (char *) tsi->tsi ) / 2 ;
	printf ("tsi->tsi:      %s\n",tsi->tsi) ;
	printf ("tsi->refcount: %li\n",tsi->refcount) ;
	printf ("tsi->yinnum:   %li\n",tsi->yinnum) ;
	
	for (i=0; i < tsi->yinnum; i++) {
		printf ("tsi->yin[%i]:   ",i);
   		for (j = 0; j < len ; j++) {
     			zs=tabeYinToZuYinSymbolSequence(tsi->yindata[i* len+j]);
       			printf( "%s ", zs);
			free(zs);
		}
		printf("\n");
	}
}

void
tabeChunkInfoShow(struct ChunkInfo *chunk)
{
	struct TsiInfo   *tsi;
	int i=0;
	
/*	printf ("[%s]<",chunk->chunk);	*/
	for (i=0 ;  i < chunk->num_tsi ; i++ ) {
		tsi = chunk->tsi+i ;
		printf("%s ",tsi->tsi) ;
	}
/*	printf (">");	*/

/*
        for (i=0 ;  i < chunk->num_tsi ; i++ ) {
                tsi = chunk->tsi+i ;
                printf("=== tsi[%d] === \n",i) ;
                tabeTsiInfoShow(tsi);
        } 
	printf("\n");
*/

}


struct ChuInfo * 
tabeChuInfoNew(char *str)
{
        struct ChuInfo *chu;
        int size=strlen(str)+1;

        if ( size == 1 ) {
		size = CHU_SIZE ;
	}

        chu = (struct ChuInfo *) malloc(sizeof(struct ChuInfo));
        memset(chu, 0, sizeof(struct ChuInfo));

        chu->chu = (char *) malloc( sizeof(unsigned char) * size );
        strcpy(chu->chu,str);

        return chu;
}

struct ChunkInfo *
tabeChunkInfoGet(FILE *stream)
{
	unsigned char str[CHUNK_SIZE];
        unsigned char *p=NULL;
        unsigned int  hex=0, status=0;

	p = str;
	*p = '\0';

        while (1) {
                if ( feof(stream) ) {
			if ( p > str ) {
                        	status = TRUE;
			}
			else {
				status = FALSE;
			}
			break;
		}
		*p=fgetc(stream) ;

                if ( IS_BIG5HI(*p) ) {
			*(p+1)=fgetc(stream);
			hex = BIG5_TO_HEX(*p,*(p+1));
			if ( !IS_BIG5LO(*(p+1)) )
				continue;
			else if ( IS_BIG5_SPACE(hex) ) {
				continue;
			} else if ( IS_BIG5_SYMBOL(hex) || IS_BIG5_JP(hex) ) {
				if ( p > str )	{
					*p = '\0';
					status = TRUE;
					break;
				}
				continue;
			}
        		else {
				p=p+2;
				continue;
			}
		}
		else { 
			if ( ( isspace(*p) == FALSE) && (p > str) ) {
				*p = '\0';
				status = TRUE;
				break;
			}
		}
		if(p-str>CHUNK_SIZE-64) {
		  *p='\0';
		  status=TRUE;
		  break;
		}
	}
	if (status == TRUE)
		return tabeChunkInfoNew(str);
	else
		return NULL;
}


int 
isprep(unsigned char *zhi) 
{
/* 
 * ref: http://www.dmpo.sinica.edu.tw:8000/~words/sou/sou.html
 */
	const char preplist[]= \
/* 連接詞 */		"並且和與及或但若" \
/* 副詞 */		"很只還皆都僅則也要就不將才" \
/* 時態詞 */		"了著" \
/* 定詞/量詞 */		"一二兩三四五六七八九十這那此本該其個杯句" \
/* 語助詞 */		"啊啦吧的得地之乎" \
/* 介詞 */		"為跟但對在於是像把" \
/* 後置詞 */		"上中下左右等間時" \
/* 及物動詞 */		"是有作做讓說" \
/* misc */		"我你妳您他她它牠祂";

	int i=0;

	for (i=0; preplist[i]; i+=2) {
                if ( strncmp(zhi, preplist+i, 2) == 0 ) 
			return TRUE;
	}
	return FALSE;
}


int
tabe_guess_newtsi (struct ChunkInfo *chunk, 
                   char *return_str, 
                   struct TsiDB *newdb )
{
	int i=0;
	unsigned char  *tsi_str;
	struct TsiInfo *tsi;		
        char buf[1024];
	int buflen=0;
	
	if (newdb == NULL)
		return 0;
	return_str[0] = '\0';

        for (i=0; i < chunk->num_tsi ; i++) {
		int tsilen;
		tsi_str = (chunk->tsi+i)->tsi ;
		tsilen = strlen(tsi_str);
                if ( tsilen == 2 && isprep(tsi_str) == FALSE ) {
			strcpy(buf+buflen,tsi_str);
			buflen+=tsilen;
					  /* 單字詞, 加到 buff */
                }

		else {		          /* 不是單字詞, 開始整理 buf */
			if ( buflen >= 4 ) {  
				if( verbose ) {
					strcat(return_str, buf); /*得到連續單字詞*/
					strcat(return_str, " ");
				}

				tsi=tabeTsiInfoNew(buf);
				newdb->Get(newdb,tsi);
				tsi->refcount++ ;
				newdb->Put(newdb,tsi);
				tabeTsiInfoDestroy(tsi);
			}
		/* else 單字詞, if 罕見, 標記 for spelling check */
			buf[0] = '\0';
			buflen=0;
		}
	}

	if ( buflen >= 4 ) { /* at the end of chunk, check again */
		if( verbose ) {
			strcat(return_str, buf);
			strcat(return_str, " ");
		}
		
		tsi=tabeTsiInfoNew(buf);
		newdb->Get(newdb,tsi);
		tsi->refcount++ ;
		newdb->Put(newdb,tsi);
		tabeTsiInfoDestroy(tsi);
	}

	return 1;
}

int
main(int argc, char **argv)
{
	int ch;	
	extern char *optarg;
	extern int optind, opterr, optopt; 

	struct TsiDB     *tsidb = NULL;
	char             *tsidb_name = NULL;
	struct TsiDB	 *newdb = NULL;
	char             *usrdb_name = NULL;
	struct ChunkInfo *chunk = NULL; 
	unsigned char    *str;

	while ((ch = getopt(argc, argv, "d:u:v")) != -1) {
    		switch(ch) {
      			case 'd':
        			tsidb_name = (char *)strdup(optarg);
        			break;
      			case 'u':
        			usrdb_name = (char *)strdup(optarg);
        			break;
			case 'v':
				verbose++;
				break;
			default:
				usage();
				break;
		}
	}

	argc -= optind;
	argv += optind;
	
	if (!tsidb_name) {
    		usage();
	}
	
	tsidb = tabeTsiDBOpen(DB_TYPE_DB, tsidb_name,
	    DB_FLAG_READONLY|DB_FLAG_NOUNPACK_YIN);
	if (!tsidb) {
  		usage();
	}

	if (usrdb_name) {
		newdb = tabeTsiDBOpen(DB_TYPE_DB, usrdb_name, 
			      DB_FLAG_CREATEDB|DB_FLAG_OVERWRITE);
	}

	str = (unsigned char *) malloc(sizeof(unsigned char) * 1024 );

	while ( !feof(stdin) ) {
        	if ( (chunk = tabeChunkInfoGet(stdin)) != NULL ) { 
			tabeChunkSegmentationComplex(tsidb,chunk);
			if (verbose)
				tabeChunkInfoShow(chunk);
			if (newdb)
				tabe_guess_newtsi(chunk, str, newdb);
			
			if ( verbose && strlen(str) > 0 ) {
				printf( "<<新詞: %s>>", str); 
				str[0] = '\0';
			}
			tabeChunkInfoDestroy(chunk);
		}
	}
	printf("\n");

	tsidb->Close(tsidb);
	if (newdb)
		newdb->Close(newdb);
	return 0;	
}
