/*
 * Integrity check for the pair of tsi and tsiyin DBs.
 *
 * Contributed by Kuang-che Wu <kcwu@ck.tp.edu.tw>
 *
 * $Id: tsiyincheck.c,v 1.2 2001/09/20 00:30:25 thhsieh Exp $
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <tabe.h>

void usage(void)
{
  printf(
      "Usage: tsiyincheck -d <TsiDB> -y <YinDB> [-f input file]\n"
      "   -d <TsiDB>      \t path to TsiDB\n"
      "   -y <YinDB>      \t path to YinDB\n"
      "   -f <output file>\t output file (default: stdout)\n"
      );
  exit(0);
}

char *YinSeqToZuYin(Yin yin[],int n,int len)
{
  int i,j;
  static char str[1024];
  str[0]=0;
  for(i=0;i<n;i++) {
    for(j=0;j<len;j++) {
      if(yin[i*len+j]==0)
        strcat(str,".");
      else {
        ZhiStr symbol=tabeYinToZuYinSymbolSequence(yin[i*len+j]);
        strcat(str,(char *)symbol);
        free(symbol);
      }
      if(j!=len-1)
        strcat(str," ");
    }
    if(i!=n-1)
      strcat(str," ");
  }
  return str;
}

int isCMEXyin(struct TsiDB *db, Zhi zhi, Yin yin, char **zuyin)
{
  int i;
  struct TsiInfo zhiinfo;

  if(zuyin) *zuyin="";

  zhiinfo.tsi=zhi;
  if (tabeTsiInfoLookupZhiYin(db,&zhiinfo) < 0)
    return 0;

  if(zuyin)
    *zuyin=YinSeqToZuYin(zhiinfo.yindata,1,zhiinfo.yinnum);

  for(i=0;i<zhiinfo.yinnum;i++)
    if(zhiinfo.yindata[i]==yin)
      return 1;
  return 0;
}

int isDByin(struct TsiDB *db, Zhi zhi, Yin yin, char **zuyin)
{
  int i;
  struct TsiInfo tsi;
  int found=0;
 
  if(zuyin) *zuyin=""; 
  if(yin==0) return 0;

  memset(&tsi,0,sizeof(tsi));
  tsi.tsi=(ZhiStr)calloc(1,1024);
  strncpy((char *)tsi.tsi,(char *)zhi,2);
  tsi.tsi[2]=0;
  db->Get(db,&tsi);

  for(i=0;!found && i<tsi.yinnum;i++)
    if(tsi.yindata[i]==yin)
      found=1;
  if(zuyin)
    *zuyin=YinSeqToZuYin(tsi.yindata,tsi.yinnum,1);

  free(tsi.tsi);
  if(tsi.yindata)
    free(tsi.yindata);
  return found;
}

/* *** 詞中的字音, 有不屬於 CMEX 所列的音 */
void check_cmex_yin(struct TsiDB *db, struct TsiYinDB *yindb, FILE *fp)
{
  int i,j;
  int rval;
  struct TsiInfo tsi;
  char *zuyin;

  memset(&tsi,0,sizeof(struct TsiInfo));
  tsi.tsi=(ZhiStr)calloc(1,1024);

  for(rval=db->CursorSet(db,&tsi);rval>=0;rval=db->CursorNext(db,&tsi)) {
    int len=strlen((char *)tsi.tsi)/2;
    for(i=0;i<tsi.yinnum;i++)
      for(j=0;j<len;j++) {
        Yin yin=tsi.yindata[i*len+j];
	if(!isCMEXyin(db,tsi.tsi+j*2,yin,&zuyin)) {
          ZhiStr symbol=tabeYinToZuYinSymbolSequence(yin);
 	  fprintf(fp,"%s:%c%c:",tsi.tsi,tsi.tsi[j*2],tsi.tsi[j*2+1]);
	  fprintf(fp,"%s:%s\n",(yin?(char *)symbol:"(yin=0)"),zuyin);
          free(symbol);
	}
      }
  }
  if(tsi.yindata)
    free(tsi.yindata);
  free(tsi.tsi);
}

/* *** 詞中的字音, 未出現於該字的注音列表 */
void check_consistency(struct TsiDB *db, struct TsiYinDB *yindb, FILE *fp)
{
  int i,j;
  int rval;
  struct TsiInfo tsi;
  char *zuyin;

  memset(&tsi,0,sizeof(struct TsiInfo));
  tsi.tsi=(ZhiStr)calloc(1,1024);

  for(rval=db->CursorSet(db,&tsi);rval>=0;rval=db->CursorNext(db,&tsi)) {
    int len=strlen((char *)tsi.tsi)/2;
    for(i=0;i<tsi.yinnum;i++)
      for(j=0;j<len;j++) {
        Yin yin=tsi.yindata[i*len+j];
	if(!isDByin(db,tsi.tsi+j*2,yin,&zuyin)) {
          ZhiStr symbol=tabeYinToZuYinSymbolSequence(yin);
	  fprintf(fp,"%s:%c%c:",tsi.tsi,tsi.tsi[j*2],tsi.tsi[j*2+1]);
	  fprintf(fp,"%s:%s\n",(yin?(char *)symbol:"(yin=0)"),zuyin);
          free(symbol);
	}
      }
  }
  if(tsi.yindata)
    free(tsi.yindata);
  free(tsi.tsi);
}

