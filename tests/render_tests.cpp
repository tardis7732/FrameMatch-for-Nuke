// Exercise the production OFX entry point with a host that cancels image fetches.
#include "../src/plugin.cpp"
#include <cstdarg>
#include <iostream>

static Instance mock;
static bool cancelled=false;
static int mode=0,fetches=0,releases=0,modal=0,persistent=0,cleared=0;
static float sourcePixels[8]={.1f,.2f,.3f,1,.4f,.5f,.6f,1},outputPixels[8];
static auto effect=reinterpret_cast<OfxImageEffectHandle>(1);
static auto sourceClip=reinterpret_cast<OfxImageClipHandle>(2);
static auto outputClip=reinterpret_cast<OfxImageClipHandle>(3);
static auto sourceImage=reinterpret_cast<OfxPropertySetHandle>(4);
static auto outputImage=reinterpret_cast<OfxPropertySetHandle>(5);
static OfxStatus getEffect(OfxImageEffectHandle,OfxPropertySetHandle* p){*p=reinterpret_cast<OfxPropertySetHandle>(6);return kOfxStatOK;}
static OfxStatus getPointer(OfxPropertySetHandle p,const char* key,int,void** value){
 if(!std::strcmp(key,kOfxPropInstanceData))*value=&mock;
 else *value=p==sourceImage?sourcePixels:outputPixels;
 return kOfxStatOK;
}
static OfxStatus getString(OfxPropertySetHandle,const char* key,int,char** value){
 *value=const_cast<char*>(!std::strcmp(key,kOfxImageEffectPropComponents)?kOfxImageComponentRGBA:!std::strcmp(key,kOfxImageEffectPropPixelDepth)?kOfxBitDepthFloat:"Result");return kOfxStatOK;
}
static OfxStatus getInt(OfxPropertySetHandle,const char*,int,int* value){*value=32;return kOfxStatOK;}
static OfxStatus getInts(OfxPropertySetHandle,const char*,int,int* value){int bounds[4]={0,0,2,1};std::copy(bounds,bounds+4,value);return kOfxStatOK;}
static OfxStatus getDouble(OfxPropertySetHandle,const char* key,int,double* value){*value=!std::strcmp(key,kOfxPropTime)?42.:1.;return kOfxStatOK;}
static OfxStatus getDoubles(OfxPropertySetHandle,const char*,int count,double* value){std::fill(value,value+count,1.);return kOfxStatOK;}
static OfxStatus paramHandle(OfxParamSetHandle,const char*,OfxParamHandle* p,OfxPropertySetHandle*){*p=reinterpret_cast<OfxParamHandle>(7);return kOfxStatOK;}
static OfxStatus paramValue(OfxParamHandle p,...){va_list ap;va_start(ap,p);*va_arg(ap,int*)=0;va_end(ap);return kOfxStatOK;}
static int abortRender(OfxImageEffectHandle){return cancelled;}
static OfxStatus getClipProps(OfxImageClipHandle,OfxPropertySetHandle* p){*p=reinterpret_cast<OfxPropertySetHandle>(8);return kOfxStatOK;}
static OfxStatus fetch(OfxImageClipHandle c,OfxTime,const OfxRectD*,OfxPropertySetHandle* p){
 ++fetches;
 if(c==sourceClip){
  if(mode==1)return kOfxStatFailed;
  if(mode==2){cancelled=true;return kOfxStatFailed;}
  if(mode==5)return kOfxStatErrBadHandle;
  *p=sourceImage;
 }else{
  if(mode==3){cancelled=true;return kOfxStatFailed;}
  if(mode==4)return kOfxStatFailed;
  *p=outputImage;
 }
 return kOfxStatOK;
}
static OfxStatus release(OfxPropertySetHandle){++releases;return kOfxStatOK;}
static OfxStatus modalMessage(void*,const char*,const char*,const char*,...){++modal;return kOfxStatOK;}
static OfxStatus persistentMessage(void*,const char*,const char*,const char*,...){++persistent;return kOfxStatOK;}
static OfxStatus clearMessage(void*){++cleared;return kOfxStatOK;}
static void require(bool value,const char* what){if(!value)throw std::runtime_error(what);}
int main(){
 OfxPropertySuiteV1 ps{};ps.propGetPointer=getPointer;ps.propGetString=getString;ps.propGetInt=getInt;ps.propGetIntN=getInts;ps.propGetDouble=getDouble;ps.propGetDoubleN=getDoubles;props=&ps;
 OfxParameterSuiteV1 pa{};pa.paramGetHandle=paramHandle;pa.paramGetValue=paramValue;params=&pa;
 OfxImageEffectSuiteV1 im{};im.getPropertySet=getEffect;im.clipGetImage=fetch;im.clipReleaseImage=release;im.clipGetPropertySet=getClipProps;im.abort=abortRender;images=&im;
 OfxMessageSuiteV1 ms{};ms.message=modalMessage;messages=&ms;
 OfxMessageSuiteV2 pm{};pm.setPersistentMessage=persistentMessage;pm.clearPersistentMessage=clearMessage;persistentMessages=&pm;
 mock.effect=effect;mock.result=sourceClip;mock.output=outputClip;
 for(mode=0;mode<=6;++mode){
  cancelled=mode==6;fetches=releases=modal=persistent=cleared=0;std::fill(outputPixels,outputPixels+8,-99.f);
  auto status=entry(kOfxImageEffectActionRender,effect,nullptr,nullptr);
  require(modal==0,"Render opened a modal error");
  require(status==((mode==4||mode==5)?kOfxStatFailed:kOfxStatOK),"Unexpected render status");
  if(mode==0){require(std::equal(sourcePixels,sourcePixels+8,outputPixels),"Normal render changed pixels");require(releases==2,"Image handle leak");}
  if(mode==1){require(std::all_of(outputPixels,outputPixels+8,[](float v){return v==0.f;}),"Empty input not transparent");require(releases==1,"Output not released");}
  if(mode==2)require(fetches==1&&releases==0,"Input cancellation did not stop");
  if(mode==3)require(releases==1&&persistent==0,"Output cancellation leaked/reported");
  if(mode==4||mode==5)require(persistent==1,"Real failure was hidden");
  if(mode==6)require(fetches==0,"Already cancelled render fetched images");
 }
 mode=1;bool strict=false;try{Image analysis(sourceClip,42);}catch(const std::exception& e){strict=std::string(e.what()).find("frame 42")!=std::string::npos;}
 require(strict,"Analysis silently accepted a missing frame");
 std::cout<<"PASS: playback cancellation, empty input, real errors, exact pixels, strict analysis\n";
}
