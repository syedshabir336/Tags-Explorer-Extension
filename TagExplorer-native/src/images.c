#define COBJMACROS
#include "images.h"
#include <wincodecsdk.h>
#include <math.h>
#include <shlwapi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>
static IWICImagingFactory *factory;
static HRESULT factory_get(void){if(factory)return S_OK;return CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void**)&factory);}
static IWICBitmapSource *decode_scaled(const wchar_t *path,UINT edge){
 if(FAILED(factory_get()))return NULL;
 IWICBitmapDecoder *dec=NULL;IWICBitmapFrameDecode *frame=NULL;IWICBitmapScaler *scaler=NULL;IWICFormatConverter *conv=NULL;IWICBitmapSource *result=NULL;
 if(FAILED(IWICImagingFactory_CreateDecoderFromFilename(factory,path,NULL,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&dec)))goto done;
 if(FAILED(IWICBitmapDecoder_GetFrame(dec,0,&frame)))goto done;
 UINT w=0,h=0;IWICBitmapFrameDecode_GetSize(frame,&w,&h);if(!w||!h)goto done;
 double factor=(double)edge/(double)(w>h?w:h);UINT nw=(UINT)(w*factor),nh=(UINT)(h*factor);if(!nw)nw=1;if(!nh)nh=1;
 if(FAILED(IWICImagingFactory_CreateBitmapScaler(factory,&scaler)))goto done;
 if(FAILED(IWICBitmapScaler_Initialize(scaler,(IWICBitmapSource*)frame,nw,nh,WICBitmapInterpolationModeFant)))goto done;
 if(FAILED(IWICImagingFactory_CreateFormatConverter(factory,&conv)))goto done;
 if(FAILED(IWICFormatConverter_Initialize(conv,(IWICBitmapSource*)scaler,&GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom)))goto done;
 result=(IWICBitmapSource*)conv;conv=NULL;
done:
 if(conv)IWICFormatConverter_Release(conv);if(scaler)IWICBitmapScaler_Release(scaler);if(frame)IWICBitmapFrameDecode_Release(frame);if(dec)IWICBitmapDecoder_Release(dec);return result;
}
HBITMAP image_thumbnail(const wchar_t *path,UINT edge){
 IWICBitmapSource *src=decode_scaled(path,edge);if(!src)return NULL;UINT w=0,h=0;IWICBitmapSource_GetSize(src,&w,&h);
 BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=(LONG)w;bi.bmiHeader.biHeight=-(LONG)h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 void *bits=NULL;HBITMAP bmp=CreateDIBSection(NULL,&bi,DIB_RGB_COLORS,&bits,NULL,0);if(bmp&&bits)IWICBitmapSource_CopyPixels(src,NULL,w*4,w*h*4,(BYTE*)bits);IWICBitmapSource_Release(src);return bmp;
}
static wchar_t g_thumb_cache_dir[MAX_PATH];
static int g_thumb_cache_ready;
static void thumbnail_cache_init(void){
 if(g_thumb_cache_ready)return;g_thumb_cache_ready=1;
 if(!GetEnvironmentVariableW(L"LOCALAPPDATA",g_thumb_cache_dir,MAX_PATH))return;
 if(!PathAppendW(g_thumb_cache_dir,L"TagExplorer\\ThumbCache"))return;
 wchar_t parent[MAX_PATH];lstrcpynW(parent,g_thumb_cache_dir,MAX_PATH);PathRemoveFileSpecW(parent);CreateDirectoryW(parent,NULL);CreateDirectoryW(g_thumb_cache_dir,NULL);
}
static uint64_t thumbnail_hash(const wchar_t *path,const WIN32_FILE_ATTRIBUTE_DATA *a,UINT edge){
 uint64_t h=1469598103934665603ULL;for(const wchar_t *p=path;*p;p++){wchar_t c=(wchar_t)towlower(*p);h^=(uint16_t)c;h*=1099511628211ULL;}
 h^=a->nFileSizeHigh;h*=1099511628211ULL;h^=a->nFileSizeLow;h*=1099511628211ULL;
 h^=a->ftLastWriteTime.dwHighDateTime;h*=1099511628211ULL;h^=a->ftLastWriteTime.dwLowDateTime;h*=1099511628211ULL;h^=edge;return h;
}
static void thumbnail_cache_save(const wchar_t *file,HBITMAP bmp){
 BITMAP b={0};if(!GetObjectW(bmp,sizeof(b),&b)||b.bmWidth<=0||b.bmHeight==0)return;
 BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=b.bmWidth;bi.bmiHeader.biHeight=-abs(b.bmHeight);bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 DWORD bytes=(DWORD)b.bmWidth*(DWORD)abs(b.bmHeight)*4;BYTE *pixels=(BYTE*)malloc(bytes);if(!pixels)return;
 HDC dc=GetDC(NULL);int lines=GetDIBits(dc,bmp,0,(UINT)abs(b.bmHeight),pixels,&bi,DIB_RGB_COLORS);ReleaseDC(NULL,dc);
 if(lines==(int)abs(b.bmHeight)){
  BITMAPFILEHEADER fh={0};fh.bfType=0x4D42;fh.bfOffBits=sizeof(fh)+sizeof(BITMAPINFOHEADER);fh.bfSize=fh.bfOffBits+bytes;
  HANDLE f=CreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,NULL);
  if(f!=INVALID_HANDLE_VALUE){DWORD written;WriteFile(f,&fh,sizeof(fh),&written,NULL);WriteFile(f,&bi.bmiHeader,sizeof(BITMAPINFOHEADER),&written,NULL);WriteFile(f,pixels,bytes,&written,NULL);CloseHandle(f);}
 }
 free(pixels);
}
HBITMAP image_thumbnail_cached(const wchar_t *path,UINT edge){
 WIN32_FILE_ATTRIBUTE_DATA attrs;if(!GetFileAttributesExW(path,GetFileExInfoStandard,&attrs))return NULL;
 thumbnail_cache_init();if(!g_thumb_cache_dir[0])return image_thumbnail(path,edge);
 uint64_t key=thumbnail_hash(path,&attrs,edge);wchar_t file[MAX_PATH],name[40];swprintf(name,40,L"%016llX.bmp",(unsigned long long)key);
 lstrcpynW(file,g_thumb_cache_dir,MAX_PATH);if(!PathAppendW(file,name))return image_thumbnail(path,edge);
 HBITMAP cached=(HBITMAP)LoadImageW(NULL,file,IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION);
 if(cached)return cached;
 HBITMAP bmp=image_thumbnail(path,edge);if(bmp)thumbnail_cache_save(file,bmp);return bmp;
}
static HBITMAP g_view_cache;
static wchar_t g_view_cache_path[MAX_PATH];
static HBITMAP image_view_bitmap(const wchar_t *path){
 if(g_view_cache&&_wcsicmp(g_view_cache_path,path)==0)return g_view_cache;
 if(g_view_cache){DeleteObject(g_view_cache);g_view_cache=NULL;}
 g_view_cache=image_thumbnail(path,2400);
 if(g_view_cache)lstrcpynW(g_view_cache_path,path,MAX_PATH);else g_view_cache_path[0]=0;
 return g_view_cache;
}
void image_invalidate_cache(const wchar_t *path){
 if(!path||!g_view_cache_path[0]||_wcsicmp(g_view_cache_path,path)==0){
  if(g_view_cache)DeleteObject(g_view_cache);g_view_cache=NULL;g_view_cache_path[0]=0;
 }
}
HRESULT image_draw(HDC dc,const RECT *area,const wchar_t *path,int rotation){
 HBITMAP bmp=image_view_bitmap(path);if(!bmp)return E_FAIL;
 BITMAP b={0};if(!GetObjectW(bmp,sizeof(b),&b)){return E_FAIL;}
 int boxw=area->right-area->left,boxh=area->bottom-area->top;
 if(boxw<=0||boxh<=0||b.bmWidth<=0||b.bmHeight<=0){return E_INVALIDARG;}
 /* Fit against the post-rotation bounding box, not the source dimensions. */
 int angle=rotation%360;if(angle<0)angle+=360;
 int quarter_turn=(angle==90||angle==270);
 double rotated_w=(double)(quarter_turn?b.bmHeight:b.bmWidth);
 double rotated_h=(double)(quarter_turn?b.bmWidth:b.bmHeight);
 double sx=(double)boxw/rotated_w,sy=(double)boxh/rotated_h;
 double scale=sx<sy?sx:sy;
 int dw=(int)(b.bmWidth*scale+0.5),dh=(int)(b.bmHeight*scale+0.5);
 if(dw<1)dw=1;if(dh<1)dh=1;
 HDC mem=CreateCompatibleDC(dc);if(!mem){return E_FAIL;}
 HGDIOBJ old=SelectObject(mem,bmp);if(!old||old==HGDI_ERROR){DeleteDC(mem);return E_FAIL;}
 int saved=SaveDC(dc);SetStretchBltMode(dc,HALFTONE);
 XFORM xf={0};double rad=(double)angle*3.14159265358979323846/180.0;
 xf.eM11=(FLOAT)cos(rad);xf.eM12=(FLOAT)sin(rad);
 xf.eM21=(FLOAT)-sin(rad);xf.eM22=(FLOAT)cos(rad);
 xf.eDx=(FLOAT)((area->left+area->right)/2.0);
 xf.eDy=(FLOAT)((area->top+area->bottom)/2.0);
 HRESULT hr=E_FAIL;
 if(saved&&SetGraphicsMode(dc,GM_ADVANCED)&&SetWorldTransform(dc,&xf)){
  /* Draw the unrotated source rectangle around the origin. The world
     transform rotates its true bounds (including width/height swapping). */
  if(StretchBlt(dc,-dw/2,-dh/2,dw,dh,mem,0,0,b.bmWidth,b.bmHeight,SRCCOPY))hr=S_OK;
 }
 if(saved)RestoreDC(dc,saved);
 SelectObject(mem,old);DeleteDC(mem);return hr;
}