/* *** 三字以上, 與其他詞注音相同, 但其中一字不同 */
void check_maybe_tsi_typo(struct TsiDB *db, struct TsiYinDB *yindb, FILE *fp)
{
  int i,j,k;
  int rval;
  struct TsiInfo tsi;
  struct TsiYinInfo yin;
  static char same1[]="台,証,菸,汙,濕,沉,辭";
  static char same2[]="臺,證,煙,污,溼,沈,詞";

  memset(&tsi,0,sizeof(struct TsiInfo));
  tsi.tsi=(ZhiStr)calloc(1,1024);
  memset(&yin,0,sizeof(yin));

  for(rval=db->CursorSet(db,&tsi);rval>=0;rval=db->CursorNext(db,&tsi)) {
    int len=strlen((char *)tsi.tsi)/2;
    if(len<3) continue;

    for(i=0;i<tsi.yinnum;i++) {
      yin.yin=tsi.yindata+len*i;
      yin.yinlen=len;
      yindb->Get(yindb,&yin);

      for(j=0;j<yin.tsinum;j++) {
	ZhiStr tsi2=yin.tsidata+len*j*2;
	/* only check tsi1<tsi2 */
	if(strncmp((char *)tsi.tsi,(char *)tsi2,len*2)<0) { 
	  int diff=0;
	  int pos=-1;
	  for(k=0;k<len;k++)
	    if(strncmp((char *)tsi.tsi+k*2,(char *)tsi2+k*2,2)) {
	      diff++;
	      pos=k;
	    }
	  if(diff==1) {
	    char str[80];
            char word[2][3];
            strncpy(word[0],(char *)tsi.tsi+pos*2,2);
            word[0][2]=0;
            strncpy(word[1],(char *)tsi2+pos*2,2);
            word[1][2]=0;
            if(strstr(same1,word[0])-same1==strstr(same2,word[1])-same2 ||
               strstr(same1,word[1])-same1==strstr(same2,word[0])-same2)
              continue;
	    strncpy(str,(char *)tsi2,len*2);
	    str[len*2]=0;
	    fprintf(fp,"%s:%s\n%s\n",tsi.tsi,YinSeqToZuYin(yin.yin,1,len),str);
	  }
	}
      }
    }
  }
  if(yin.tsidata)
    free(yin.tsidata);
  if(tsi.yindata)
    free(tsi.yindata);
  free(tsi.tsi);
}

void check(struct TsiDB *db, struct TsiYinDB *yindb, FILE *fp)
{
  fprintf(fp,"*** 詞中的字音, 有不屬於 CMEX 所列的音\n");
  check_cmex_yin(db,yindb,fp);
  
  fprintf(fp,"*** 詞中的字音, 未出現於該字的注音列表\n");
  check_consistency(db,yindb,fp);

  fprintf(fp,"*** 三字以上, 與其他詞注音相同, 但其中一字不同\n");
  check_maybe_tsi_typo(db,yindb,fp);
}

int main(int argc, char *argv[])
{
  int ch;
  struct TsiDB *db;
  struct TsiYinDB *yindb;

  extern char *optarg;
  extern int optind, opterr, optopt;

  char *db_name, *op_name, *yindb_name;

  db_name = op_name = yindb_name = NULL;

  while ((ch = getopt(argc, argv, "d:f:y:")) != -1) {
    switch(ch) {
      case 'd':
        db_name = (char *)strdup(optarg);
        break;
      case 'f':
        op_name = (char *)strdup(optarg);
        break;
      case 'y':
	yindb_name = (char *)strdup(optarg);
	break;
      default:
        usage();
        break;
    }
  }
  argc -= optind;
  argv += optind;

  if(!db_name || !yindb_name)
    usage();
  db = tabeTsiDBOpen(DB_TYPE_DB, db_name, 0);
  yindb = tabeTsiYinDBOpen(DB_TYPE_DB, yindb_name, 0);
  if (!yindb || !db) 
    usage();

  if(op_name) {
    FILE *fp=fopen(op_name,"w");
    check(db,yindb,fp);
    fclose(fp);
  } else 
    check(db,yindb,stdout);
  db->Close(db);
  yindb->Close(yindb);
  return 0;
}
