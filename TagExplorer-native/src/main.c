#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>
#include "resource.h"
#include "catalog.h"
#include "images.h"
#define MAIN_CLASS L"TagExplorerNativeMain"
#define PREVIEW_CLASS L"TagExplorerNativePreview"
static HINSTANCE app_instance;
static HWND main_window,path_edit,search_edit,tag_edit,photo_list,preview_window,status_text;
static wchar_t **photo_paths,**photo_tags;
static int photo_count,selected_index=-1;
static HIMAGELIST thumbnail_list;
static int viewer_index,viewer_rotation;
static void set_status(const wchar_t *s){SetWindowTextW(status_text,s);}
static void free_results(void){if(photo_paths)catalog_free_results(photo_paths,photo_tags,photo_count);photo_paths=photo_tags=NULL;photo_count=0;selected_index=-1;}
static void refresh_catalog(void){
 free_results();ListView_DeleteAllItems(photo_list);ImageList_RemoveAll(thumbnail_list);
 wchar_t query[512];GetWindowTextW(search_edit,query,512);
 if(!catalog_search(query,&photo_paths,&photo_tags,&photo_count)){set_status(L"Catalog search failed.");return;}
 for(int i=0;i<photo_count;i++){
  LVITEMW item={0};item.mask=LVIF_TEXT|LVIF_IMAGE;item.iItem=i;item.pszText=PathFindFileNameW(photo_paths[i]);
  HBITMAP bitmap=image_thumbnail(photo_paths[i],88);HICON icon=NULL;
  if(bitmap){ICONINFO info={0};info.fIcon=TRUE;info.hbmColor=bitmap;info.hbmMask=bitmap;icon=CreateIconIndirect(&info);DeleteObject(bitmap);}
  item.iImage=icon?ImageList_AddIcon(thumbnail_list,icon):-1;if(icon)DestroyIcon(icon);ListView_InsertItem(photo_list,&item);
 }
 wchar_t message[128];swprintf(message,128,L"%d image(s) in the catalog.",photo_count);set_status(message);
 InvalidateRect(preview_window,NULL,TRUE);
}
static void browse_folder(void){
 BROWSEINFOW bi={0};bi.hwndOwner=main_window;bi.lpszTitle=L"Choose a folder containing photos";bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
 PIDLIST_ABSOLUTE id=SHBrowseForFolderW(&bi);if(!id)return;wchar_t folder[MAX_PATH];
 if(SHGetPathFromIDListW(id,folder)){SetWindowTextW(path_edit,folder);int n=catalog_add_tree(folder);wchar_t m[128];swprintf(m,128,L"Indexed %d image(s) from folder.",n);set_status(m);refresh_catalog();}
 CoTaskMemFree(id);
}
static void add_files(void){
 wchar_t buffer[32768]=L"";OPENFILENAMEW ofn={0};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=main_window;
 ofn.lpstrFilter=L"Image files\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff;*.ico;*.wdp;*.heic;*.webp\0All files\0*.*\0";
 ofn.lpstrFile=buffer;ofn.nMaxFile=32768;ofn.Flags=OFN_EXPLORER|OFN_ALLOWMULTISELECT|OFN_FILEMUSTEXIST;
 if(!GetOpenFileNameW(&ofn))return;
 if(buffer[wcslen(buffer)+1]==0)catalog_add(buffer);
 else{wchar_t folder[MAX_PATH];lstrcpynW(folder,buffer,MAX_PATH);wchar_t *name=buffer+wcslen(buffer)+1;
  while(*name){wchar_t full[MAX_PATH];lstrcpynW(full,folder,MAX_PATH);PathAppendW(full,name);catalog_add(full);name+=wcslen(name)+1;}}
 refresh_catalog();
}
static void apply_tags(void){
 int i=ListView_GetNextItem(photo_list,-1,LVNI_SELECTED);if(i<0){set_status(L"Select a photo first.");return;}
 wchar_t tags[512];GetWindowTextW(tag_edit,tags,512);if(!tags[0]){set_status(L"Enter one or more comma-separated tags.");return;}
 catalog_apply_tags(photo_paths[i],tags);SetWindowTextW(tag_edit,L"");refresh_catalog();set_status(L"Tags saved.");
}
static LRESULT CALLBACK preview_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT rc;GetClientRect(w,&rc);FillRect(dc,&rc,(HBRUSH)GetStockObject(BLACK_BRUSH));if(selected_index>=0&&selected_index<photo_count)image_draw(dc,&rc,photo_paths[selected_index],0);EndPaint(w,&ps);return 0;}
 return DefWindowProcW(w,msg,wp,lp);
}
static LRESULT CALLBACK viewer_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE:viewer_index=(int)(INT_PTR)((CREATESTRUCTW*)lp)->lpCreateParams;viewer_rotation=0;return 0;
 case WM_KEYDOWN:
  if(wp==VK_ESCAPE){DestroyWindow(w);return 0;}
  if(wp==VK_RIGHT&&photo_count){viewer_index=(viewer_index+1)%photo_count;viewer_rotation=0;SetWindowTextW(w,PathFindFileNameW(photo_paths[viewer_index]));InvalidateRect(w,NULL,TRUE);}
  if(wp==VK_LEFT&&photo_count){viewer_index=(viewer_index+photo_count-1)%photo_count;viewer_rotation=0;SetWindowTextW(w,PathFindFileNameW(photo_paths[viewer_index]));InvalidateRect(w,NULL,TRUE);}
  if(wp=='R'){viewer_rotation=(viewer_rotation+90)%360;InvalidateRect(w,NULL,TRUE);}return 0;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT rc;GetClientRect(w,&rc);FillRect(dc,&rc,(HBRUSH)GetStockObject(BLACK_BRUSH));if(photo_count)image_draw(dc,&rc,photo_paths[viewer_index],viewer_rotation);EndPaint(w,&ps);return 0;}
 }return DefWindowProcW(w,msg,wp,lp);
}
static void show_viewer(void){
 int i=ListView_GetNextItem(photo_list,-1,LVNI_SELECTED);if(i<0)return;
 WNDCLASSW wc={0};wc.lpfnWndProc=viewer_proc;wc.hInstance=app_instance;wc.lpszClassName=L"TagExplorerNativeViewer";wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClassW(&wc);
 HWND v=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,PathFindFileNameW(photo_paths[i]),WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,1100,760,main_window,NULL,app_instance,(LPVOID)(INT_PTR)i);
 ShowWindow(v,SW_MAXIMIZE);SetFocus(v);
}
static LRESULT CALLBACK main_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE:{
  main_window=w;HFONT font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
  path_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,10,10,550,26,w,(HMENU)(INT_PTR)IDC_PATH,app_instance,NULL);
  CreateWindowW(L"BUTTON",L"Folder",WS_CHILD|WS_VISIBLE|WS_TABSTOP,568,10,76,26,w,(HMENU)(INT_PTR)IDC_FOLDER,app_instance,NULL);
  CreateWindowW(L"BUTTON",L"Add files",WS_CHILD|WS_VISIBLE|WS_TABSTOP,650,10,82,26,w,(HMENU)(INT_PTR)IDC_ADD,app_instance,NULL);
  search_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,10,44,360,26,w,(HMENU)(INT_PTR)IDC_SEARCH,app_instance,NULL);
  CreateWindowW(L"BUTTON",L"Search",WS_CHILD|WS_VISIBLE|WS_TABSTOP,378,44,72,26,w,(HMENU)(INT_PTR)IDC_REFRESH,app_instance,NULL);
  tag_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,460,44,260,26,w,(HMENU)IDC_TAG,app_instance,NULL);
  CreateWindowW(L"BUTTON",L"Apply tags",WS_CHILD|WS_VISIBLE|WS_TABSTOP,728,44,90,26,w,(HMENU)(INT_PTR)(IDC_TAG+100),app_instance,NULL);
  CreateWindowW(L"BUTTON",L"View",WS_CHILD|WS_VISIBLE|WS_TABSTOP,826,44,70,26,w,(HMENU)(INT_PTR)IDC_OPEN,app_instance,NULL);
  photo_list=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_ICON|LVS_SHOWSELALWAYS,10,80,680,540,w,(HMENU)(INT_PTR)IDC_LIST,app_instance,NULL);
  ListView_SetExtendedListViewStyle(photo_list,LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
  thumbnail_list=ImageList_Create(96,96,ILC_COLOR32|ILC_MASK,64,64);ListView_SetImageList(photo_list,thumbnail_list,LVSIL_NORMAL);
  WNDCLASSW pc={0};pc.lpfnWndProc=preview_proc;pc.hInstance=app_instance;pc.lpszClassName=PREVIEW_CLASS;pc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClassW(&pc);
  preview_window=CreateWindowW(PREVIEW_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_BORDER,700,80,380,540,w,(HMENU)(INT_PTR)IDC_PREVIEW,app_instance,NULL);
  status_text=CreateWindowW(L"STATIC",L"Ready",WS_CHILD|WS_VISIBLE,10,630,1000,24,w,(HMENU)(INT_PTR)IDC_STATUS,app_instance,NULL);
  HWND controls[]={path_edit,search_edit,tag_edit,photo_list,status_text};for(int k=0;k<5;k++)SendMessageW(controls[k],WM_SETFONT,(WPARAM)font,TRUE);
  if(!catalog_open())MessageBoxW(w,L"Could not open the photo catalog.",L"Tag Explorer",MB_ICONERROR);
  refresh_catalog();return 0;
 }
 case WM_SIZE:{int width=LOWORD(lp),height=HIWORD(lp);MoveWindow(path_edit,10,10,width-500,26,TRUE);MoveWindow(search_edit,10,44,width/3,26,TRUE);MoveWindow(tag_edit,width/3+120,44,width/3,26,TRUE);MoveWindow(photo_list,10,80,width*2/3-20,height-120,TRUE);MoveWindow(preview_window,width*2/3,80,width/3-20,height-120,TRUE);MoveWindow(status_text,10,height-30,width-20,22,TRUE);return 0;}
 case WM_COMMAND:
  switch(LOWORD(wp)){case IDC_FOLDER:browse_folder();break;case IDC_ADD:add_files();break;case IDC_REFRESH:refresh_catalog();break;case IDC_TAG+100:apply_tags();break;case IDC_OPEN:show_viewer();break;}return 0;
 case WM_NOTIFY:
  if(((NMHDR*)lp)->idFrom==IDC_LIST){NMHDR *h=(NMHDR*)lp;if(h->code==NM_DBLCLK)show_viewer();if(h->code==LVN_ITEMCHANGED){NMLISTVIEW *n=(NMLISTVIEW*)lp;if(n->uNewState&LVIS_SELECTED){selected_index=n->iItem;InvalidateRect(preview_window,NULL,TRUE);}}}return 0;
 case WM_DESTROY:free_results();if(thumbnail_list)ImageList_Destroy(thumbnail_list);catalog_close();PostQuitMessage(0);return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command_line,int show){
 (void)previous;(void)command_line;app_instance=instance;HRESULT com=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);(void)com;
 INITCOMMONCONTROLSEX cc={sizeof(cc),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&cc);
 WNDCLASSW wc={0};wc.lpfnWndProc=main_proc;wc.hInstance=instance;wc.lpszClassName=MAIN_CLASS;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&wc);
 HWND w=CreateWindowExW(0,MAIN_CLASS,L"Tag Explorer — Native C",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1120,740,NULL,NULL,instance,NULL);
 ShowWindow(w,show);UpdateWindow(w);MSG m;while(GetMessageW(&m,NULL,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}CoUninitialize();return (int)m.wParam;
}
