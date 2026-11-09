#define COBJMACROS
#include "images.h"
#include <wincodecsdk.h>
#include <math.h>
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
HRESULT image_draw(HDC dc,const RECT *area,const wchar_t *path,int rotation){
 HBITMAP bmp=image_thumbnail(path,2400);if(!bmp)return E_FAIL;
 BITMAP b={0};if(!GetObjectW(bmp,sizeof(b),&b)){DeleteObject(bmp);return E_FAIL;}
 int boxw=area->right-area->left,boxh=area->bottom-area->top;
 if(boxw<=0||boxh<=0||b.bmWidth<=0||b.bmHeight<=0){DeleteObject(bmp);return E_INVALIDARG;}
 /* Fit against the post-rotation bounding box, not the source dimensions. */
 int angle=rotation%360;if(angle<0)angle+=360;
 int quarter_turn=(angle==90||angle==270);
 double rotated_w=(double)(quarter_turn?b.bmHeight:b.bmWidth);
 double rotated_h=(double)(quarter_turn?b.bmWidth:b.bmHeight);
 double sx=(double)boxw/rotated_w,sy=(double)boxh/rotated_h;
 double scale=sx<sy?sx:sy;
 int dw=(int)(b.bmWidth*scale+0.5),dh=(int)(b.bmHeight*scale+0.5);
 if(dw<1)dw=1;if(dh<1)dh=1;
 HDC mem=CreateCompatibleDC(dc);if(!mem){DeleteObject(bmp);return E_FAIL;}
 HGDIOBJ old=SelectObject(mem,bmp);if(!old||old==HGDI_ERROR){DeleteDC(mem);DeleteObject(bmp);return E_FAIL;}
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
 SelectObject(mem,old);DeleteDC(mem);DeleteObject(bmp);return hr;
}
