#define COBJMACROS
#include <windows.h>
#include <shlwapi.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "icons.h"
#define NANOSVG_IMPLEMENTATION
#include "../third_party/nanosvg/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "../third_party/nanosvg/nanosvgrast.h"

enum { CMD_ADD_FOLDER=1102,CMD_ADD_FILES=1103,CMD_SEARCH=1105,CMD_REFRESH=1105,
 CMD_APPLY_TAGS=1107,CMD_OPEN=1108,CMD_SHELL=1109,CMD_NAVIGATE=1115,
 CMD_ROTATE_LEFT=1201,CMD_ROTATE_RIGHT=1202,CMD_RENAME=1203,CMD_VIEW_APPLY_TAGS=1205,
 CMD_RENAME_OK=1302,CMD_RENAME_CANCEL=1303 };
typedef struct { int id; const char *file; HICON icon; } IconEntry;
static IconEntry g_icons[]={
 {CMD_ADD_FOLDER,"folder-plus.svg",NULL},{CMD_ADD_FILES,"photo.svg",NULL},
 {CMD_REFRESH,"refresh.svg",NULL},{CMD_APPLY_TAGS,"tag.svg",NULL},
 {CMD_OPEN,"external-link.svg",NULL},{CMD_SHELL,"folder.svg",NULL},
 {CMD_NAVIGATE,"arrow-right.svg",NULL},{CMD_ROTATE_LEFT,"rotate-2.svg",NULL},
 {CMD_ROTATE_RIGHT,"rotate-clockwise-2.svg",NULL},{CMD_RENAME,"edit.svg",NULL},
 {CMD_VIEW_APPLY_TAGS,"tag.svg",NULL},{CMD_RENAME_OK,"edit.svg",NULL},
 {CMD_RENAME_CANCEL,"external-link.svg",NULL}
};
static char *replace_current_color(const char *src){
 const char *key="currentColor";size_t n=strlen(src),k=strlen(key),outn=0;
 char *out=(char*)malloc(n+1);if(!out)return NULL;
 for(size_t i=0;i<n;){if(!strncmp(src+i,key,k)){memcpy(out+outn,"#E7EAEE",7);outn+=7;i+=k;}else out[outn++]=src[i++];}
 out[outn]=0;return out;
}
static HICON load_svg_icon(const char *path){
 FILE *f=fopen(path,"rb");if(!f)return NULL;
 fseek(f,0,SEEK_END);long len=ftell(f);rewind(f);
 if(len<=0||len>1024*1024){fclose(f);return NULL;}
 char *raw=(char*)malloc((size_t)len+1);if(!raw){fclose(f);return NULL;}
 fread(raw,1,(size_t)len,f);fclose(f);raw[len]=0;
 char *svg=replace_current_color(raw);free(raw);if(!svg)return NULL;
 NSVGimage *image=nsvgParse(svg,"px",96.0f);free(svg);if(!image)return NULL;
 NSVGrasterizer *raster=nsvgCreateRasterizer();if(!raster){nsvgDelete(image);return NULL;}
 unsigned char *rgba=(unsigned char*)calloc(24*24,4);
 if(!rgba){nsvgDeleteRasterizer(raster);nsvgDelete(image);return NULL;}
 float scale=24.0f/(image->width>image->height?image->width:image->height);
 float ox=(24.0f-image->width*scale)*0.5f,oy=(24.0f-image->height*scale)*0.5f;
 nsvgRasterize(raster,image,ox,oy,scale,rgba,24,24,24*4);
 BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=24;
 bi.bmiHeader.biHeight=-24;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 void *bits=NULL;HDC dc=GetDC(NULL);HBITMAP color=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,NULL,0);ReleaseDC(NULL,dc);
 if(color&&bits){unsigned char *dst=(unsigned char*)bits;for(int i=0;i<24*24;i++){dst[i*4]=rgba[i*4+2];dst[i*4+1]=rgba[i*4+1];dst[i*4+2]=rgba[i*4];dst[i*4+3]=rgba[i*4+3];}}
 HBITMAP mask=CreateBitmap(24,24,1,1,NULL);ICONINFO ii={0};ii.fIcon=TRUE;ii.hbmColor=color;ii.hbmMask=mask;
 HICON icon=(color&&mask)?CreateIconIndirect(&ii):NULL;
 if(mask)DeleteObject(mask);if(color)DeleteObject(color);free(rgba);nsvgDeleteRasterizer(raster);nsvgDelete(image);return icon;
}
int icons_init(void){
 wchar_t exe[MAX_PATH],dir[MAX_PATH];if(!GetModuleFileNameW(NULL,exe,MAX_PATH))return 0;
 lstrcpynW(dir,exe,MAX_PATH);PathRemoveFileSpecW(dir);if(!PathAppendW(dir,L"assets\\icons"))return 0;
 char folder[MAX_PATH*4];if(!WideCharToMultiByte(CP_UTF8,0,dir,-1,folder,sizeof(folder),NULL,NULL))return 0;
 int loaded=0;for(size_t i=0;i<sizeof(g_icons)/sizeof(g_icons[0]);i++){
  char path[MAX_PATH*4];_snprintf_s(path,sizeof(path),_TRUNCATE,"%s\\%s",folder,g_icons[i].file);
  g_icons[i].icon=load_svg_icon(path);if(g_icons[i].icon)loaded++;
 }return loaded;
}
void icons_shutdown(void){for(size_t i=0;i<sizeof(g_icons)/sizeof(g_icons[0]);i++){if(g_icons[i].icon){DestroyIcon(g_icons[i].icon);g_icons[i].icon=NULL;}}}
BOOL icons_draw_button(const DRAWITEMSTRUCT *d){
 if(!d||d->CtlType!=ODT_BUTTON)return FALSE;
 HBRUSH bg=CreateSolidBrush((d->itemState&ODS_SELECTED)?RGB(59,68,72):RGB(48,49,53));
 FillRect(d->hDC,&d->rcItem,bg);DeleteObject(bg);
 HPEN pen=CreatePen(PS_SOLID,1,(d->itemState&ODS_FOCUS)?RGB(79,174,184):RGB(67,69,74));
 HGDIOBJ old=SelectObject(d->hDC,pen);HGDIOBJ oldbrush=SelectObject(d->hDC,GetStockObject(NULL_BRUSH));
 RoundRect(d->hDC,d->rcItem.left,d->rcItem.top,d->rcItem.right,d->rcItem.bottom,7,7);
 SelectObject(d->hDC,oldbrush);SelectObject(d->hDC,old);DeleteObject(pen);
 HICON icon=NULL;int id=GetDlgCtrlID(d->hwndItem);
 for(size_t i=0;i<sizeof(g_icons)/sizeof(g_icons[0]);i++)if(g_icons[i].id==id){icon=g_icons[i].icon;break;}
 RECT r=d->rcItem;int left=r.left+12;
 if(icon){int iy=r.top+((r.bottom-r.top)-18)/2;DrawIconEx(d->hDC,left,iy,icon,18,18,0,NULL,DI_NORMAL);left+=27;}
 wchar_t label[128]=L"";GetWindowTextW(d->hwndItem,label,128);
 RECT tr={left,r.top+2,r.right-8,r.bottom-2};SetBkMode(d->hDC,TRANSPARENT);
 SetTextColor(d->hDC,RGB(235,237,240));SelectObject(d->hDC,GetStockObject(DEFAULT_GUI_FONT));
 DrawTextW(d->hDC,label,-1,&tr,DT_SINGLELINE|DT_VCENTER|DT_LEFT|DT_END_ELLIPSIS);
 if(d->itemState&ODS_FOCUS){RECT fr=r;InflateRect(&fr,-4,-4);DrawFocusRect(d->hDC,&fr);}return TRUE;
}