/* Rotate the image pixels and replace the original file atomically. The decoder
   and encoder are released before replacement so Windows can rename the file. */
HRESULT image_rotate_file(const wchar_t *path, int clockwise_degrees){
 if(!path || (clockwise_degrees!=90 && clockwise_degrees!=270)) return E_INVALIDARG;
 IWICImagingFactory *local_factory=NULL; IWICBitmapDecoder *decoder=NULL; IWICBitmapFrameDecode *frame=NULL;
 HRESULT hr;
 IWICBitmapFlipRotator *rotator=NULL; IWICBitmapEncoder *encoder=NULL;
 IWICBitmapFrameEncode *out_frame=NULL; IPropertyBag2 *options=NULL;
 IWICStream *stream=NULL; GUID container={0},pixel_format={0};
 UINT width=0,height=0; double dpi_x=96.0,dpi_y=96.0;
 wchar_t folder[MAX_PATH],temp[MAX_PATH]; lstrcpynW(folder,path,MAX_PATH);
 wchar_t *slash=wcsrchr(folder,L'\\'); if(!slash) slash=wcsrchr(folder,L'/');
 if(slash) slash[1]=0; else lstrcpyW(folder,L".\\");
 if(!GetTempFileNameW(folder,L"ter",0,temp)) return HRESULT_FROM_WIN32(GetLastError());
 hr=CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void**)&local_factory);
 if(FAILED(hr)){DeleteFileW(temp);return hr;}
 hr=IWICImagingFactory_CreateDecoderFromFilename(local_factory,path,NULL,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder);
 if(FAILED(hr)) goto done;
 hr=IWICBitmapDecoder_GetContainerFormat(decoder,&container); if(FAILED(hr)) goto done;
 hr=IWICBitmapDecoder_GetFrame(decoder,0,&frame); if(FAILED(hr)) goto done;
 hr=IWICBitmapFrameDecode_GetSize(frame,&width,&height); if(FAILED(hr)) goto done;
 IWICBitmapFrameDecode_GetResolution(frame,&dpi_x,&dpi_y);
 hr=IWICImagingFactory_CreateBitmapFlipRotator(local_factory,&rotator); if(FAILED(hr)) goto done;
 hr=IWICBitmapFlipRotator_Initialize(rotator,(IWICBitmapSource*)frame,
      clockwise_degrees==90?WICBitmapTransformRotate90:WICBitmapTransformRotate270);
 if(FAILED(hr)) goto done;
 hr=IWICImagingFactory_CreateStream(local_factory,&stream); if(FAILED(hr)) goto done;
 hr=IWICStream_InitializeFromFilename(stream,temp,GENERIC_WRITE); if(FAILED(hr)) goto done;
 hr=IWICImagingFactory_CreateEncoder(local_factory,&container,NULL,&encoder); if(FAILED(hr)) goto done;
 hr=IWICBitmapEncoder_Initialize(encoder,(IStream*)stream,WICBitmapEncoderNoCache); if(FAILED(hr)) goto done;
 hr=IWICBitmapEncoder_CreateNewFrame(encoder,&out_frame,&options); if(FAILED(hr)) goto done;
 hr=IWICBitmapFrameEncode_Initialize(out_frame,options); if(FAILED(hr)) goto done;
 hr=IWICBitmapFrameEncode_SetSize(out_frame,clockwise_degrees==90?height:height,
                                  clockwise_degrees==90?width:width);
 if(FAILED(hr)) goto done;
 IWICBitmapSource_GetPixelFormat((IWICBitmapSource*)frame,&pixel_format);
 hr=IWICBitmapFrameEncode_SetPixelFormat(out_frame,&pixel_format); if(FAILED(hr)) goto done;
 if(dpi_x>0&&dpi_y>0) IWICBitmapFrameEncode_SetResolution(out_frame,dpi_x,dpi_y);
 hr=IWICBitmapFrameEncode_WriteSource(out_frame,(IWICBitmapSource*)rotator,NULL); if(FAILED(hr)) goto done;
 hr=IWICBitmapFrameEncode_Commit(out_frame); if(FAILED(hr)) goto done;
 hr=IWICBitmapEncoder_Commit(encoder); if(FAILED(hr)) goto done;
done:
 if(options) IPropertyBag2_Release(options);
 if(local_factory) IWICImagingFactory_Release(local_factory);
 if(out_frame) IWICBitmapFrameEncode_Release(out_frame);
 if(encoder) IWICBitmapEncoder_Release(encoder);
 if(stream) IWICStream_Release(stream);
 if(rotator) IWICBitmapFlipRotator_Release(rotator);
 if(frame) IWICBitmapFrameDecode_Release(frame);
 if(decoder) IWICBitmapDecoder_Release(decoder);
 if(SUCCEEDED(hr)){
  if(!MoveFileExW(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) hr=HRESULT_FROM_WIN32(GetLastError());
 }
 if(FAILED(hr)) DeleteFileW(temp);
 return hr;
}
