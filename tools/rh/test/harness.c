// Headless mGBA runner: harness ROM script [savefile]
// script lines: "wait N" | "press KEYS N" (hold keys N frames then release 1 frame) | "shot file.png" | "savestate f" | "loadstate f"
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/core/log.h>
#include <mgba-util/vfs.h>
#include <png.h>
#include <mgba/internal/arm/arm.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static struct mCore* core; static color_t* buf; static unsigned W,H;
static void nolog(struct mLogger* l,int c,enum mLogLevel lv,const char* f,va_list a){}
static struct mLogger logger={.log=nolog};
static void shot(const char* fn){
  FILE* f=fopen(fn,"wb"); png_structp p=png_create_write_struct(PNG_LIBPNG_VER_STRING,0,0,0); png_infop i=png_create_info_struct(p);
  png_init_io(p,f); png_set_IHDR(p,i,W,H,8,PNG_COLOR_TYPE_RGB,0,0,0); png_write_info(p,i);
  unsigned char row[W*3];
  for(unsigned y=0;y<H;y++){for(unsigned x=0;x<W;x++){uint32_t c=buf[y*256+x]; row[x*3]=c&0xff;row[x*3+1]=(c>>8)&0xff;row[x*3+2]=(c>>16)&0xff;} png_write_row(p,row);}
  png_write_end(p,i); png_destroy_write_struct(&p,&i); fclose(f);
}
static int keymask(const char* s){int m=0;for(;*s;s++){switch(*s){case 'A':m|=1;break;case 'B':m|=2;break;case 's':m|=4;break;case 'S':m|=8;break;case 'R':m|=16;break;case 'L':m|=32;break;case 'U':m|=64;break;case 'D':m|=128;break;case 'r':m|=256;break;case 'l':m|=512;break;}}return m;}
int main(int argc,char**argv){
  mLogSetDefaultLogger(&logger);
  core=mCoreFind(argv[1]); core->init(core); mCoreInitConfig(core,NULL);
  core->desiredVideoDimensions(core,&W,&H); buf=calloc(256*256,sizeof(color_t)); core->setVideoBuffer(core,buf,256);
  mCoreLoadFile(core,argv[1]);
  if(argc>3){struct VFile* sv=VFileOpen(argv[3],O_CREAT|O_RDWR); core->loadSave(core,sv);}
  core->reset(core);
  FILE* sc=fopen(argv[2],"r"); char line[512];
  while(fgets(line,sizeof line,sc)){
    char cmd[32],a[256],b3[64]; int n=0; a[0]=0; b3[0]=0; int k=sscanf(line,"%31s %255s %63s",cmd,a,b3); n=strtol(b3,0,!strcmp(cmd,"peekp")?16:10);
    if(k<1||cmd[0]=='#') continue;
    if(!strcmp(cmd,"wait")){int f=atoi(a);for(int i=0;i<f;i++)core->runFrame(core);}
    else if(!strcmp(cmd,"press")){int m=keymask(a);if(n<=0)n=4;core->setKeys(core,m);for(int i=0;i<n;i++)core->runFrame(core);core->setKeys(core,0);for(int i=0;i<6;i++)core->runFrame(core);}
    else if(!strcmp(cmd,"repeat")){ /* repeat KEYS count : press each with 20 frame gap */ int m=keymask(a);for(int j=0;j<n;j++){core->setKeys(core,m);for(int i=0;i<4;i++)core->runFrame(core);core->setKeys(core,0);for(int i=0;i<16;i++)core->runFrame(core);}}
    else if(!strcmp(cmd,"prof")){ /* prof STEPS: sample PC every 997 instructions, print raw samples */
      long st=atol(a); struct ARMCore* cpu=core->cpu; FILE* o=fopen("prof.txt","w");
      for(long i=0;i<st;i++){core->step(core); if(i%997==0) fprintf(o,"%08x\n",cpu->gprs[15]);} fclose(o);}
    else if(!strcmp(cmd,"shot")) shot(a);
    else if(!strcmp(cmd,"peek32")){unsigned long ad=strtoul(a,0,16);printf("%08lx=%08x\n",ad,core->busRead32(core,ad));}
    else if(!strcmp(cmd,"dumpstr")){ /* dumpstr ADDR(hex) MAXLEN : print ASCII until NUL */ unsigned long ad=strtoul(a,0,16);int mx=atoi(b3);for(int i=0;i<mx;i++){int c=core->busRead8(core,ad+i);if(!c)break;putchar(c);}putchar('\n');fflush(stdout);}
    else if(!strcmp(cmd,"dump")){ /* dump ADDR(hex) LEN FILE : raw bytes; ADDR may be *PTR to deref */ unsigned long ad; if(a[0]=='*'){ad=core->busRead32(core,strtoul(a+1,0,16));} else ad=strtoul(a,0,16); int len=atoi(b3); char fn[256]; sscanf(line,"%*s %*s %*s %255s",fn); FILE* o=fopen(fn,"wb"); for(int i=0;i<len;i++) fputc(core->busRead8(core,ad+i),o); fclose(o);}
    else if(!strcmp(cmd,"poke8")){ unsigned long ad; if(a[0]=='*'){ad=core->busRead32(core,strtoul(a+1,0,16));} else ad=strtoul(a,0,16); int off=0, v=0; sscanf(line,"%*s %*s %i %i",&off,&v); core->busWrite8(core,ad+off,v);}
    else if(!strcmp(cmd,"peek16")){unsigned long ad=strtoul(a,0,16);printf("%08lx=%04x\n",ad,core->busRead16(core,ad));}
    else if(!strcmp(cmd,"peekp")){ /* peekp ptraddr offset(n=hex) */ unsigned long ad=strtoul(a,0,16);uint32_t p=core->busRead32(core,ad);printf("*%08lx=%08x +%x -> %04x\n",ad,p,n,core->busRead16(core,p+n));}
    else if(!strcmp(cmd,"savestate")){struct VFile* v=VFileOpen(a,O_CREAT|O_TRUNC|O_RDWR);mCoreSaveStateNamed(core,v,SAVESTATE_SAVEDATA);v->close(v);}
    else if(!strcmp(cmd,"loadstate")){struct VFile* v=VFileOpen(a,O_RDONLY);mCoreLoadStateNamed(core,v,SAVESTATE_SAVEDATA);v->close(v);}
  }
  core->deinit(core); return 0;
}
