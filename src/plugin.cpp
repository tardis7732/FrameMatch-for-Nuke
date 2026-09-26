#include "ofxImageEffect.h"
#include "ofxParam.h"
#include "ofxProperty.h"
#include "ofxMessage.h"
#include "ofxProgress.h"
#include "matcher.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>

static OfxHost* host=nullptr;
static const OfxPropertySuiteV1* props=nullptr;
static const OfxImageEffectSuiteV1* images=nullptr;
static const OfxParameterSuiteV1* params=nullptr;
static const OfxMessageSuiteV1* messages=nullptr;
static const OfxMessageSuiteV2* persistentMessages=nullptr;
static const OfxProgressSuiteV1* progress=nullptr;
static thread_local const char* currentAction="unknown";
static void check(OfxStatus s,const char* what){if(s!=kOfxStatOK)throw std::runtime_error(std::string(what)+" (OFX "+std::to_string(s)+")");}
static std::string stringProp(OfxPropertySetHandle p,const char* key){char* v=nullptr;props->propGetString(p,key,0,&v);return v?v:"";}
static int intProp(OfxPropertySetHandle p,const char* key,int fallback=0){int v=fallback;props->propGetInt(p,key,0,&v);return v;}
static OfxPropertySetHandle effectProps(OfxImageEffectHandle e){OfxPropertySetHandle p=nullptr;check(images->getPropertySet(e,&p),"effect properties");return p;}
struct Instance {
    OfxImageEffectHandle effect=nullptr;OfxImageClipHandle result=nullptr,original=nullptr,output=nullptr,mask=nullptr;
    OfxParamSetHandle set=nullptr;std::atomic<bool> analyzing{false};
    OfxParamHandle get(const char* name)const{OfxParamHandle p=nullptr;check(params->paramGetHandle(set,name,&p,nullptr),name);return p;}
    int integer(const char* name)const{int value=0;check(params->paramGetValue(get(name),&value),name);return value;}
    double number(const char* name)const{double value=0;check(params->paramGetValue(get(name),&value),name);return value;}
    void text(const char* name,const std::string& value)const{check(params->paramSetValue(get(name),value.c_str()),name);}
};
static Instance* instance(OfxImageEffectHandle e){void* data=nullptr;props->propGetPointer(effectProps(e),kOfxPropInstanceData,0,&data);return static_cast<Instance*>(data);}
struct Image {
    OfxPropertySetHandle handle=nullptr;void* data=nullptr;OfxRectI bounds{};int stride=0,channels=0;double sx=1,sy=1,par=1;
    Image(OfxImageClipHandle clip,double time,const OfxRectD* region=nullptr,bool allowEmpty=false){
        OfxStatus fetched=images->clipGetImage(clip,time,region,&handle);
        // OFX defines a failed input fetch as an empty/transparent image. Viewer
        // cancellation can also fail a fetch; Render checks abort separately.
        // Analysis remains strict: an absent frame must never become a match.
        if(fetched==kOfxStatFailed&&allowEmpty){handle=nullptr;return;}
        if(fetched!=kOfxStatOK){
            OfxPropertySetHandle cp=nullptr;images->clipGetPropertySet(clip,&cp);
            std::ostringstream error;error<<"Cannot fetch "<<stringProp(cp,kOfxPropName)<<" frame "<<time<<" during "<<currentAction<<" (OFX "<<fetched<<")";
            if(region)error<<" region "<<region->x1<<','<<region->y1<<','<<region->x2<<','<<region->y2;
            throw std::runtime_error(error.str());
        }
        try {
            props->propGetPointer(handle,kOfxImagePropData,0,&data);props->propGetIntN(handle,kOfxImagePropBounds,4,&bounds.x1);
            stride=intProp(handle,kOfxImagePropRowBytes);auto comp=stringProp(handle,kOfxImageEffectPropComponents);
            channels=comp==kOfxImageComponentRGBA?4:comp==kOfxImageComponentRGB?3:comp==kOfxImageComponentAlpha?1:0;
            if(!data||!channels||stringProp(handle,kOfxImageEffectPropPixelDepth)!=kOfxBitDepthFloat)throw std::runtime_error("FrameMatch requires float RGB/RGBA images or alpha masks");
            double scale[2]={1,1};props->propGetDoubleN(handle,kOfxImageEffectPropRenderScale,2,scale);sx=scale[0];sy=scale[1];
            props->propGetDouble(handle,kOfxImagePropPixelAspectRatio,0,&par);
        }catch(...){images->clipReleaseImage(handle);handle=nullptr;throw;}
    }
    ~Image(){if(handle)images->clipReleaseImage(handle);}
    Image(const Image&)=delete;Image& operator=(const Image&)=delete;
    float* pixel(int x,int y)const{
        if(x<bounds.x1||x>=bounds.x2||y<bounds.y1||y>=bounds.y2)return nullptr;
        return reinterpret_cast<float*>(static_cast<char*>(data)+ptrdiff_t(y-bounds.y1)*stride)+(x-bounds.x1)*channels;
    }
    float sample(double x,double y,int channel)const{
        int x0=int(std::floor(x)),y0=int(std::floor(y));float ax=float(x-x0),ay=float(y-y0),v=0;
        for(int yy=0;yy<2;++yy)for(int xx=0;xx<2;++xx){float* p=pixel(x0+xx,y0+yy);if(p)v+=p[channel]*(xx?ax:1-ax)*(yy?ay:1-ay);}
        return v;
    }
};
static float srgb(float x){x=std::clamp(x,0.f,1.f);return x<=.0031308f?12.92f*x:1.055f*std::pow(x,1.f/2.4f)-.055f;}
static fm::Feature extract(OfxImageClipHandle clip,double time,const OfxRectD& region,OfxImageClipHandle maskClip=nullptr){
    Image image(clip,time,&region);std::vector<float> gray(fm::W*fm::H);
    for(int y=0;y<fm::H;++y)for(int x=0;x<fm::W;++x){float rgb[3]={0,0,0};
        for(int yy=0;yy<4;++yy)for(int xx=0;xx<4;++xx){
            double cx=region.x1+(x+(xx+.5)/4)*(region.x2-region.x1)/fm::W;
            double cy=region.y2-(y+(yy+.5)/4)*(region.y2-region.y1)/fm::H;
            for(int c=0;c<3;++c)rgb[c]+=image.sample(cx*image.sx/image.par-.5,cy*image.sy-.5,c)/16;
        }
        gray[y*fm::W+x]=.299f*srgb(rgb[0])+.587f*srgb(rgb[1])+.114f*srgb(rgb[2]);
    }
    auto feature=fm::features(gray);
    if(maskClip){
        Image mask(maskClip,time,&region);std::vector<float> alpha(fm::W*fm::H);
        if(mask.channels!=1&&mask.channels!=4)throw std::runtime_error("Mask requires an alpha channel");
        for(int y=0;y<fm::H;++y)for(int x=0;x<fm::W;++x){
            float coverage=0;
            for(int yy=0;yy<4;++yy)for(int xx=0;xx<4;++xx){
                double cx=region.x1+(x+(xx+.5)/4)*(region.x2-region.x1)/fm::W;
                double cy=region.y2-(y+(yy+.5)/4)*(region.y2-region.y1)/fm::H;
                coverage=std::max(coverage,mask.sample(cx*mask.sx/mask.par-.5,cy*mask.sy-.5,mask.channels==1?0:3));
            }
            alpha[y*fm::W+x]=coverage;
        }
        fm::excludeMask(feature,alpha);
    }
    return feature;
}
static bool connected(OfxImageClipHandle c){OfxPropertySetHandle p=nullptr;images->clipGetPropertySet(c,&p);return intProp(p,kOfxImageClipPropConnected)!=0;}
static double mapped(Instance* d,double time){
    (void)d;
    return time; // Analysis/export tool: its own output is always unchanged.
}
struct ProgressGuard {
    OfxImageEffectHandle e;bool started=false;
    explicit ProgressGuard(OfxImageEffectHandle effect):e(effect){if(progress)started=progress->progressStart(e,"FrameMatch: native frame analysis")==kOfxStatOK;}
    bool update(double p){return !images->abort(e)&&(!started||progress->progressUpdate(e,p)!=kOfxStatReplyNo);}
    void finish(){if(started){started=false;progress->progressEnd(e);}}
    ~ProgressGuard(){finish();}
};
static void exportCsv(Instance* d,const fm::Solution& s,const std::vector<double>& cost,int ss,int cs,int n,int m){
    char* filename=nullptr;params->paramGetValue(d->get("reportFile"),&filename);if(!filename||!*filename)return;
    std::filesystem::path path=std::filesystem::u8path(filename);
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);if(!out)throw std::runtime_error("Cannot write report CSV");
    out<<"source_frame,result_frame,matched_source_frame,missing,confidence_score,image_hold\n"<<std::setprecision(9);
    for(int i=0;i<n;++i){int j=s.inverse[i];out<<ss+i<<','<<cs+j<<','<<ss+s.path[j]<<','<<s.missing[i]<<','<<s.confidence[j]<<','<<s.imageHeld[i]<<'\n';}
    if(cost.empty())return; // A stored lookup has no freshly solved cost matrix.
    path.replace_filename(path.stem().u8string()+"_change_to_source.csv");std::ofstream detail(path);
    detail<<"result_frame,source_frame,source_step,event,cost,confidence_score\n"<<std::setprecision(9);
    for(int j=0;j<m;++j){int delta=j?s.path[j]-s.path[j-1]:1;
        detail<<cs+j<<','<<ss+s.path[j]<<','<<delta<<','<<(j==0?"start":delta==0?"hold_candidate":delta>1?"drop_candidate":"normal")<<','<<cost[size_t(j)*n+s.path[j]]<<','<<s.confidence[j]<<'\n';}
}
static void analyze(Instance* d){
    if(d->analyzing.exchange(true))return;
    struct Reset{Instance* d;~Reset(){d->analyzing=false;}}reset{d};
    if(!connected(d->result)||!connected(d->original))throw std::runtime_error("Connect Result and Source inputs first");
    int ss=d->integer("sourceFirst"),se=d->integer("sourceLast"),cs=d->integer("resultFirst"),ce=d->integer("resultLast");
    int n=se-ss+1,m=ce-cs+1;
    if(n<1||m<1||static_cast<long long>(n)*m>1000000)throw std::runtime_error("Invalid or excessive frame range (maximum 1,000,000 pairs)");
    OfxRectD region{};check(images->clipGetRegionOfDefinition(d->original,ss,&region),"Source image region");
    if(region.x2<=region.x1||region.y2<=region.y1)throw std::runtime_error("Empty source image region");
    auto maskClip=d->mask&&connected(d->mask)?d->mask:nullptr;
    ProgressGuard meter(d->effect);std::vector<fm::Feature> source,result;source.reserve(n);result.reserve(m);
    for(int i=0;i<n;++i){if(!meter.update(.55*double(i)/(n+m)))throw std::runtime_error("Analysis cancelled");source.push_back(extract(d->original,ss+i,region,maskClip));}
    for(int j=0;j<m;++j){if(!meter.update(.55*double(n+j)/(n+m)))throw std::runtime_error("Analysis cancelled");result.push_back(extract(d->result,cs+j,region,maskClip));}
    std::vector<double> cost;fm::Solution solution;
    bool preserve=d->integer("repairStoredTiming")!=0;
    if(preserve){
        solution.inverse.resize(n);solution.missing.resize(n);solution.path.assign(m,0);solution.confidence.assign(m,0);
        solution.reviews=d->integer("reviewCount");
        for(int i=0;i<n;++i){double frame=0,missing=0,confidence=0;
            check(params->paramGetValueAtTime(d->get("inputFrame"),ss+i,&frame),"Stored lookup");
            check(params->paramGetValueAtTime(d->get("missing"),ss+i,&missing),"Stored missing frames");
            check(params->paramGetValueAtTime(d->get("confidence"),ss+i,&confidence),"Stored confidence");
            if(!std::isfinite(frame)||frame<cs||frame>ce)throw std::runtime_error("Stored lookup outside Change range");
            int j=int(std::round(frame))-cs;
            if(i&&j<solution.inverse[i-1])throw std::runtime_error("Stored lookup goes backwards");
            solution.inverse[i]=j;solution.missing[i]=missing>.5;solution.confidence[j]=confidence;
            if(!solution.missing[i])solution.path[j]=i;
        }
    }else{
        cost=fm::costs(source,result,[&](double p){return meter.update(.55+.4*p);});
        solution=fm::solve(cost,m,n,d->number("timingPreference"));
    }
    int imageHolds=fm::markImageHolds(solution,source,result);
    if(!meter.update(.97))throw std::runtime_error("Analysis cancelled");
    // All image handles have already been released. Commit only a complete solution.
    params->paramEditBegin(d->set,"FrameMatch analysis");
    for(const char* name:{"inputFrame","confidence","missing"})params->paramDeleteAllKeys(d->get(name));
    for(int i=0;i<n;++i){int j=solution.inverse[i];
        params->paramSetValueAtTime(d->get("inputFrame"),double(ss+i),double(cs+j));
        params->paramSetValueAtTime(d->get("confidence"),double(ss+i),solution.confidence[j]);
        params->paramSetValueAtTime(d->get("missing"),double(ss+i),double(solution.missing[i]));
    }
    params->paramSetValue(d->get("ready"),1);
    int missing=std::accumulate(solution.missing.begin(),solution.missing.end(),0);
    params->paramSetValue(d->get("matchedCount"),n-missing);
    params->paramSetValue(d->get("missingCount"),missing);
    params->paramSetValue(d->get("reviewCount"),solution.reviews);
    d->text("status",(preserve?std::string("Stored timing"):std::to_string(solution.holds)+" mapping holds")+" / "+std::to_string(imageHolds)+" image holds / "+std::to_string(missing)+" missing source frames / "+std::to_string(solution.reviews)+" review frames / 0 reverse steps"+(maskClip?" / white Mask excluded":" / no Mask"));
    params->paramEditEnd(d->set);
    try{exportCsv(d,solution,cost,ss,cs,n,m);}catch(const std::exception& e){if(messages)messages->message(d->effect,kOfxMessageWarning,"report","%s",e.what());}
    meter.update(1.);
    meter.finish();
    // Last successful-commit notification. Nuke's bridge queues graph edits
    // until this native action returns; failed/cancelled solves never export.
    params->paramSetValue(d->get("analysisRevision"),d->integer("analysisRevision")+1);
}
static OfxPropertySetHandle parameter(OfxParamSetHandle set,const char* type,const char* name,const char* label){
    OfxPropertySetHandle p=nullptr;check(params->paramDefine(set,type,name,&p),name);props->propSetString(p,kOfxPropLabel,0,label);return p;
}
static OfxStatus describe(OfxImageEffectHandle e){
    auto p=effectProps(e);props->propSetString(p,kOfxPropLabel,0,"FrameMatchOFX");
    props->propSetString(p,kOfxImageEffectPluginPropGrouping,0,"Time");
    props->propSetString(p,kOfxPropPluginDescription,0,"Native C++ monotone frame matching. Result + original Source. Optional white-alpha exclusion Mask. No external process or image-file extraction. Analyze after upstream changes.");
    props->propSetString(p,kOfxImageEffectPropSupportedContexts,0,kOfxImageEffectContextGeneral);
    props->propSetString(p,kOfxImageEffectPropSupportedPixelDepths,0,kOfxBitDepthFloat);
    props->propSetInt(p,kOfxImageEffectPropTemporalClipAccess,0,1);
    props->propSetInt(p,kOfxImageEffectPropSupportsTiles,0,0);
    props->propSetInt(p,kOfxImageEffectPropSupportsMultiResolution,0,1);
    props->propSetString(p,kOfxImageEffectPluginRenderThreadSafety,0,kOfxImageEffectRenderFullySafe);
    props->propSetInt(p,kOfxImageEffectPluginPropHostFrameThreading,0,0);
    return kOfxStatOK;
}
static OfxStatus describeContext(OfxImageEffectHandle e){
    for(const char* name:{"Source","Reference","Mask","Output"}){OfxPropertySetHandle p=nullptr;check(images->clipDefine(e,name,&p),name);
        props->propSetString(p,kOfxImageEffectPropSupportedComponents,0,kOfxImageComponentRGBA);
        props->propSetString(p,kOfxImageEffectPropSupportedComponents,1,kOfxImageComponentRGB);
        props->propSetString(p,kOfxPropLabel,0,std::strcmp(name,"Source")==0?"Result":std::strcmp(name,"Reference")==0?"Source":name);
        props->propSetInt(p,kOfxImageEffectPropTemporalClipAccess,0,1);props->propSetInt(p,kOfxImageClipPropOptional,0,!std::strcmp(name,"Mask"));
    }
    OfxParamSetHandle set=nullptr;images->getParamSet(e,&set);
    struct Range{const char* name;const char* label;int value;};
    for(auto x:{Range{"sourceFirst","Source first",1},Range{"sourceLast","Source last",252},Range{"resultFirst","Result first",1},Range{"resultLast","Result last",249}}){
        auto p=parameter(set,kOfxParamTypeInteger,x.name,x.label);props->propSetInt(p,kOfxParamPropDefault,0,x.value);props->propSetInt(p,kOfxParamPropAnimates,0,0);
        props->propSetInt(p,kOfxParamPropMin,0,-1000000);props->propSetInt(p,kOfxParamPropMax,0,1000000);
    }
    parameter(set,kOfxParamTypePushButton,"ranges","Use input ranges");
    parameter(set,kOfxParamTypePushButton,"analyze","Analyze and export TimeWarp");
    auto p=parameter(set,kOfxParamTypeDouble,"timingPreference","Timing preference");
    props->propSetDouble(p,kOfxParamPropDefault,0,.15);props->propSetDouble(p,kOfxParamPropMin,0,0);props->propSetDouble(p,kOfxParamPropMax,0,1);props->propSetInt(p,kOfxParamPropAnimates,0,0);
    for(const char* name:{"inputFrame","confidence","missing"}){
        const char* label=std::strcmp(name,"inputFrame")==0?"Result frame":std::strcmp(name,"confidence")==0?"Confidence score":"Missing source frame";
        p=parameter(set,kOfxParamTypeDouble,name,label);props->propSetDouble(p,kOfxParamPropDefault,0,std::strcmp(name,"inputFrame")==0?1.:0.);
        props->propSetInt(p,kOfxParamPropAnimates,0,1);
        if(std::strcmp(name,"inputFrame"))props->propSetInt(p,kOfxParamPropEnabled,0,0);
    }
    p=parameter(set,kOfxParamTypeBoolean,"ready","Analysis complete");props->propSetInt(p,kOfxParamPropDefault,0,0);props->propSetInt(p,kOfxParamPropAnimates,0,0);props->propSetInt(p,kOfxParamPropEnabled,0,0);
    p=parameter(set,kOfxParamTypeBoolean,"externalOutput","Output via TimeWarp");props->propSetInt(p,kOfxParamPropDefault,0,0);props->propSetInt(p,kOfxParamPropAnimates,0,0);
    props->propSetInt(p,kOfxParamPropSecret,0,1); // Legacy project compatibility only.
    p=parameter(set,kOfxParamTypeBoolean,"repairStoredTiming","Preserve stored timing");
    props->propSetInt(p,kOfxParamPropDefault,0,0);props->propSetInt(p,kOfxParamPropAnimates,0,0);props->propSetInt(p,kOfxParamPropSecret,0,1);
    for(const char* name:{"matchedCount","missingCount","reviewCount","analysisRevision"}){
        const char* label=std::strcmp(name,"matchedCount")==0?"Matched source frames":std::strcmp(name,"missingCount")==0?"Missing source frames":std::strcmp(name,"reviewCount")==0?"Review result frames":"Analysis revision";
        p=parameter(set,kOfxParamTypeInteger,name,label);props->propSetInt(p,kOfxParamPropDefault,0,0);props->propSetInt(p,kOfxParamPropAnimates,0,0);props->propSetInt(p,kOfxParamPropEnabled,0,0);
        if(!std::strcmp(name,"analysisRevision"))props->propSetInt(p,kOfxParamPropSecret,0,1);
    }
    p=parameter(set,kOfxParamTypeString,"status","Status");props->propSetString(p,kOfxParamPropDefault,0,"Ready. Analyze after upstream changes. Low confidence needs review.");
    props->propSetString(p,kOfxParamPropStringMode,0,kOfxParamStringIsSingleLine);props->propSetInt(p,kOfxParamPropAnimates,0,0);props->propSetInt(p,kOfxParamPropEnabled,0,0);
    p=parameter(set,kOfxParamTypeString,"reportFile","Optional report CSV");props->propSetString(p,kOfxParamPropStringMode,0,kOfxParamStringIsFilePath);props->propSetString(p,kOfxParamPropDefault,0,"");
    p=parameter(set,kOfxParamTypePage,"controls","Frame Match");int idx=0;
    for(const char* name:{"sourceFirst","sourceLast","resultFirst","resultLast","ranges","analyze","timingPreference","ready","externalOutput","inputFrame","matchedCount","missingCount","reviewCount","confidence","missing","status","reportFile"})props->propSetString(p,kOfxParamPropPageChild,idx++,name);
    return kOfxStatOK;
}
static OfxStatus create(OfxImageEffectHandle e){
    auto d=std::make_unique<Instance>();d->effect=e;check(images->getParamSet(e,&d->set),"parameter set");
    check(images->clipGetHandle(e,"Source",&d->result,nullptr),"Result clip");check(images->clipGetHandle(e,"Reference",&d->original,nullptr),"Source clip");check(images->clipGetHandle(e,"Output",&d->output,nullptr),"Output clip");
    check(images->clipGetHandle(e,"Mask",&d->mask,nullptr),"Mask clip");
    props->propSetPointer(effectProps(e),kOfxPropInstanceData,0,d.release());return kOfxStatOK;
}
static OfxStatus changed(OfxImageEffectHandle e,OfxPropertySetHandle in){
    auto d=instance(e);if(!d||d->analyzing)return kOfxStatOK;
    auto name=stringProp(in,kOfxPropName);auto reason=stringProp(in,kOfxPropChangeReason);
    if(reason!=kOfxChangeUserEdited)return kOfxStatOK;
    if(name=="analyze"){analyze(d);return kOfxStatOK;}
    if(name=="ranges"){
        for(auto item:{std::pair<OfxImageClipHandle,const char*>{d->original,"source"},{d->result,"result"}}){
            OfxPropertySetHandle p=nullptr;images->clipGetPropertySet(item.first,&p);double r[2]={1,1};props->propGetDoubleN(p,kOfxImageEffectPropFrameRange,2,r);
            params->paramSetValue(d->get((std::string(item.second)+"First").c_str()),int(std::ceil(r[0])));params->paramSetValue(d->get((std::string(item.second)+"Last").c_str()),int(std::floor(r[1])));
        }
    }
    if(name=="ranges"||name=="sourceFirst"||name=="sourceLast"||name=="resultFirst"||name=="resultLast"||name=="timingPreference"||name=="Source"||name=="Reference"||name=="Mask"){
        params->paramSetValue(d->get("ready"),0);d->text("status","Inputs/settings changed. Analyze frames again.");
    }
    return kOfxStatOK;
}
static OfxStatus render(OfxImageEffectHandle e,OfxPropertySetHandle in){
    auto d=instance(e);double time=0;props->propGetDouble(in,kOfxPropTime,0,&time);OfxRectI window{};props->propGetIntN(in,kOfxImageEffectPropRenderWindow,4,&window.x1);
    if(images->abort(e))return kOfxStatOK;
    Image source(d->result,mapped(d,time),nullptr,true);
    if(images->abort(e))return kOfxStatOK;
    Image output(d->output,time);
    for(int y=window.y1;y<window.y2;++y){if(images->abort(e))return kOfxStatOK;
        for(int x=window.x1;x<window.x2;++x){float* to=output.pixel(x,y);if(!to)continue;float* from=source.pixel(x,y);
            for(int c=0;c<output.channels;++c)to[c]=from?(c<source.channels?from[c]:1.f):0.f;
        }
    }
    if(persistentMessages)persistentMessages->clearPersistentMessage(e);
    return kOfxStatOK;
}
static OfxStatus entry(const char* action,const void* handle,OfxPropertySetHandle in,OfxPropertySetHandle out){
    const char* previousAction=currentAction;currentAction=action;
    struct RestoreAction{const char* old;~RestoreAction(){currentAction=old;}}restoreAction{previousAction};
    auto e=const_cast<OfxImageEffectHandle>(reinterpret_cast<const OfxImageEffectStruct*>(handle));
    try{
        if(!std::strcmp(action,kOfxActionLoad)){
            props=static_cast<const OfxPropertySuiteV1*>(host->fetchSuite(host->host,kOfxPropertySuite,1));
            images=static_cast<const OfxImageEffectSuiteV1*>(host->fetchSuite(host->host,kOfxImageEffectSuite,1));
            params=static_cast<const OfxParameterSuiteV1*>(host->fetchSuite(host->host,kOfxParameterSuite,1));
            messages=static_cast<const OfxMessageSuiteV1*>(host->fetchSuite(host->host,kOfxMessageSuite,1));
            persistentMessages=static_cast<const OfxMessageSuiteV2*>(host->fetchSuite(host->host,kOfxMessageSuite,2));
            progress=static_cast<const OfxProgressSuiteV1*>(host->fetchSuite(host->host,kOfxProgressSuite,1));
            return props&&images&&params?kOfxStatOK:kOfxStatErrMissingHostFeature;
        }
        if(!std::strcmp(action,kOfxActionDescribe))return describe(e);
        if(!std::strcmp(action,kOfxImageEffectActionDescribeInContext))return describeContext(e);
        if(!std::strcmp(action,kOfxActionCreateInstance))return create(e);
        if(!std::strcmp(action,kOfxActionDestroyInstance)){delete instance(e);props->propSetPointer(effectProps(e),kOfxPropInstanceData,0,nullptr);return kOfxStatOK;}
        if(!std::strcmp(action,kOfxActionInstanceChanged))return changed(e,in);
        if(!std::strcmp(action,kOfxImageEffectActionRender))return render(e,in);
        if(!std::strcmp(action,kOfxImageEffectActionGetRegionOfDefinition)){
            auto d=instance(e);double t=0;props->propGetDouble(in,kOfxPropTime,0,&t);OfxRectD r{};check(images->clipGetRegionOfDefinition(d->result,mapped(d,t),&r),"output region");
            props->propSetDoubleN(out,kOfxImageEffectPropRegionOfDefinition,4,&r.x1);return kOfxStatOK;
        }
        if(!std::strcmp(action,kOfxImageEffectActionGetFramesNeeded)){
            auto d=instance(e);double t=0;props->propGetDouble(in,kOfxPropTime,0,&t);double f=mapped(d,t),range[2]={f,f};
            props->propSetDoubleN(out,"OfxImageClipPropFrameRange_Source",2,range);double ref[2]={t,t};props->propSetDoubleN(out,"OfxImageClipPropFrameRange_Reference",2,ref);if(d->mask&&connected(d->mask))props->propSetDoubleN(out,"OfxImageClipPropFrameRange_Mask",2,ref);return kOfxStatOK;
        }
        if(!std::strcmp(action,kOfxImageEffectActionGetTimeDomain)){
            auto d=instance(e);OfxPropertySetHandle cp=nullptr;images->clipGetPropertySet(d->result,&cp);
            double r[2]={1,1};check(props->propGetDoubleN(cp,kOfxImageEffectPropFrameRange,2,r),"Result frame range");props->propSetDoubleN(out,kOfxImageEffectPropFrameRange,2,r);return kOfxStatOK;
        }
        return kOfxStatReplyDefault;
    }catch(const std::exception& error){
        // A Viewer cancels stale renders while playing/scrubbing. This is not a
        // user error. Render threads must never open modal message boxes.
        if(!std::strcmp(action,kOfxImageEffectActionRender)){
            if(images&&images->abort(e))return kOfxStatOK;
            if(persistentMessages)persistentMessages->setPersistentMessage(e,kOfxMessageError,"FrameMatch","%s",error.what());
            return kOfxStatFailed;
        }
        if(messages&&e)messages->message(e,kOfxMessageError,"FrameMatch","%s",error.what());
        return kOfxStatFailed;
    }catch(...){return kOfxStatErrUnknown;}
}
static void setHost(OfxHost* h){host=h;}
static OfxPlugin plugin={kOfxImageEffectPluginApi,1,"com.matchframe.FrameMatch",1,8,setHost,entry};
extern "C" {
OfxExport int OfxGetNumberOfPlugins(void){return 1;}
OfxExport OfxPlugin* OfxGetPlugin(int index){return index==0?&plugin:nullptr;}
}
