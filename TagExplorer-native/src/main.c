#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>
#include "resource.h"
#include "catalog.h"
#include "images.h"
#include "icons.h"

#define MAIN_CLASS L"TagExplorerNativeMain"
#define VIEWER_CLASS L"TagExplorerNativeViewer"
#define SIDEBAR_W 278
#define TOP_H 66
#define TOOL_H 104
#define ID_SOURCE_LIST 1101
#define ID_ADD_FOLDER 1102
#define ID_ADD_FILES 1103
#define ID_SEARCH 1104
#define ID_REFRESH 1105
#define ID_TAG 1106
#define ID_APPLY_TAGS 1107
#define ID_OPEN 1108
#define ID_SHELL 1109
#define ID_LIST 1110
#define ID_STATUS 1111
#define ID_TAGS_HEADER 1112
#define ID_TAG_LIST 1113
#define ID_ADDRESS 1114
#define ID_NAVIGATE 1115
#define ID_MONTH_TAG 1116
#define ID_YEAR_TAG 1117
#define ID_MONTH_FIRST 2101
#define ID_VIEW_ROTATE_LEFT 1201
#define ID_VIEW_ROTATE_RIGHT 1202
#define ID_VIEW_RENAME 1203
#define ID_VIEW_TAGS 1204
#define ID_VIEW_APPLY_TAGS 1205
#define ID_VIEW_MONTH_TAG 1206
#define ID_VIEW_YEAR_TAG 1207
#define ID_VIEW_PREVIOUS 1208
#define ID_VIEW_NEXT 1209
#define WM_APP_ROTATE_COMPLETE (WM_APP+41)
#define WM_APP_THUMBNAIL_READY (WM_APP+42)
#define THUMBNAIL_SLOTS 256
static HWND viewer_tags_edit,viewer_window,viewer_status;
static HACCEL viewer_accel;
static int viewer_rotation_busy;
typedef struct { LONG generation; int index; wchar_t path[MAX_PATH]; } ThumbRequest;
typedef struct { LONG generation; int index; HBITMAP bitmap; } ThumbReady;
static volatile LONG thumbnail_generation;
static CRITICAL_SECTION thumbnail_lock;
static HANDLE thumbnail_event,thumbnail_thread;
static ThumbRequest thumbnail_queue[1024];
static unsigned thumbnail_head,thumbnail_tail,thumbnail_count;
static int thumbnail_stopping,thumbnail_slot_count,thumbnail_next_slot;
static int *photo_image_slots,*photo_thumbnail_states;
static int slot_photo[THUMBNAIL_SLOTS];
typedef struct { HWND window; wchar_t path[MAX_PATH]; int degrees; HRESULT result; } RotateJob;
static DWORD WINAPI rotate_worker(LPVOID parameter){
 RotateJob *job=(RotateJob*)parameter; HRESULT com_hr=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
 job->result=FAILED(com_hr)?com_hr:image_rotate_file(job->path,job->degrees);
 if(SUCCEEDED(com_hr))CoUninitialize();
 if(!PostMessageW(job->window,WM_APP_ROTATE_COMPLETE,0,(LPARAM)job)){InterlockedExchange(&viewer_rotation_busy,0);free(job);}
 return 0;
}

static HINSTANCE app_instance;
static HWND main_window,sidebar,source_list,tag_list,address_edit,search_edit,tag_edit,photo_list,status_text;
static HWND title_label,subtitle_label,brand_label,source_heading,tag_heading;
static wchar_t **photo_paths,**photo_tags,**tag_names;
static int tag_name_count;
static wchar_t active_tag[256];
static int photo_count,viewer_index,viewer_rotation;
static HIMAGELIST thumbnail_list;
static HFONT font_ui,font_small,font_title;
static COLORREF bg_main=RGB(31,31,33),bg_panel=RGB(43,43,46),bg_input=RGB(55,55,59);
static COLORREF fg_main=RGB(238,238,240),fg_muted=RGB(151,153,158),accent=RGB(79,174,184);

