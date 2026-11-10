#include "catalog.h"
#include <sqlite3.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <shlwapi.h>
#include <stdio.h>
static sqlite3 *db;
static char *to_utf8(const wchar_t *s){int n=WideCharToMultiByte(CP_UTF8,0,s,-1,NULL,0,NULL,NULL);if(!n)return NULL;char *p=(char*)malloc((size_t)n);if(p)WideCharToMultiByte(CP_UTF8,0,s,-1,p,n,NULL,NULL);return p;}
static wchar_t *to_wide(const unsigned char *s){if(!s)return _wcsdup(L"");int n=MultiByteToWideChar(CP_UTF8,0,(const char*)s,-1,NULL,0);if(!n)return _wcsdup(L"");wchar_t *p=(wchar_t*)malloc((size_t)n*sizeof(wchar_t));if(p)MultiByteToWideChar(CP_UTF8,0,(const char*)s,-1,p,n);return p;}
static int sql(const char *s){return sqlite3_exec(db,s,NULL,NULL,NULL)==SQLITE_OK;}
int catalog_open(void){
 wchar_t dir[MAX_PATH],file[MAX_PATH];if(!GetEnvironmentVariableW(L"LOCALAPPDATA",dir,MAX_PATH))return 0;
 PathAppendW(dir,L"TagExplorer");CreateDirectoryW(dir,NULL);lstrcpynW(file,dir,MAX_PATH);PathAppendW(file,L"catalog.db");
 char *p=to_utf8(file);if(!p)return 0;int rc=sqlite3_open_v2(p,&db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,NULL);free(p);if(rc!=SQLITE_OK)return 0;
 sqlite3_busy_timeout(db,3000);
 if(!sql("PRAGMA journal_mode=WAL;PRAGMA synchronous=NORMAL;PRAGMA temp_store=MEMORY;PRAGMA cache_size=-16000;PRAGMA foreign_keys=ON;"
 "CREATE TABLE IF NOT EXISTS Images(Path TEXT PRIMARY KEY COLLATE NOCASE);"
 "CREATE TABLE IF NOT EXISTS Tags(Id INTEGER PRIMARY KEY,Name TEXT NOT NULL COLLATE NOCASE UNIQUE,Color TEXT NOT NULL DEFAULT '#5B8DEF');"
 "CREATE TABLE IF NOT EXISTS PhotoTags(PhotoPath TEXT NOT NULL COLLATE NOCASE,TagId INTEGER NOT NULL REFERENCES Tags(Id) ON DELETE CASCADE,PRIMARY KEY(PhotoPath,TagId));"
 "CREATE INDEX IF NOT EXISTS IX_PhotoTags_TagId ON PhotoTags(TagId);"
 "INSERT OR IGNORE INTO Images(Path) SELECT DISTINCT PhotoPath FROM PhotoTags;"))return 0;
 if(!sql("BEGIN IMMEDIATE;"
 "INSERT OR IGNORE INTO Tags(Name) "
 "SELECT DISTINCT 'Month: ' || substr(y.Name,7,4) || '-' || "
 "CASE lower(substr(m.Name,8)) "
 "WHEN 'january' THEN '01' WHEN 'february' THEN '02' WHEN 'march' THEN '03' "
 "WHEN 'april' THEN '04' WHEN 'may' THEN '05' WHEN 'june' THEN '06' "
 "WHEN 'july' THEN '07' WHEN 'august' THEN '08' WHEN 'september' THEN '09' "
 "WHEN 'october' THEN '10' WHEN 'november' THEN '11' WHEN 'december' THEN '12' END "
 "FROM PhotoTags pm JOIN Tags m ON m.Id=pm.TagId "
 "JOIN PhotoTags py ON py.PhotoPath=pm.PhotoPath JOIN Tags y ON y.Id=py.TagId "
 "WHERE m.Name IN ('Month: January','Month: February','Month: March','Month: April','Month: May','Month: June','Month: July','Month: August','Month: September','Month: October','Month: November','Month: December') "
 "AND y.Name GLOB 'Year: [0-9][0-9][0-9][0-9]';"
 "INSERT OR IGNORE INTO PhotoTags(PhotoPath,TagId) "
 "SELECT pm.PhotoPath,nt.Id FROM PhotoTags pm JOIN Tags m ON m.Id=pm.TagId "
 "JOIN PhotoTags py ON py.PhotoPath=pm.PhotoPath JOIN Tags y ON y.Id=py.TagId "
 "JOIN Tags nt ON nt.Name='Month: ' || substr(y.Name,7,4) || '-' || "
 "CASE lower(substr(m.Name,8)) "
 "WHEN 'january' THEN '01' WHEN 'february' THEN '02' WHEN 'march' THEN '03' "
 "WHEN 'april' THEN '04' WHEN 'may' THEN '05' WHEN 'june' THEN '06' "
 "WHEN 'july' THEN '07' WHEN 'august' THEN '08' WHEN 'september' THEN '09' "
 "WHEN 'october' THEN '10' WHEN 'november' THEN '11' WHEN 'december' THEN '12' END "
 "WHERE m.Name IN ('Month: January','Month: February','Month: March','Month: April','Month: May','Month: June','Month: July','Month: August','Month: September','Month: October','Month: November','Month: December') "
 "AND y.Name GLOB 'Year: [0-9][0-9][0-9][0-9]';"
 "DELETE FROM PhotoTags WHERE TagId IN (SELECT Id FROM Tags WHERE Name IN "
 "('Month: January','Month: February','Month: March','Month: April','Month: May','Month: June','Month: July','Month: August','Month: September','Month: October','Month: November','Month: December')) "
 "AND EXISTS(SELECT 1 FROM PhotoTags py JOIN Tags y ON y.Id=py.TagId "
 "WHERE py.PhotoPath=PhotoTags.PhotoPath AND y.Name GLOB 'Year: [0-9][0-9][0-9][0-9]');"
 "DELETE FROM Tags WHERE Name IN ('Month: January','Month: February','Month: March','Month: April','Month: May','Month: June','Month: July','Month: August','Month: September','Month: October','Month: November','Month: December') "
 "AND NOT EXISTS(SELECT 1 FROM PhotoTags WHERE TagId=Tags.Id);"
 "COMMIT;")){sql("ROLLBACK");return 0;}
 return 1;
}
void catalog_close(void){if(db){sqlite3_close(db);db=NULL;}}
int catalog_add(const wchar_t *path){
 char *p=to_utf8(path);if(!p)return 0;sqlite3_stmt *s=NULL;int ok=sqlite3_prepare_v2(db,"INSERT OR IGNORE INTO Images(Path) VALUES(?1)",-1,&s,NULL)==SQLITE_OK;
 if(ok){sqlite3_bind_text(s,1,p,-1,SQLITE_TRANSIENT);ok=sqlite3_step(s)==SQLITE_DONE;}sqlite3_finalize(s);free(p);return ok;
}
int catalog_rename(const wchar_t *old_path,const wchar_t *new_path){
 char *oldp=to_utf8(old_path),*newp=to_utf8(new_path);
 if(!oldp||!newp){free(oldp);free(newp);return 0;}
 int ok=sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL)==SQLITE_OK;
 sqlite3_stmt *s=NULL;
 if(ok){
  ok=sqlite3_prepare_v2(db,"UPDATE Images SET Path=?1 WHERE Path=?2 COLLATE NOCASE",-1,&s,NULL)==SQLITE_OK;
  if(ok){sqlite3_bind_text(s,1,newp,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,oldp,-1,SQLITE_TRANSIENT);ok=sqlite3_step(s)==SQLITE_DONE;}
  sqlite3_finalize(s);s=NULL;
 }
 if(ok){
  ok=sqlite3_prepare_v2(db,"UPDATE PhotoTags SET PhotoPath=?1 WHERE PhotoPath=?2 COLLATE NOCASE",-1,&s,NULL)==SQLITE_OK;
  if(ok){sqlite3_bind_text(s,1,newp,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,oldp,-1,SQLITE_TRANSIENT);ok=sqlite3_step(s)==SQLITE_DONE;}
  sqlite3_finalize(s);
 }
 sqlite3_exec(db,ok?"COMMIT":"ROLLBACK",NULL,NULL,NULL);
 free(oldp);free(newp);return ok;
}
static int supported(const wchar_t *p){const wchar_t *e=PathFindExtensionW(p);return !_wcsicmp(e,L".jpg")||!_wcsicmp(e,L".jpeg")||!_wcsicmp(e,L".png")||!_wcsicmp(e,L".bmp")||!_wcsicmp(e,L".gif")||!_wcsicmp(e,L".tif")||!_wcsicmp(e,L".tiff")||!_wcsicmp(e,L".ico")||!_wcsicmp(e,L".wdp")||!_wcsicmp(e,L".heic")||!_wcsicmp(e,L".webp");}
static int walk(const wchar_t *folder,int depth){
 if(depth>64)return 0;wchar_t pattern[MAX_PATH];lstrcpynW(pattern,folder,MAX_PATH);PathAppendW(pattern,L"*");WIN32_FIND_DATAW fd;HANDLE h=FindFirstFileW(pattern,&fd);if(h==INVALID_HANDLE_VALUE)return 0;int n=0;
 do{if(!wcscmp(fd.cFileName,L".")||!wcscmp(fd.cFileName,L".."))continue;wchar_t p[MAX_PATH];lstrcpynW(p,folder,MAX_PATH);PathAppendW(p,fd.cFileName);
 if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(!(fd.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))n+=walk(p,depth+1);}else if(supported(p)){n+=catalog_add(p)?1:0;}
 }while(FindNextFileW(h,&fd));FindClose(h);return n;
}
int catalog_add_tree(const wchar_t *folder){return walk(folder,0);}
int catalog_apply_tags(const wchar_t *path,const wchar_t *csv){
 char *p=to_utf8(path);if(!p)return 0;catalog_add(path);sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL);
 wchar_t *copy=_wcsdup(csv),*ctx=NULL,*tok=wcstok(copy,L",;",&ctx);int count=0;
 while(tok){while(*tok==L' '||*tok==L'\t')tok++;wchar_t *end=tok+wcslen(tok);while(end>tok&&(end[-1]==L' '||end[-1]==L'\t'))*--end=0;
 if(*tok){char *t=to_utf8(tok);sqlite3_stmt *s=NULL;if(t&&sqlite3_prepare_v2(db,"INSERT OR IGNORE INTO Tags(Name) VALUES(?1)",-1,&s,NULL)==SQLITE_OK){sqlite3_bind_text(s,1,t,-1,SQLITE_TRANSIENT);sqlite3_step(s);}sqlite3_finalize(s);
 s=NULL;if(t&&sqlite3_prepare_v2(db,"INSERT OR IGNORE INTO PhotoTags(PhotoPath,TagId) SELECT ?1,Id FROM Tags WHERE Name=?2 COLLATE NOCASE",-1,&s,NULL)==SQLITE_OK){sqlite3_bind_text(s,1,p,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,t,-1,SQLITE_TRANSIENT);if(sqlite3_step(s)==SQLITE_DONE)count++;}sqlite3_finalize(s);free(t);}
 tok=wcstok(NULL,L",;",&ctx);}
 free(copy);free(p);sqlite3_exec(db,"COMMIT",NULL,NULL,NULL);return count;
}
int catalog_list_tags(wchar_t ***names,int *count){
 *names=NULL;*count=0;sqlite3_stmt *s=NULL;
 if(sqlite3_prepare_v2(db,"SELECT Name FROM Tags ORDER BY Name COLLATE NOCASE",-1,&s,NULL)!=SQLITE_OK)return 0;
 int ok=1;while(sqlite3_step(s)==SQLITE_ROW){wchar_t *name=to_wide(sqlite3_column_text(s,0));if(!name)continue;
  wchar_t **next=(wchar_t**)realloc(*names,(size_t)(*count+1)*sizeof(wchar_t*));if(!next){free(name);ok=0;break;}*names=next;(*names)[(*count)++]=name;
 }sqlite3_finalize(s);return ok;
}
int catalog_search(const wchar_t *query,const wchar_t *tag_filter,wchar_t ***paths,wchar_t ***tags,int *count){
 *paths=NULL;*tags=NULL;*count=0;char *q=to_utf8(query?query:L""),*tag=to_utf8(tag_filter?tag_filter:L"");if(!q||!tag){free(q);free(tag);return 0;}
 const char *statement="SELECT i.Path,COALESCE((SELECT group_concat(t.Name,', ') FROM Tags t JOIN PhotoTags pt ON pt.TagId=t.Id WHERE pt.PhotoPath=i.Path),'') FROM Images i WHERE (?1='' OR i.Path LIKE ?2 OR EXISTS(SELECT 1 FROM PhotoTags p JOIN Tags t ON t.Id=p.TagId WHERE p.PhotoPath=i.Path AND t.Name LIKE ?2)) AND (?3='' OR EXISTS(SELECT 1 FROM PhotoTags p2 JOIN Tags t2 ON t2.Id=p2.TagId WHERE p2.PhotoPath=i.Path AND t2.Name=?3 COLLATE NOCASE)) ORDER BY i.Path";
 sqlite3_stmt *s=NULL;int ok=sqlite3_prepare_v2(db,statement,-1,&s,NULL)==SQLITE_OK;int capacity=0;
 if(ok){size_t z=strlen(q)+3;char *like=(char*)malloc(z);if(!like)ok=0;else{snprintf(like,z,"%%%s%%",q);sqlite3_bind_text(s,1,q,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,2,like,-1,SQLITE_TRANSIENT);sqlite3_bind_text(s,3,tag,-1,SQLITE_TRANSIENT);free(like);
  while(sqlite3_step(s)==SQLITE_ROW){
   wchar_t *wp=to_wide(sqlite3_column_text(s,0));if(!wp)continue;
   if(GetFileAttributesW(wp)==INVALID_FILE_ATTRIBUTES){free(wp);continue;}
   if(*count==capacity){
    int next=capacity?capacity*2:256;
    wchar_t **np=(wchar_t**)malloc((size_t)next*sizeof(wchar_t*));
    wchar_t **nt=(wchar_t**)malloc((size_t)next*sizeof(wchar_t*));
    if(!np||!nt){free(np);free(nt);free(wp);ok=0;break;}
    if(*count){memcpy(np,*paths,(size_t)*count*sizeof(wchar_t*));memcpy(nt,*tags,(size_t)*count*sizeof(wchar_t*));}
    free(*paths);free(*tags);*paths=np;*tags=nt;capacity=next;
   }
   wchar_t *wt=to_wide(sqlite3_column_text(s,1));
   if(!wt){free(wp);ok=0;break;}
   (*paths)[*count]=wp;(*tags)[*count]=wt;(*count)++;
  }}
 }sqlite3_finalize(s);free(q);free(tag);
 if(!ok){catalog_free_results(*paths,*tags,*count);*paths=NULL;*tags=NULL;*count=0;}
 return ok;
}
int catalog_get_photo_tags(const wchar_t *path,wchar_t **tags){
 if(tags)*tags=NULL;if(!db||!path||!tags)return 0;
 char *p=to_utf8(path);if(!p)return 0;sqlite3_stmt *s=NULL;
 int ok=sqlite3_prepare_v2(db,"SELECT COALESCE((SELECT group_concat(t.Name, ', ') FROM Tags t JOIN PhotoTags pt ON pt.TagId=t.Id WHERE pt.PhotoPath=?1 COLLATE NOCASE),'')",-1,&s,NULL)==SQLITE_OK;
 if(ok){sqlite3_bind_text(s,1,p,-1,SQLITE_TRANSIENT);ok=sqlite3_step(s)==SQLITE_ROW;if(ok)*tags=to_wide(sqlite3_column_text(s,0));}
 sqlite3_finalize(s);free(p);return ok&&*tags!=NULL;
}
void catalog_free_results(wchar_t **p,wchar_t **t,int n){for(int i=0;i<n;i++){free(p[i]);free(t[i]);}free(p);free(t);}
