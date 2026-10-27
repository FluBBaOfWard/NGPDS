"""Compile the actual config validators/loadSettings body with filesystem-only stubs.
No implementation is re-created: function bodies are extracted from FileHandling.c.
"""
from pathlib import Path
import sys,tempfile,subprocess
source=Path(sys.argv[1] if len(sys.argv)>1 else Path(__file__).resolve().parents[1]).resolve();s=(source/'source/FileHandling.c').read_text();a=s.index('static bool configDataValid(') if 'static bool configDataValid(' in s else s.index('static bool configValid(');validators=s[a:s.index('static bool configEqual(',a)];a=s.index('int loadSettings()');loader=s[a:s.index('int saveSettings()',a)]
code=r'''
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#define u8 uint8_t
#define FILEPATH_MAX_LENGTH 256
#define AUTOSAVE_SETTINGS 2
#define AUTOSAVE_NVRAM 16
ConfigData cfg;
static bool settingsLoadBlocked;
static unsigned emuSettings, applied, opens, mutation;
static bool storageEnter(char *p) {return getcwd(p,256)!=NULL;}
static void applyConfigData(void) {emuSettings=cfg.emuSettings;applied++;}
static void infoOutput(const char *s) {(void)s;}
static FILE *checkedOpen(const char *p,const char *mode) {
 if (++opens==2 && mutation) {
  if (mutation==1) {FILE *f=fopen(p,"rb+");assert(f);fputc('X',f);fclose(f);}
  if (mutation==2) {FILE *f=fopen(p,"ab");assert(f);fputc(0,f);fclose(f);}
  if (mutation==3) assert(!truncate(p,7));
 }
 return fopen(p,mode);
}
#define fopen checkedOpen
'''
# ConfigData remains the original project's packed layout, not a mirrored test struct.
prefix='#include "'+str(source/'source/Emubase.h')+'"\n'
code=code.replace('ConfigData cfg;',prefix+'ConfigData cfg;');code+='\n#include "'+str(source/'source/SafeFile.c')+'"\n'+validators+'\nstatic const char *settingName="settings.cfg";\n'+loader+'\n#undef fopen\n'
code+=r'''
static void fixture(void) {
 ConfigData c={0};memcpy(c.magic,"cfg",4);c.emuSettings=0x20309;c.gammaValue=1;c.language=1;strcpy(c.currentPath,"/");strcpy(c.biosPathColor,"/ngpc_bios.bin");
 FILE*f=fopen(settingName,"wb");assert(f&&fwrite(&c,1,sizeof(c),f)==sizeof(c)&&!fclose(f));
 memset(&cfg,0,sizeof(cfg));strcpy(cfg.biosPathColor,"/initial");settingsLoadBlocked=false;emuSettings=0x2031b;opens=applied=mutation=0;
}
int main(void) {
 char d[]="/tmp/ngpds-config-XXXXXX";assert(mkdtemp(d)&&!chdir(d));assert(sizeof(ConfigData)==792);
 fixture();assert(!loadSettings());assert(applied==1&&emuSettings==0x20309&&!strcmp(cfg.biosPathColor,"/ngpc_bios.bin"));
 for(unsigned n=1;n<=3;n++) {fixture();mutation=n;int result=loadSettings();if(result!=1||applied||!settingsLoadBlocked||(emuSettings&0x12)) {fprintf(stderr,"FAIL mutation=%u result=%d applied=%u flags=%x blocked=%d\n",n,result,applied,emuSettings,settingsLoadBlocked);return 1;}assert(!strcmp(cfg.biosPathColor,"/initial"));}
 remove(settingName);chdir("/");assert(!rmdir(d));puts("PASS actual loadSettings: legacy flags/BIOS preserved; late bad magic, trailing bytes and truncation rejected before apply; autosave disabled on read inconsistency");return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='ngpds-config-build-') as d:
 p=Path(d);(p/'test.c').write_text(code);subprocess.run(['clang','-O1','-g','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