static void set_status(const wchar_t *s){if(status_text)SetWindowTextW(status_text,s);}
static void set_viewer_status(const wchar_t *s){if(viewer_status)SetWindowTextW(viewer_status,s);}
static void free_results(void){if(photo_paths)catalog_free_results(photo_paths,photo_tags,photo_count);photo_paths=photo_tags=NULL;free(photo_image_slots);free(photo_thumbnail_states);photo_image_slots=photo_thumbnail_states=NULL;photo_count=0;}
static HWND make_label(HWND parent,const wchar_t *text,int x,int y,int w,int h,HFONT font,COLORREF color){
 HWND hnd=CreateWindowExW(0,L"STATIC",text,WS_CHILD|WS_VISIBLE|SS_LEFT,x,y,w,h,parent,NULL,app_instance,NULL);
 SendMessageW(hnd,WM_SETFONT,(WPARAM)font,TRUE);SetPropW(hnd,L"TagExplorerTextColor",(HANDLE)(ULONG_PTR)(color+1));return hnd;
}
static HWND make_button(HWND parent,const wchar_t *text,int id,int x,int y,int w,int h){
 HWND b=CreateWindowExW(0,L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,x,y,w,h,parent,(HMENU)(INT_PTR)id,app_instance,NULL);
 SendMessageW(b,WM_SETFONT,(WPARAM)font_ui,TRUE);SetWindowTheme(b,L"Explorer",NULL);return b;
}
static void add_tag_list_entry(const wchar_t *filter,const wchar_t *label){
 wchar_t **next=(wchar_t**)realloc(tag_names,(size_t)(tag_name_count+1)*sizeof(wchar_t*));
 if(!next)return;tag_names=next;tag_names[tag_name_count]=_wcsdup(filter);
 if(!tag_names[tag_name_count])return;
 SendMessageW(tag_list,LB_ADDSTRING,0,(LPARAM)label);tag_name_count++;
}
static int tag_is_month_key(const wchar_t *tag){
 return tag&&wcsncmp(tag,L"Month: ",7)==0&&wcslen(tag)==14&&tag[11]==L'-'&&
  tag[7]>=L'0'&&tag[7]<=L'9'&&tag[8]>=L'0'&&tag[8]<=L'9'&&
  tag[9]>=L'0'&&tag[9]<=L'9'&&tag[10]>=L'0'&&tag[10]<=L'9'&&
  tag[12]>=L'0'&&tag[12]<=L'9'&&tag[13]>=L'0'&&tag[13]<=L'9';
}
static void refresh_tag_list(void){
 static const wchar_t *months[]={L"January",L"February",L"March",L"April",L"May",L"June",L"July",L"August",L"September",L"October",L"November",L"December"};
 if(tag_names){for(int i=0;i<tag_name_count;i++)free(tag_names[i]);free(tag_names);tag_names=NULL;tag_name_count=0;}
 SendMessageW(tag_list,LB_RESETCONTENT,0,0);SendMessageW(tag_list,LB_ADDSTRING,0,(LPARAM)L"All tags");
 wchar_t **db_names=NULL;int db_count=0;
 if(catalog_list_tags(&db_names,&db_count)){
  /* Each year is a selectable parent; its month entries carry that year in
     their database key, so equal month names from different years never merge. */
  for(int i=0;i<db_count;i++)if(wcsncmp(db_names[i],L"Year: ",6)==0&&wcslen(db_names[i])==10){
   wchar_t year[5],label[16];lstrcpynW(year,db_names[i]+6,5);
   swprintf(label,16,L"%s",year);add_tag_list_entry(db_names[i],label);
   for(int m=1;m<=12;m++){
    wchar_t key[32],child[48];swprintf(key,32,L"Month: %s-%02d",year,m);
    int found=0;for(int j=0;j<db_count;j++)if(!_wcsicmp(db_names[j],key)){found=1;break;}
    if(found){swprintf(child,48,L"    %s",months[m-1]);add_tag_list_entry(key,child);}
   }
  }
  for(int i=0;i<db_count;i++){
   const wchar_t *tag=db_names[i];
   if(wcsncmp(tag,L"Year: ",6)==0&&wcslen(tag)==10)continue;
   if(tag_is_month_key(tag))continue;
   if(wcsncmp(tag,L"Month: ",7)==0){
    wchar_t label[96];swprintf(label,96,L"    %s (year not set)",tag+7);add_tag_list_entry(tag,label);
   }else add_tag_list_entry(tag,tag);
  }
  for(int i=0;i<db_count;i++)free(db_names[i]);free(db_names);
 }
 int selected=0;if(active_tag[0])for(int i=0;i<tag_name_count;i++)if(!_wcsicmp(active_tag,tag_names[i])){selected=i+1;break;}
 SendMessageW(tag_list,LB_SETCURSEL,selected,0);
}
static void refresh_catalog(void){
 InterlockedIncrement(&thumbnail_generation);
 EnterCriticalSection(&thumbnail_lock);
 thumbnail_head=thumbnail_tail=thumbnail_count=0;
 LeaveCriticalSection(&thumbnail_lock);
 ListView_SetItemCountEx(photo_list,0,LVSICF_NOINVALIDATEALL|LVSICF_NOSCROLL);
 free_results();ImageList_RemoveAll(thumbnail_list);thumbnail_slot_count=thumbnail_next_slot=0;
 wchar_t query[512];GetWindowTextW(search_edit,query,512);
 if(!catalog_search(query,active_tag,&photo_paths,&photo_tags,&photo_count)){set_status(L"Unable to read the photo catalog.");return;}
 if(photo_count){
  photo_image_slots=(int*)malloc((size_t)photo_count*sizeof(int));
  photo_thumbnail_states=(int*)calloc((size_t)photo_count,sizeof(int));
  if(photo_image_slots)for(int i=0;i<photo_count;i++)photo_image_slots[i]=-1;
 }
 ListView_SetItemCountEx(photo_list,photo_count,LVSICF_NOINVALIDATEALL|LVSICF_NOSCROLL);
 if(photo_count)ListView_RedrawItems(photo_list,0,photo_count-1);
 refresh_tag_list();
 wchar_t message[160];if(active_tag[0])swprintf(message,160,L"%d photos tagged '%s'",photo_count,active_tag);else swprintf(message,160,L"%d photos",photo_count);set_status(message);
}
static void enqueue_thumbnail(int index){
 if(index<0||index>=photo_count||!photo_paths||!photo_thumbnail_states||!thumbnail_thread||!thumbnail_event||photo_thumbnail_states[index])return;
 ThumbRequest request={0};request.generation=InterlockedCompareExchange(&thumbnail_generation,0,0);request.index=index;
 lstrcpynW(request.path,photo_paths[index],MAX_PATH);
 EnterCriticalSection(&thumbnail_lock);
 if(thumbnail_count<1024&&!thumbnail_stopping){
  thumbnail_queue[thumbnail_tail]=request;thumbnail_tail=(thumbnail_tail+1)%1024;thumbnail_count++;
  photo_thumbnail_states[index]=1;SetEvent(thumbnail_event);
 }
 LeaveCriticalSection(&thumbnail_lock);
}
static DWORD WINAPI thumbnail_worker(LPVOID unused){
 (void)unused;HRESULT co=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
 for(;;){
  WaitForSingleObject(thumbnail_event,INFINITE);
  ThumbRequest request;int have=0,stop=0;
  EnterCriticalSection(&thumbnail_lock);
  if(thumbnail_count){request=thumbnail_queue[thumbnail_head];thumbnail_head=(thumbnail_head+1)%1024;thumbnail_count--;have=1;if(thumbnail_count)SetEvent(thumbnail_event);}
  stop=thumbnail_stopping&&!thumbnail_count;
  LeaveCriticalSection(&thumbnail_lock);
  if(stop)break;
  if(!have)continue;
  if(request.generation!=InterlockedCompareExchange(&thumbnail_generation,0,0))continue;
  HBITMAP bitmap=image_thumbnail_cached(request.path,176);
  if(bitmap&&request.generation==InterlockedCompareExchange(&thumbnail_generation,0,0)){
   ThumbReady *ready=(ThumbReady*)malloc(sizeof(*ready));
   if(ready){ready->generation=request.generation;ready->index=request.index;ready->bitmap=bitmap;
    if(!PostMessageW(main_window,WM_APP_THUMBNAIL_READY,0,(LPARAM)ready)){DeleteObject(bitmap);free(ready);}
   }else DeleteObject(bitmap);
  }else if(bitmap)DeleteObject(bitmap);
 }
 if(SUCCEEDED(co))CoUninitialize();return 0;
}
static void accept_thumbnail(ThumbReady *ready){
 if(!ready)return;
 if(ready->generation==InterlockedCompareExchange(&thumbnail_generation,0,0)&&ready->index>=0&&ready->index<photo_count&&photo_image_slots){
  int row=ready->index,slot=photo_image_slots[row];
  if(slot<0){
   if(thumbnail_slot_count<THUMBNAIL_SLOTS)slot=thumbnail_slot_count++;
   else slot=thumbnail_next_slot++%THUMBNAIL_SLOTS;
   int old=slot_photo[slot];if(old>=0&&old<photo_count&&photo_image_slots[old]==slot){photo_image_slots[old]=-1;ListView_RedrawItems(photo_list,old,old);}
   photo_image_slots[row]=slot;slot_photo[slot]=row;
  }
  ICONINFO info={0};info.fIcon=TRUE;info.hbmColor=ready->bitmap;info.hbmMask=ready->bitmap;
  HICON icon=CreateIconIndirect(&info);
  if(icon){if(slot<ImageList_GetImageCount(thumbnail_list))ImageList_ReplaceIcon(thumbnail_list,slot,icon);else ImageList_AddIcon(thumbnail_list,icon);DestroyIcon(icon);}
  if(photo_thumbnail_states)photo_thumbnail_states[row]=2;
  ListView_RedrawItems(photo_list,row,row);
 }else if(ready->generation==InterlockedCompareExchange(&thumbnail_generation,0,0)&&ready->index>=0&&ready->index<photo_count&&photo_thumbnail_states)photo_thumbnail_states[ready->index]=0;
 DeleteObject(ready->bitmap);free(ready);
}
static void browse_folder(void){
 BROWSEINFOW bi={0};bi.hwndOwner=main_window;bi.lpszTitle=L"Choose a folder to add to your photo library";bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
 PIDLIST_ABSOLUTE id=SHBrowseForFolderW(&bi);if(!id)return;wchar_t folder[MAX_PATH];
 if(SHGetPathFromIDListW(id,folder)){int n=catalog_add_tree(folder);wchar_t m[160];swprintf(m,160,L"Added %d photos from %s",n,folder);set_status(m);refresh_catalog();}
 CoTaskMemFree(id);
}
static void add_files(void){
 wchar_t buffer[32768]=L"";OPENFILENAMEW ofn={0};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=main_window;
 ofn.lpstrFilter=L"Images\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff;*.ico;*.wdp;*.heic;*.webp\0All files\0*.*\0";
 ofn.lpstrFile=buffer;ofn.nMaxFile=32768;ofn.Flags=OFN_EXPLORER|OFN_ALLOWMULTISELECT|OFN_FILEMUSTEXIST;
 if(!GetOpenFileNameW(&ofn))return;
 if(buffer[wcslen(buffer)+1]==0)catalog_add(buffer);
 else{wchar_t folder[MAX_PATH];lstrcpynW(folder,buffer,MAX_PATH);wchar_t *name=buffer+wcslen(buffer)+1;
  while(*name){wchar_t full[MAX_PATH];lstrcpynW(full,folder,MAX_PATH);PathAppendW(full,name);catalog_add(full);name+=wcslen(name)+1;}}
 refresh_catalog();
}
static void navigate_address(void){
 wchar_t path[32768];GetWindowTextW(address_edit,path,32768);if(!path[0])return;
 DWORD attr=GetFileAttributesW(path);if(attr==INVALID_FILE_ATTRIBUTES){set_status(L"Address does not exist. Check the path and try again.");return;}
 if(attr&FILE_ATTRIBUTE_DIRECTORY){int n=catalog_add_tree(path);wchar_t msg[160];swprintf(msg,160,L"Indexed %d images from this folder",n);set_status(msg);}
 else catalog_add(path);
 SetWindowTextW(search_edit,path);active_tag[0]=0;SendMessageW(tag_list,LB_SETCURSEL,0,0);refresh_catalog();
}
static void apply_tags(void){
 int i=ListView_GetNextItem(photo_list,-1,LVNI_SELECTED);if(i<0){set_status(L"Select photos in the gallery first.");return;}
 wchar_t tags[512];GetWindowTextW(tag_edit,tags,512);if(!tags[0]){set_status(L"Type tags separated by commas.");return;}
 int count=0;for(;i>=0;i=ListView_GetNextItem(photo_list,i,LVNI_SELECTED)){catalog_apply_tags(photo_paths[i],tags);count++;}
 SetWindowTextW(tag_edit,L"");refresh_catalog();wchar_t m[96];swprintf(m,96,L"Updated tags on %d photo(s)",count);set_status(m);
}
static void install_shell_menu(void){
 const wchar_t *exts[]={L".jpg",L".jpeg",L".png",L".bmp",L".gif",L".tif",L".tiff",L".webp",L".heic"};
 wchar_t exe[MAX_PATH],keypath[320],command[MAX_PATH*2];GetModuleFileNameW(NULL,exe,MAX_PATH);
 for(int i=0;i<9;i++){wsprintfW(keypath,L"Software\\Classes\\SystemFileAssociations\\%s\\shell\\TagExplorer\\command",exts[i]);HKEY key;
  if(RegCreateKeyExW(HKEY_CURRENT_USER,keypath,0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL)==ERROR_SUCCESS){
   wsprintfW(command,L"\"%s\" --quick-tag \"%%1\"",exe);RegSetValueExW(key,NULL,0,REG_SZ,(const BYTE*)command,(DWORD)((wcslen(command)+1)*sizeof(wchar_t)));RegCloseKey(key);
  }}
 set_status(L"Tag Explorer was added to the File Explorer right-click menu.");
}
static void open_viewer(int index){
 if(index<0||index>=photo_count)return;viewer_index=index;viewer_rotation=0;
 WNDCLASSW wc={0};wc.lpfnWndProc=NULL;wc.hInstance=app_instance;wc.lpszClassName=VIEWER_CLASS;
}
#define ID_RENAME_EDIT 1301
#define ID_RENAME_OK 1302
#define ID_RENAME_CANCEL 1303
static wchar_t rename_input[MAX_PATH];
static const wchar_t *prompt_title=L"Rename original image";
static const wchar_t *prompt_label=L"New file name (the original image will be renamed in place):";
static HWND rename_edit;
static LRESULT CALLBACK rename_prompt_proc(HWND d,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_DRAWITEM:icons_draw_button((DRAWITEMSTRUCT*)lp);return TRUE;
 case WM_CREATE:{
  CreateWindowExW(0,L"STATIC",prompt_label,WS_CHILD|WS_VISIBLE,16,16,390,22,d,NULL,app_instance,NULL);
  rename_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",rename_input,WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,16,44,390,28,d,(HMENU)(INT_PTR)ID_RENAME_EDIT,app_instance,NULL);
  SendMessageW(rename_edit,WM_SETFONT,(WPARAM)font_ui,TRUE);
  HWND ok=make_button(d,L"Rename",ID_RENAME_OK,230,86,84,32);
  make_button(d,L"Cancel",ID_RENAME_CANCEL,322,86,84,32);
  SetFocus(rename_edit);SendMessageW(rename_edit,EM_SETSEL,0,-1);return 0;}
 case WM_COMMAND:
  if(LOWORD(wp)==ID_RENAME_OK){GetWindowTextW(rename_edit,rename_input,MAX_PATH);if(rename_input[0])DestroyWindow(d);return 0;}
  if(LOWORD(wp)==ID_RENAME_CANCEL){rename_input[0]=0;DestroyWindow(d);return 0;}break;
 case WM_CLOSE:rename_input[0]=0;DestroyWindow(d);return 0;
 }
 return DefWindowProcW(d,msg,wp,lp);
}
static int prompt_value(HWND owner,const wchar_t *title,const wchar_t *label,const wchar_t *current_name,wchar_t *new_name,size_t capacity){
 prompt_title=title;prompt_label=label;
 WNDCLASSW wc={0};wc.lpfnWndProc=rename_prompt_proc;wc.hInstance=app_instance;wc.lpszClassName=L"TagExplorerRenamePrompt";wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
 RegisterClassW(&wc);
 lstrcpynW(rename_input,current_name,MAX_PATH);
 HWND d=CreateWindowExW(WS_EX_DLGMODALFRAME, L"TagExplorerRenamePrompt",prompt_title,WS_CAPTION|WS_SYSMENU|WS_POPUP|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,440,168,owner,NULL,app_instance,NULL);
 if(!d)return 0;
 RECT r;GetWindowRect(d,&r);int x=(GetSystemMetrics(SM_CXSCREEN)-(r.right-r.left))/2,y=(GetSystemMetrics(SM_CYSCREEN)-(r.bottom-r.top))/2;SetWindowPos(d,NULL,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER);
 EnableWindow(owner,FALSE);SetForegroundWindow(d);SetFocus(rename_edit);
 MSG msg;while(IsWindow(d)&&GetMessageW(&msg,NULL,0,0)>0){if(!IsDialogMessageW(d,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
 EnableWindow(owner,TRUE);SetForegroundWindow(owner);
 if(!rename_input[0])return 0;
 lstrcpynW(new_name,rename_input,(int)capacity);return 1;
}
static int prompt_rename(HWND owner,const wchar_t *current_name,wchar_t *new_name,size_t capacity){return prompt_value(owner,L"Rename original image",L"New file name (the original image will be renamed in place):",current_name,new_name,capacity);}
static void viewer_rename(HWND w){
 if(viewer_index<0||viewer_index>=photo_count)return;
 wchar_t old[MAX_PATH],folder[MAX_PATH],name[MAX_PATH],target[MAX_PATH];
 lstrcpynW(old,photo_paths[viewer_index],MAX_PATH);lstrcpynW(folder,old,MAX_PATH);PathRemoveFileSpecW(folder);
 lstrcpynW(name,PathFindFileNameW(old),MAX_PATH);
 if(!prompt_rename(w,name,target,MAX_PATH))return;
 if(wcschr(target,L'\\')||wcschr(target,L'/')||wcschr(target,L':')){MessageBoxW(w,L"Enter only a file name, not a folder path.",L"Rename photo",MB_ICONWARNING);return;}
 if(!_wcsicmp(name,target))return;
 wchar_t new_path[MAX_PATH];lstrcpynW(new_path,folder,MAX_PATH);if(!PathAppendW(new_path,target)){MessageBoxW(w,L"The new file name is too long.",L"Rename photo",MB_ICONERROR);return;}
 if(GetFileAttributesW(new_path)!=INVALID_FILE_ATTRIBUTES){MessageBoxW(w,L"A file with that name already exists in this folder.",L"Rename photo",MB_ICONWARNING);return;}
 /* MoveFileW performs a same-folder rename; it does not encode, save, or copy image data. */
 if(!MoveFileW(old,new_path)){MessageBoxW(w,L"Windows could not rename the original image.",L"Rename photo",MB_ICONERROR);return;}
 if(!catalog_rename(old,new_path)){MoveFileW(new_path,old);MessageBoxW(w,L"The catalog could not update the renamed photo. The original filename was restored.",L"Rename photo",MB_ICONERROR);return;}
 free(photo_paths[viewer_index]);photo_paths[viewer_index]=_wcsdup(new_path);
 SetWindowTextW(w,PathFindFileNameW(new_path));set_status(L"Original image renamed in its existing folder.");InvalidateRect(w,NULL,TRUE);
}
static void viewer_apply_date_tag(const wchar_t *tag){
 if(viewer_index<0||viewer_index>=photo_count)return;
 if(catalog_apply_tags(photo_paths[viewer_index],tag)){
  wchar_t message[128];swprintf(message,128,L"Applied %s to this photo",tag);set_status(message);
 }else set_status(L"Could not apply the date tag to this photo.");
}
static int prompt_tag_year(HWND owner,wchar_t *year,size_t capacity){
 SYSTEMTIME now;GetLocalTime(&now);swprintf(year,capacity,L"%04u",(unsigned)now.wYear);
 if(!prompt_value(owner,L"Choose year",L"Enter a four-digit year:",year,year,capacity))return 0;
 if(wcslen(year)!=4||year[0]<L'0'||year[0]>L'9'||year[1]<L'0'||year[1]>L'9'||year[2]<L'0'||year[2]>L'9'||year[3]<L'0'||year[3]>L'9'){
  MessageBoxW(owner,L"Enter a valid four-digit year, such as 2026.",L"Year tag",MB_ICONWARNING);return 0;
 }
 return 1;
}
static int choose_month_number(HWND owner,int button_id){
 static const wchar_t *months[]={L"January",L"February",L"March",L"April",L"May",L"June",L"July",L"August",L"September",L"October",L"November",L"December"};
 HMENU menu=CreatePopupMenu();if(!menu)return 0;
 for(int i=0;i<12;i++)AppendMenuW(menu,MF_STRING,ID_MONTH_FIRST+i,months[i]);
 RECT rc;GetWindowRect(GetDlgItem(owner,button_id),&rc);
 int selected=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN|TPM_TOPALIGN,rc.left,rc.bottom,0,owner,NULL);
 DestroyMenu(menu);return selected>=ID_MONTH_FIRST&&selected<ID_MONTH_FIRST+12?selected-ID_MONTH_FIRST+1:0;
}
static void viewer_choose_month_tag(HWND owner){
 wchar_t year[16],tag[64];if(!prompt_tag_year(owner,year,16))return;
 int month=choose_month_number(owner,ID_VIEW_MONTH_TAG);if(!month)return;
 swprintf(tag,64,L"Year: %s, Month: %s-%02d",year,year,month);viewer_apply_date_tag(tag);
}
static void viewer_enter_year_tag(HWND owner){
 wchar_t year[16],tag[64];if(!prompt_tag_year(owner,year,16))return;
 swprintf(tag,64,L"Year: %s",year);viewer_apply_date_tag(tag);
}
static LRESULT CALLBACK viewer_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_DRAWITEM:icons_draw_button((DRAWITEMSTRUCT*)lp);return TRUE;
 case WM_CTLCOLORSTATIC:{HDC dc=(HDC)wp;HWND ctl=(HWND)lp;SetBkMode(dc,TRANSPARENT);HANDLE c=GetPropW(ctl,L"TagExplorerTextColor");SetTextColor(dc,c?(COLORREF)((ULONG_PTR)c-1):fg_main);return (LRESULT)GetStockObject(NULL_BRUSH);}
 case WM_CREATE:
  make_button(w,L"Rotate left",ID_VIEW_ROTATE_LEFT,16,12,104,34);
  make_button(w,L"Rotate right",ID_VIEW_ROTATE_RIGHT,128,12,110,34);
  make_button(w,L"Rename",ID_VIEW_RENAME,248,12,86,34);
  make_button(w,L"Month tag",ID_VIEW_MONTH_TAG,344,12,100,34);
  make_button(w,L"Year tag",ID_VIEW_YEAR_TAG,452,12,90,34);
  viewer_tags_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,550,12,260,34,w,(HMENU)(INT_PTR)ID_VIEW_TAGS,app_instance,NULL);
  SendMessageW(viewer_tags_edit,WM_SETFONT,(WPARAM)font_ui,TRUE);
  make_button(w,L"Add tags",ID_VIEW_APPLY_TAGS,820,12,96,34);
  make_button(w,L"Previous",ID_VIEW_PREVIOUS,924,12,82,34);
  make_button(w,L"Next",ID_VIEW_NEXT,1014,12,68,34);
  viewer_status=make_label(w,L"Ready",16,0,520,22,font_small,fg_muted);
  return 0;
 case WM_SIZE:{
  int width=LOWORD(lp);MoveWindow(GetDlgItem(w,ID_VIEW_ROTATE_LEFT),16,12,104,34,TRUE);
  MoveWindow(GetDlgItem(w,ID_VIEW_ROTATE_RIGHT),128,12,110,34,TRUE);
  MoveWindow(GetDlgItem(w,ID_VIEW_RENAME),248,12,86,34,TRUE);
  MoveWindow(GetDlgItem(w,ID_VIEW_MONTH_TAG),344,12,100,34,TRUE);
  MoveWindow(GetDlgItem(w,ID_VIEW_YEAR_TAG),452,12,90,34,TRUE);
  MoveWindow(viewer_tags_edit,550,12,width-838>100?width-838:100,34,TRUE);
  MoveWindow(GetDlgItem(w,ID_VIEW_APPLY_TAGS),width-276,12,88,34,TRUE);
  MoveWindow(GetDlgItem(w,ID_VIEW_PREVIOUS),width-180,12,82,34,TRUE);
  MoveWindow(GetDlgItem(w,ID_VIEW_NEXT),width-90,12,74,34,TRUE);
  RECT client;GetClientRect(w,&client);if(viewer_status)MoveWindow(viewer_status,16,client.bottom-28,520,20,TRUE);return 0;}
 case WM_COMMAND:
  switch(LOWORD(wp)){
   case ID_VIEW_ROTATE_LEFT:
   case ID_VIEW_ROTATE_RIGHT:{
    if(viewer_index<0||viewer_index>=photo_count||viewer_rotation_busy)return 0;
    RotateJob *job=(RotateJob*)calloc(1,sizeof(*job));if(!job){set_status(L"Not enough memory to rotate this image.");return 0;}
    job->window=w;job->degrees=LOWORD(wp)==ID_VIEW_ROTATE_RIGHT?90:270;
    lstrcpynW(job->path,photo_paths[viewer_index],MAX_PATH);
    viewer_rotation_busy=1;
    EnableWindow(GetDlgItem(w,ID_VIEW_ROTATE_LEFT),FALSE);EnableWindow(GetDlgItem(w,ID_VIEW_ROTATE_RIGHT),FALSE);
    set_viewer_status(L"Saving rotation to the original image... You can keep browsing.");
    HANDLE thread=CreateThread(NULL,0,rotate_worker,job,0,NULL);
    if(!thread){viewer_rotation_busy=0;EnableWindow(GetDlgItem(w,ID_VIEW_ROTATE_LEFT),TRUE);EnableWindow(GetDlgItem(w,ID_VIEW_ROTATE_RIGHT),TRUE);free(job);set_viewer_status(L"Could not start the image rotation task.");}
    else CloseHandle(thread);
    SetFocus(w);return 0;
   }
   case ID_VIEW_PREVIOUS:if(photo_count){viewer_index=(viewer_index+photo_count-1)%photo_count;viewer_rotation=0;SetWindowTextW(w,PathFindFileNameW(photo_paths[viewer_index]));InvalidateRect(w,NULL,TRUE);}SetFocus(w);return 0;
   case ID_VIEW_NEXT:if(photo_count){viewer_index=(viewer_index+1)%photo_count;viewer_rotation=0;SetWindowTextW(w,PathFindFileNameW(photo_paths[viewer_index]));InvalidateRect(w,NULL,TRUE);}SetFocus(w);return 0;
   case ID_VIEW_RENAME:viewer_rename(w);SetFocus(w);return 0;
   case ID_VIEW_MONTH_TAG:viewer_choose_month_tag(w);SetFocus(w);return 0;
   case ID_VIEW_YEAR_TAG:viewer_enter_year_tag(w);SetFocus(w);return 0;
   case ID_VIEW_APPLY_TAGS:{wchar_t tags[512];GetWindowTextW(viewer_tags_edit,tags,512);if(tags[0]&&catalog_apply_tags(photo_paths[viewer_index],tags)){SetWindowTextW(viewer_tags_edit,L"");set_status(L"Tags added to this photo.");}else set_status(L"Enter one or more comma-separated tags.");return 0;}
  }break;
 case WM_KEYDOWN:
  if(wp==VK_ESCAPE){DestroyWindow(w);return 0;}
  if(wp==VK_RIGHT&&photo_count){viewer_index=(viewer_index+1)%photo_count;viewer_rotation=0;SetWindowTextW(w,PathFindFileNameW(photo_paths[viewer_index]));InvalidateRect(w,NULL,TRUE);return 0;}
  if(wp==VK_LEFT&&photo_count){viewer_index=(viewer_index+photo_count-1)%photo_count;viewer_rotation=0;SetWindowTextW(w,PathFindFileNameW(photo_paths[viewer_index]));InvalidateRect(w,NULL,TRUE);return 0;}
  if(wp=='R'){viewer_rotation=(viewer_rotation+90)%360;InvalidateRect(w,NULL,TRUE);}return 0;
 case WM_APP_ROTATE_COMPLETE:{
  RotateJob *job=(RotateJob*)lp;viewer_rotation_busy=0;
  EnableWindow(GetDlgItem(w,ID_VIEW_ROTATE_LEFT),TRUE);EnableWindow(GetDlgItem(w,ID_VIEW_ROTATE_RIGHT),TRUE);
  if(SUCCEEDED(job->result)){
   image_invalidate_cache(job->path);
   if(viewer_index>=0&&viewer_index<photo_count&&!_wcsicmp(photo_paths[viewer_index],job->path)){viewer_rotation=0;InvalidateRect(w,NULL,TRUE);}
   {wchar_t saved[160];swprintf(saved,160,L"Rotation saved to original file: %s",PathFindFileNameW(job->path));set_viewer_status(saved);}
  }else{
   wchar_t error[192];swprintf(error,192,L"Could not save rotation (0x%08X). The original image was left unchanged.",(unsigned)job->result);
   set_viewer_status(error);MessageBoxW(w,error,L"Rotate image",MB_ICONERROR);
  }
  free(job);SetFocus(w);return 0;
 }
 case WM_DESTROY:if(viewer_window==w)viewer_window=NULL;viewer_status=NULL;InterlockedExchange(&viewer_rotation_busy,0);image_invalidate_cache(NULL);if(main_window&&IsWindow(main_window))refresh_catalog();return 0;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT rc;GetClientRect(w,&rc);FillRect(dc,&rc,(HBRUSH)GetStockObject(BLACK_BRUSH));rc.top+=58;if(photo_count)image_draw(dc,&rc,photo_paths[viewer_index],viewer_rotation);EndPaint(w,&ps);return 0;}
 }return DefWindowProcW(w,msg,wp,lp);
}
static void show_viewer(void){
 int i=ListView_GetNextItem(photo_list,-1,LVNI_SELECTED);if(i<0)return;
 WNDCLASSW wc={0};wc.lpfnWndProc=viewer_proc;wc.hInstance=app_instance;wc.lpszClassName=VIEWER_CLASS;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClassW(&wc);
 HWND v=CreateWindowExW(WS_EX_APPWINDOW,VIEWER_CLASS,PathFindFileNameW(photo_paths[i]),WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,1100,760,main_window,NULL,app_instance,NULL);
 viewer_window=v;viewer_index=i;viewer_rotation=0;ShowWindow(v,SW_MAXIMIZE);SetFocus(v);
}
static void layout_children(int width,int height){
 int content=width-SIDEBAR_W;if(content<360)content=360;
 /* Sidebar is painted by the main window background handler. */
 MoveWindow(title_label,SIDEBAR_W+28,15,420,28,TRUE);
 MoveWindow(subtitle_label,SIDEBAR_W+29,42,450,18,TRUE);
 MoveWindow(search_edit,width-390,18,250,32,TRUE);
 MoveWindow(photo_list,SIDEBAR_W+18,TOP_H+TOOL_H+14,content-36,height-TOP_H-TOOL_H-88,TRUE);
 MoveWindow(status_text,SIDEBAR_W+24,height-32,content-48,20,TRUE);
}
static void apply_tag_to_selected(const wchar_t *tag){
 int i=ListView_GetNextItem(photo_list,-1,LVNI_SELECTED);
 if(i<0){set_status(L"Select one or more photos in the gallery first.");return;}
 int count=0;
 for(;i>=0;i=ListView_GetNextItem(photo_list,i,LVNI_SELECTED))
  if(catalog_apply_tags(photo_paths[i],tag))count++;
 refresh_catalog();
 wchar_t message[128];swprintf(message,128,L"Applied %s to %d photo(s)",tag,count);set_status(message);
}
static void choose_month_tag(HWND owner){
 wchar_t year[16],tag[64];if(!prompt_tag_year(owner,year,16))return;
 int month=choose_month_number(owner,ID_MONTH_TAG);if(!month)return;
 swprintf(tag,64,L"Year: %s, Month: %s-%02d",year,year,month);apply_tag_to_selected(tag);
}
static void enter_year_tag(HWND owner){
 wchar_t year[16],tag[64];if(!prompt_tag_year(owner,year,16))return;
 swprintf(tag,64,L"Year: %s",year);apply_tag_to_selected(tag);
}
static LRESULT CALLBACK main_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_DRAWITEM:icons_draw_button((DRAWITEMSTRUCT*)lp);return TRUE;
 case WM_CREATE:{
  main_window=w;font_ui=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
  font_small=CreateFontW(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
  font_title=CreateFontW(-23,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
  sidebar=NULL;
  brand_label=make_label(w,L"◉  TAG EXPLORER",24,22,225,34,font_title,fg_main);
  source_heading=make_label(w,L"LIBRARY",24,92,220,20,font_small,fg_muted);
  make_button(w,L"Add folder",ID_ADD_FOLDER,20,122,238,38);
  make_button(w,L"Add photos",ID_ADD_FILES,20,168,238,38);
  make_label(w,L"SOURCES",24,232,220,20,font_small,fg_muted);
  source_list=CreateWindowExW(0,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|WS_VSCROLL,18,260,242,126,w,(HMENU)(INT_PTR)ID_SOURCE_LIST,app_instance,NULL);
  SendMessageW(source_list,WM_SETFONT,(WPARAM)font_ui,TRUE);SendMessageW(source_list,LB_SETITEMHEIGHT,0,28);MoveWindow(source_list,18,260,242,126,TRUE);SendMessageW(source_list,LB_ADDSTRING,0,(LPARAM)L"  All photos");
  make_label(w,L"TAGS",24,408,220,20,font_small,fg_muted);
  tag_list=CreateWindowExW(0,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|WS_VSCROLL,18,438,242,250,w,(HMENU)(INT_PTR)ID_TAG_LIST,app_instance,NULL);
  SendMessageW(tag_list,WM_SETFONT,(WPARAM)font_ui,TRUE);SendMessageW(tag_list,LB_SETITEMHEIGHT,0,28);
  make_button(w,L"Install Explorer menu",ID_SHELL,20,704,238,36);
  title_label=make_label(w,L"Photo library",SIDEBAR_W+28,15,420,28,font_title,fg_main);
  subtitle_label=make_label(w,L"Browse, search and organize your collection",SIDEBAR_W+29,42,450,18,font_small,fg_muted);
  search_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,0,0,250,32,w,(HMENU)(INT_PTR)ID_SEARCH,app_instance,NULL);
  SendMessageW(search_edit,WM_SETFONT,(WPARAM)font_ui,TRUE);
  make_button(w,L"Search",ID_REFRESH,0,0,74,32);
  address_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,0,0,600,32,w,(HMENU)(INT_PTR)ID_ADDRESS,app_instance,NULL);
  SendMessageW(address_edit,WM_SETFONT,(WPARAM)font_ui,TRUE);SendMessageW(address_edit,EM_SETCUEBANNER,TRUE,(LPARAM)L"Enter a folder or image path, then press Go");
  make_button(w,L"Go",ID_NAVIGATE,0,0,64,32);
  tag_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,0,0,250,32,w,(HMENU)(INT_PTR)ID_TAG,app_instance,NULL);
  SendMessageW(tag_edit,WM_SETFONT,(WPARAM)font_ui,TRUE);
  make_button(w,L"Apply tags",ID_APPLY_TAGS,0,0,100,32);
  make_button(w,L"Open photo",ID_OPEN,0,0,100,32);
  make_button(w,L"Month tag",ID_MONTH_TAG,0,0,104,32);
  make_button(w,L"Year tag",ID_YEAR_TAG,0,0,96,32);
  photo_list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_ICON|LVS_OWNERDATA|LVS_SHOWSELALWAYS,0,0,800,600,w,(HMENU)(INT_PTR)ID_LIST,app_instance,NULL);
  ListView_SetExtendedListViewStyle(photo_list,LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP|LVS_EX_BORDERSELECT);ListView_SetBkColor(photo_list,bg_main);ListView_SetTextBkColor(photo_list,bg_main);ListView_SetTextColor(photo_list,fg_main);
  thumbnail_list=ImageList_Create(176,136,ILC_COLOR32|ILC_MASK,THUMBNAIL_SLOTS,16);ListView_SetImageList(photo_list,thumbnail_list,LVSIL_NORMAL);for(int si=0;si<THUMBNAIL_SLOTS;si++)slot_photo[si]=-1;
  status_text=make_label(w,L"Ready",0,0,500,20,font_small,fg_muted);
  InitializeCriticalSection(&thumbnail_lock);thumbnail_event=CreateEventW(NULL,FALSE,FALSE,NULL);if(thumbnail_event)thumbnail_thread=CreateThread(NULL,0,thumbnail_worker,NULL,0,NULL);
  if(!catalog_open())MessageBoxW(w,L"Could not open the local photo catalog.",L"Tag Explorer",MB_ICONERROR);
  refresh_catalog();return 0;
 }
 case WM_SIZE:layout_children(LOWORD(lp),HIWORD(lp));{
  int width=LOWORD(lp),height=HIWORD(lp);
  MoveWindow(search_edit,width-360,17,230,32,TRUE);
  MoveWindow(GetDlgItem(w,ID_REFRESH),width-120,17,82,32,TRUE);
  MoveWindow(address_edit,SIDEBAR_W+22,TOP_H+5,width-SIDEBAR_W-132,32,TRUE);
  MoveWindow(GetDlgItem(w,ID_NAVIGATE),width-92,TOP_H+5,70,32,TRUE);
  MoveWindow(tag_edit,SIDEBAR_W+22,TOP_H+48,260,32,TRUE);
  MoveWindow(GetDlgItem(w,ID_APPLY_TAGS),SIDEBAR_W+292,TOP_H+48,104,32,TRUE);
  MoveWindow(GetDlgItem(w,ID_OPEN),SIDEBAR_W+406,TOP_H+48,100,32,TRUE);
  MoveWindow(GetDlgItem(w,ID_MONTH_TAG),SIDEBAR_W+516,TOP_H+48,104,32,TRUE);
  MoveWindow(GetDlgItem(w,ID_YEAR_TAG),SIDEBAR_W+628,TOP_H+48,96,32,TRUE);
  MoveWindow(tag_list,18,438,242,height-460>100?height-460:100,TRUE);
  MoveWindow(GetDlgItem(w,ID_SHELL),20,height-52,238,36,TRUE);
  }return 0;
 case WM_COMMAND:
  switch(LOWORD(wp)){case ID_ADD_FOLDER:browse_folder();break;case ID_ADD_FILES:add_files();break;case ID_REFRESH:refresh_catalog();break;case ID_NAVIGATE:navigate_address();break;case ID_APPLY_TAGS:apply_tags();break;case ID_OPEN:show_viewer();break;case ID_MONTH_TAG:choose_month_tag(w);break;case ID_YEAR_TAG:enter_year_tag(w);break;case ID_SHELL:install_shell_menu();break;case ID_SEARCH:if(HIWORD(wp)==EN_CHANGE){}break;case ID_SOURCE_LIST:if(HIWORD(wp)==LBN_SELCHANGE){active_tag[0]=0;SendMessageW(tag_list,LB_SETCURSEL,0,0);refresh_catalog();}break;case ID_TAG_LIST:if(HIWORD(wp)==LBN_SELCHANGE){int sel=(int)SendMessageW(tag_list,LB_GETCURSEL,0,0);if(sel<=0)active_tag[0]=0;else if(sel-1<tag_name_count)lstrcpynW(active_tag,tag_names[sel-1],256);refresh_catalog();}break;}return 0;
 case WM_KEYDOWN:if(wp==VK_RETURN&&GetFocus()==address_edit){navigate_address();return 0;}break;
 case WM_APP_THUMBNAIL_READY:accept_thumbnail((ThumbReady*)lp);return 0;
 case WM_NOTIFY:
  if(((NMHDR*)lp)->idFrom==ID_LIST){
   if(((NMHDR*)lp)->code==LVN_GETDISPINFOW){NMLVDISPINFOW *di=(NMLVDISPINFOW*)lp;int row=di->item.iItem;
    if(row>=0&&row<photo_count){
     if((di->item.mask&LVIF_TEXT)&&di->item.pszText){wchar_t caption[MAX_PATH+160];const wchar_t *name=PathFindFileNameW(photo_paths[row]);if(photo_tags[row]&&photo_tags[row][0])swprintf(caption,MAX_PATH+160,L"%s\\n%s",name,photo_tags[row]);else lstrcpynW(caption,name,MAX_PATH+160);lstrcpynW(di->item.pszText,caption,di->item.cchTextMax);}
     if(di->item.mask&LVIF_IMAGE){int slot=photo_image_slots?photo_image_slots[row]:-1;if(slot>=0&&slot<thumbnail_slot_count&&slot_photo[slot]==row)di->item.iImage=slot;else{di->item.iImage=-1;enqueue_thumbnail(row);}}
    }return 0;
   }
   if(((NMHDR*)lp)->code==NM_DBLCLK)show_viewer();
  }return 0;
 case WM_CTLCOLORSTATIC:{HDC dc=(HDC)wp;HWND ctl=(HWND)lp;SetBkMode(dc,TRANSPARENT);HANDLE c=GetPropW(ctl,L"TagExplorerTextColor");SetTextColor(dc,c?(COLORREF)((ULONG_PTR)c-1):fg_main);return (LRESULT)GetStockObject(NULL_BRUSH);}
 case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,fg_main);SetBkColor(dc,bg_input);return (LRESULT)GetStockObject(DKGRAY_BRUSH);}
 case WM_CTLCOLORLISTBOX:{HDC dc=(HDC)wp;SetTextColor(dc,fg_main);SetBkColor(dc,bg_panel);return (LRESULT)GetStockObject(DKGRAY_BRUSH);}
 case WM_ERASEBKGND:{RECT rc;GetClientRect(w,&rc);HBRUSH base=CreateSolidBrush(bg_main);FillRect((HDC)wp,&rc,base);DeleteObject(base);RECT side={0,0,SIDEBAR_W,rc.bottom};HBRUSH panel=CreateSolidBrush(bg_panel);FillRect((HDC)wp,&side,panel);DeleteObject(panel);return 1;}
 case WM_DESTROY:InterlockedIncrement(&thumbnail_generation);if(thumbnail_event){EnterCriticalSection(&thumbnail_lock);thumbnail_head=thumbnail_tail=thumbnail_count=0;thumbnail_stopping=1;SetEvent(thumbnail_event);LeaveCriticalSection(&thumbnail_lock);}if(thumbnail_thread){WaitForSingleObject(thumbnail_thread,INFINITE);CloseHandle(thumbnail_thread);}if(thumbnail_event)CloseHandle(thumbnail_event);DeleteCriticalSection(&thumbnail_lock);free_results();if(tag_names){for(int i=0;i<tag_name_count;i++)free(tag_names[i]);free(tag_names);}if(thumbnail_list)ImageList_Destroy(thumbnail_list);catalog_close();if(font_ui)DeleteObject(font_ui);if(font_small)DeleteObject(font_small);if(font_title)DeleteObject(font_title);icons_shutdown();PostQuitMessage(0);return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command_line,int show){
 (void)previous;(void)command_line;app_instance=instance;icons_init();CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
 INITCOMMONCONTROLSEX cc={sizeof(cc),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&cc);
 WNDCLASSW wc={0};wc.lpfnWndProc=main_proc;wc.hInstance=instance;wc.lpszClassName=MAIN_CLASS;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClassW(&wc);
 HWND w=CreateWindowExW(0,MAIN_CLASS,L"Tag Explorer",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1450,920,NULL,NULL,instance,NULL);
 ACCEL keys[]={{FVIRTKEY,VK_LEFT,ID_VIEW_PREVIOUS},{FVIRTKEY,VK_RIGHT,ID_VIEW_NEXT}};
 viewer_accel=CreateAcceleratorTableW(keys,2);
 ShowWindow(w,show);UpdateWindow(w);MSG m;while(GetMessageW(&m,NULL,0,0)>0){
  if(viewer_window&&IsWindow(viewer_window)&&GetActiveWindow()==viewer_window&&GetFocus()!=viewer_tags_edit&&viewer_accel&&TranslateAcceleratorW(viewer_window,viewer_accel,&m))continue;
  TranslateMessage(&m);DispatchMessageW(&m);
 }
 if(viewer_accel)DestroyAcceleratorTable(viewer_accel);CoUninitialize();return (int)m.wParam;
}