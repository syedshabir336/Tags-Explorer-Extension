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
 HBITMAP bmp=image_thumbnail(path,2400);if(!bmp)return E_FAIL;BITMAP b={0};GetObjectW(bmp,sizeof(b),&b);
 int boxw=area->right-area->left,boxh=area->bottom-area->top;double a=(double)boxw/b.bmWidth,c=(double)boxh/b.bmHeight,scale=a<c?a:c;
 int dw=(int)(b.bmWidth*scale),dh=(int)(b.bmHeight*scale);int x=area->left+(boxw-dw)/2,y=area->top+(boxh-dh)/2;
 HDC mem=CreateCompatibleDC(dc);HGDIOBJ old=SelectObject(mem,bmp);SetStretchBltMode(dc,HALFTONE);
 if(rotation%180==0)StretchBlt(dc,x,y,dw,dh,mem,0,0,b.bmWidth,b.bmHeight,SRCCOPY);
 else {int rw=dh,rh=dw;XFORM xf={0};double rad=rotation*3.141592653589793/180.0;xf.eM11=(FLOAT)cos(rad);xf.eM12=(FLOAT)sin(rad);xf.eM21=(FLOAT)-sin(rad);xf.eM22=(FLOAT)cos(rad);xf.eDx=(FLOAT)(area->left+boxw/2);xf.eDy=(FLOAT)(area->top+boxh/2);SetGraphicsMode(dc,GM_ADVANCED);SetWorldTransform(dc,&xf);StretchBlt(dc,-rw/2,-rh/2,rw,rh,mem,0,0,b.bmWidth,b.bmHeight,SRCCOPY);ModifyWorldTransform(dc,NULL,MWT_IDENTITY);}
 SelectObject(mem,old);DeleteDC(mem);DeleteObject(bmp);return S_OK;
}
