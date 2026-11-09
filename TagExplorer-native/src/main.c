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

#define MAIN_CLASS L"TagExplorerNativeMain"
#define VIEWER_CLASS L"TagExplorerNativeViewer"
#define SIDEBAR_W 278
#define TOP_H 66
#define TOOL_H 54
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
#define ID_VIEW_ROTATE_LEFT 1201
#define ID_VIEW_ROTATE_RIGHT 1202
#define ID_VIEW_RENAME 1203
#define ID_VIEW_TAGS 1204
#define ID_VIEW_APPLY_TAGS 1205
static HWND viewer_tags_edit;

static HINSTANCE app_instance;
static HWND main_window,sidebar,source_list,search_edit,tag_edit,photo_list,status_text;
static HWND title_label,subtitle_label,brand_label,source_heading,tag_heading;
static wchar_t **photo_paths,**photo_tags;
static int photo_count,viewer_index,viewer_rotation;
static HIMAGELIST thumbnail_list;
static HFONT font_ui,font_small,font_title;
static COLORREF bg_main=RGB(31,31,33),bg_panel=RGB(43,43,46),bg_input=RGB(55,55,59);
static COLORREF fg_main=RGB(238,238,240),fg_muted=RGB(151,153,158),accent=RGB(79,174,184);

static void set_status(const wchar_t *s){if(status_text)SetWindowTextW(status_text,s);}
static void free_results(void){if(photo_paths)catalog_free_results(photo_paths,photo_tags,photo_count);photo_paths=photo_tags=NULL;photo_count=0;}
static HWND make_label(HWND parent,const wchar_t *text,int x,int y,int w,int h,HFONT font,COLORREF color){
 HWND hnd=CreateWindowExW(0,L"STATIC",text,WS_CHILD|WS_VISIBLE|SS_LEFT,x,y,w,h,parent,NULL,app_instance,NULL);
 SendMessageW(hnd,WM_SETFONT,(WPARAM)font,TRUE);SetPropW(hnd,L"TagExplorerTextColor",(HANDLE)(ULONG_PTR)(color+1));return hnd;
}
static HWND make_button(HWND parent,const wchar_t *text,int id,int x,int y,int w,int h){
 HWND b=CreateWindowExW(0,L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,x,y,w,h,parent,(HMENU)(INT_PTR)id,app_instance,NULL);
 SendMessageW(b,WM_SETFONT,(WPARAM)font_ui,TRUE);SetWindowTheme(b,L"Explorer",NULL);return b;
}
static void refresh_catalog(void){
 free_results();ListView_DeleteAllItems(photo_list);ImageList_RemoveAll(thumbnail_list);
 wchar_t query[512];GetWindowTextW(search_edit,query,512);
 if(!catalog_search(query,&photo_paths,&photo_tags,&photo_count)){set_status(L"Unable to read the photo catalog.");return;}
 for(int i=0;i<photo_count;i++){
  wchar_t caption[MAX_PATH+160];const wchar_t *name=PathFindFileNameW(photo_paths[i]);
  if(photo_tags[i]&&photo_tags[i][0])swprintf(caption,MAX_PATH+160,L"%s\n%s",name,photo_tags[i]);
  else lstrcpynW(caption,name,MAX_PATH+160);
  HBITMAP bitmap=image_thumbnail(photo_paths[i],300);HICON icon=NULL;
  if(bitmap){ICONINFO info={0};info.fIcon=TRUE;info.hbmColor=bitmap;info.hbmMask=bitmap;icon=CreateIconIndirect(&info);DeleteObject(bitmap);}
  int image=icon?ImageList_AddIcon(thumbnail_list,icon):-1;if(icon)DestroyIcon(icon);
  LVITEMW item={0};item.mask=LVIF_TEXT|LVIF_IMAGE;item.iItem=i;item.pszText=caption;item.iImage=image;
  ListView_InsertItem(photo_list,&item);
 }
 wchar_t message[160];swprintf(message,160,L"%d photos",photo_count);set_status(message);
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
static void viewer_rename(HWND w){
 if(viewer_index<0||viewer_index>=photo_count)return;
 wchar_t target[MAX_PATH];lstrcpynW(target,PathFindFileNameW(photo_paths[viewer_index]),MAX_PATH);
 OPENFILENAMEW ofn={0};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=w;ofn.lpstrFile=target;ofn.nMaxFile=MAX_PATH;
 wchar_t initial_dir[MAX_PATH];lstrcpynW(initial_dir,photo_paths[viewer_index],MAX_PATH);PathRemoveFileSpecW(initial_dir);ofn.lpstrInitialDir=initial_dir;
 ofn.lpstrFilter=L"Image files\\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff;*.webp;*.heic\\0All files\\0*.*\\0";
 ofn.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 if(!GetSaveFileNameW(&ofn))return;
 if(!_wcsicmp(target,photo_paths[viewer_index]))return;
 if(GetFileAttributesW(target)!=INVALID_FILE_ATTRIBUTES){MessageBoxW(w,L"A file with that name already exists.",L"Rename photo",MB_ICONWARNING);return;}
 wchar_t old[MAX_PATH];lstrcpynW(old,photo_paths[viewer_index],MAX_PATH);
 if(!MoveFileW(old,target)){MessageBoxW(w,L"Windows could not rename this photo.",L"Rename photo",MB_ICONERROR);return;}
 if(!catalog_rename(old,target)){MoveFileW(target,old);MessageBoxW(w,L"The catalog could not update the renamed photo.",L"Rename photo",MB_ICONERROR);return;}
 free(photo_paths[viewer_index]);photo_paths[viewer_index]=_wcsdup(target);
 SetWindowTextW(w,PathFindFileNameW(target));set_status(L"Photo renamed.");InvalidateRect(w,NULL,TRUE);
}
static LRESULT CALLBACK viewer_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE:
  make_button(w,L"Rotate left",ID_VIEW_ROTATE_LEFT,16,12,104,34);
  make_button(w,L"Rotate right",ID_VIEW_ROTATE_RIGHT,128,12,110,34);
  make_button(w,L"Rename",ID_VIEW_RENAME,248,12,86,34);
  viewer_tags_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,350,12,260,34,w,(HMENU)(INT_PTR)ID_VIEW_TAGS,app_instance,NULL);
  SendMessageW(viewer_tags_edit,WM_SETFONT,(WPARAM)font_ui,TRUE);
  make_button(w,L"Add tags",ID_VIEW_APPLY_TAGS,620,12,96,34);
  return 0;
 case WM_SIZE:{
  int width=LOWORD(lp);MoveWindow(GetDlgItem(w,ID_VIEW_ROTATE_LEFT),16,12,104,34,TRUE);
  MoveWindow(GetDlgItem(w,ID_VIEW_ROTATE_RIGHT),128,12,110,34,TRUE);
  MoveWindow(GetDlgItem(w,ID_VIEW_RENAME),248,12,86,34,TRUE);
  MoveWindow(viewer_tags_edit,350,12,width-462>180?width-462:180,34,TRUE);
  MoveWindow(GetDlgItem(w,ID_VIEW_APPLY_TAGS),width-96,12,80,34,TRUE);return 0;}
 case WM_COMMAND:
  switch(LOWORD(wp)){
   case ID_VIEW_ROTATE_LEFT:viewer_rotation=(viewer_rotation+270)%360;InvalidateRect(w,NULL,TRUE);return 0;
   case ID_VIEW_ROTATE_RIGHT:viewer_rotation=(viewer_rotation+90)%360;InvalidateRect(w,NULL,TRUE);return 0;
   case ID_VIEW_RENAME:viewer_rename(w);return 0;
   case ID_VIEW_APPLY_TAGS:{wchar_t tags[512];GetWindowTextW(viewer_tags_edit,tags,512);if(tags[0]&&catalog_apply_tags(photo_paths[viewer_index],tags)){SetWindowTextW(viewer_tags_edit,L"");set_status(L"Tags added to this photo.");}else set_status(L"Enter one or more comma-separated tags.");return 0;}
  }break;
 case WM_KEYDOWN:
  if(wp==VK_ESCAPE){DestroyWindow(w);return 0;}
  if(wp==VK_RIGHT&&photo_count){viewer_index=(viewer_index+1)%photo_count;viewer_rotation=0;SetWindowTextW(w,PathFindFileNameW(photo_paths[viewer_index]));InvalidateRect(w,NULL,TRUE);}
  if(wp==VK_LEFT&&photo_count){viewer_index=(viewer_index+photo_count-1)%photo_count;viewer_rotation=0;SetWindowTextW(w,PathFindFileNameW(photo_paths[viewer_index]));InvalidateRect(w,NULL,TRUE);}
  if(wp=='R'){viewer_rotation=(viewer_rotation+90)%360;InvalidateRect(w,NULL,TRUE);}return 0;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT rc;GetClientRect(w,&rc);FillRect(dc,&rc,(HBRUSH)GetStockObject(BLACK_BRUSH));rc.top+=58;if(photo_count)image_draw(dc,&rc,photo_paths[viewer_index],viewer_rotation);EndPaint(w,&ps);return 0;}
 }return DefWindowProcW(w,msg,wp,lp);
}
static void show_viewer(void){
 int i=ListView_GetNextItem(photo_list,-1,LVNI_SELECTED);if(i<0)return;
 WNDCLASSW wc={0};wc.lpfnWndProc=viewer_proc;wc.hInstance=app_instance;wc.lpszClassName=VIEWER_CLASS;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClassW(&wc);
 HWND v=CreateWindowExW(WS_EX_APPWINDOW,VIEWER_CLASS,PathFindFileNameW(photo_paths[i]),WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,1100,760,main_window,NULL,app_instance,NULL);
 viewer_index=i;viewer_rotation=0;ShowWindow(v,SW_MAXIMIZE);SetFocus(v);
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
static LRESULT CALLBACK main_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE:{
  main_window=w;font_ui=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
  font_small=CreateFontW(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
  font_title=CreateFontW(-23,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Segoe UI");
  sidebar=NULL;
  brand_label=make_label(w,L"◉  TAG EXPLORER",24,22,225,34,font_title,fg_main);
  source_heading=make_label(w,L"LIBRARY",24,92,220,20,font_small,fg_muted);
  make_button(w,L"+  Add folder",ID_ADD_FOLDER,20,122,238,38);
  make_button(w,L"+  Add photos",ID_ADD_FILES,20,168,238,38);
  make_label(w,L"SOURCES",24,232,220,20,font_small,fg_muted);
  source_list=CreateWindowExW(0,L"LISTBOX",L"",WS_CHILD|WS_VISIBLE|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL,18,260,242,126,w,(HMENU)(INT_PTR)ID_SOURCE_LIST,app_instance,NULL);
  SendMessageW(source_list,WM_SETFONT,(WPARAM)font_ui,TRUE);MoveWindow(source_list,18,260,242,126,TRUE);SendMessageW(source_list,LB_ADDSTRING,0,(LPARAM)L"  All photos");
  make_label(w,L"ORGANIZE",24,414,220,20,font_small,fg_muted);
  make_label(w,L"Tags are stored locally in your catalog.",24,446,230,40,font_small,fg_muted);
  make_button(w,L"Install Explorer menu",ID_SHELL,20,510,238,36);
  title_label=make_label(w,L"Photo library",SIDEBAR_W+28,15,420,28,font_title,fg_main);
  subtitle_label=make_label(w,L"Browse, search and organize your collection",SIDEBAR_W+29,42,450,18,font_small,fg_muted);
  search_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,0,0,250,32,w,(HMENU)(INT_PTR)ID_SEARCH,app_instance,NULL);
  SendMessageW(search_edit,WM_SETFONT,(WPARAM)font_ui,TRUE);
  make_button(w,L"Search",ID_REFRESH,0,0,74,32);
  tag_edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,0,0,250,32,w,(HMENU)(INT_PTR)ID_TAG,app_instance,NULL);
  SendMessageW(tag_edit,WM_SETFONT,(WPARAM)font_ui,TRUE);
  make_button(w,L"Apply tags",ID_APPLY_TAGS,0,0,100,32);
  make_button(w,L"Open photo",ID_OPEN,0,0,100,32);
  photo_list=CreateWindowExW(0,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_ICON|LVS_SHOWSELALWAYS|LVS_AUTOARRANGE,0,0,800,600,w,(HMENU)(INT_PTR)ID_LIST,app_instance,NULL);
  ListView_SetExtendedListViewStyle(photo_list,LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP|LVS_EX_BORDERSELECT);ListView_SetBkColor(photo_list,bg_main);ListView_SetTextBkColor(photo_list,bg_main);ListView_SetTextColor(photo_list,fg_main);
  thumbnail_list=ImageList_Create(176,136,ILC_COLOR32|ILC_MASK,96,32);ListView_SetImageList(photo_list,thumbnail_list,LVSIL_NORMAL);
  status_text=make_label(w,L"Ready",0,0,500,20,font_small,fg_muted);
  if(!catalog_open())MessageBoxW(w,L"Could not open the local photo catalog.",L"Tag Explorer",MB_ICONERROR);
  refresh_catalog();return 0;
 }
 case WM_SIZE:layout_children(LOWORD(lp),HIWORD(lp));{
  int width=LOWORD(lp);int content=width-SIDEBAR_W;
  MoveWindow(search_edit,width-360,17,230,32,TRUE);
  MoveWindow(GetDlgItem(w,ID_REFRESH),width-120,17,82,32,TRUE);
  MoveWindow(tag_edit,SIDEBAR_W+22,TOP_H+10,260,32,TRUE);
  MoveWindow(GetDlgItem(w,ID_APPLY_TAGS),SIDEBAR_W+292,TOP_H+10,104,32,TRUE);
  MoveWindow(GetDlgItem(w,ID_OPEN),SIDEBAR_W+406,TOP_H+10,112,32,TRUE);
  (void)content;
  }return 0;
 case WM_COMMAND:
  switch(LOWORD(wp)){case ID_ADD_FOLDER:browse_folder();break;case ID_ADD_FILES:add_files();break;case ID_REFRESH:refresh_catalog();break;case ID_APPLY_TAGS:apply_tags();break;case ID_OPEN:show_viewer();break;case ID_SHELL:install_shell_menu();break;case ID_SEARCH:if(HIWORD(wp)==EN_CHANGE){}break;case ID_SOURCE_LIST:if(HIWORD(wp)==LBN_SELCHANGE)refresh_catalog();break;}return 0;
 case WM_NOTIFY:
  if(((NMHDR*)lp)->idFrom==ID_LIST&&((NMHDR*)lp)->code==NM_DBLCLK)show_viewer();return 0;
 case WM_CTLCOLORSTATIC:{HDC dc=(HDC)wp;HWND ctl=(HWND)lp;SetBkMode(dc,TRANSPARENT);HANDLE c=GetPropW(ctl,L"TagExplorerTextColor");SetTextColor(dc,c?(COLORREF)((ULONG_PTR)c-1):fg_main);return (LRESULT)GetStockObject(NULL_BRUSH);}
 case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,fg_main);SetBkColor(dc,bg_input);return (LRESULT)GetStockObject(DKGRAY_BRUSH);}
 case WM_CTLCOLORLISTBOX:{HDC dc=(HDC)wp;SetTextColor(dc,fg_main);SetBkColor(dc,bg_panel);return (LRESULT)GetStockObject(DKGRAY_BRUSH);}
 case WM_ERASEBKGND:{RECT rc;GetClientRect(w,&rc);HBRUSH base=CreateSolidBrush(bg_main);FillRect((HDC)wp,&rc,base);DeleteObject(base);RECT side={0,0,SIDEBAR_W,rc.bottom};HBRUSH panel=CreateSolidBrush(bg_panel);FillRect((HDC)wp,&side,panel);DeleteObject(panel);return 1;}
 case WM_DESTROY:free_results();if(thumbnail_list)ImageList_Destroy(thumbnail_list);catalog_close();if(font_ui)DeleteObject(font_ui);if(font_small)DeleteObject(font_small);if(font_title)DeleteObject(font_title);PostQuitMessage(0);return 0;
 }return DefWindowProcW(w,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command_line,int show){
 (void)previous;(void)command_line;app_instance=instance;CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
 INITCOMMONCONTROLSEX cc={sizeof(cc),ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&cc);
 WNDCLASSW wc={0};wc.lpfnWndProc=main_proc;wc.hInstance=instance;wc.lpszClassName=MAIN_CLASS;wc.hCursor=LoadCursorW(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClassW(&wc);
 HWND w=CreateWindowExW(0,MAIN_CLASS,L"Tag Explorer",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1450,920,NULL,NULL,instance,NULL);
 ShowWindow(w,show);UpdateWindow(w);MSG m;while(GetMessageW(&m,NULL,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}CoUninitialize();return (int)m.wParam;
}