#include "matcher.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
static void require(bool pass,const char* name){if(!pass)throw std::runtime_error(name);}
int main(){try{
    std::vector<int> truth{2,3,4,4,4,5,8,9,10};int n=13,m=int(truth.size());
    std::vector<double> c(n*m,1);for(int j=0;j<m;++j)c[j*n+truth[j]]=0;
    auto s=fm::solve(c,m,n);require(s.path==truth,"hold/drop truth");require(s.holds==2&&s.skips==2,"event counts");
    c.assign(36,1);for(int i=0;i<6;++i)c[i*6+i]=.01;c[4*6+1]=0;
    s=fm::solve(c,6,6);for(int i=0;i<6;++i)require(s.path[i]==i,"reverse rejection");
    s=fm::solve(std::vector<double>(100,0),10,10);for(double x:s.confidence)require(x<.25,"static ambiguity");
    std::mt19937 rng(21);std::vector<float> panorama((fm::W+100)*fm::H);for(float& v:panorama)v=float(rng()%1000)/1000;
    std::vector<fm::Feature> source,result;std::vector<std::vector<float>> grays;
    for(int i=0;i<30;++i){std::vector<float> g(fm::W*fm::H);for(int y=0;y<fm::H;++y)for(int x=0;x<fm::W;++x)g[y*fm::W+x]=panorama[y*(fm::W+100)+x+i*2];grays.push_back(g);source.push_back(fm::features(g));}
    truth.clear();for(int i=0;i<30;++i){if(i==10||i==11||i==23)continue;truth.push_back(i);if(i==18){truth.push_back(i);truth.push_back(i);truth.push_back(i);}}
    for(int i:truth){auto g=grays[i];for(float& v:g)v=v*.8f+.1f;for(int y=35;y<65;++y)for(int x=65;x<105;++x)g[y*fm::W+x]=float(rng()%1000)/1000;result.push_back(fm::features(g));}
    c=fm::costs(source,result);s=fm::solve(c,int(result.size()),int(source.size()));require(s.path==truth,"image matching with replaced object");
    auto identity=[](int count){fm::Solution v;v.inverse.resize(count);v.missing.assign(count,0);for(int i=0;i<count;++i)v.inverse[i]=i;return v;};
    auto heldResult=source;auto noisy=grays[14];for(float& v:noisy)v+=float(int(rng()%101)-50)*.00002f;
    heldResult[15]=fm::features(noisy);auto heldSolution=identity(30);
    require(fm::markImageHolds(heldSolution,source,heldResult)==1,"encoded hold at different Change indices");
    require(heldSolution.missing[15]&&!heldSolution.missing[14]&&!heldSolution.missing[16],"retain both interpolation endpoints");
    require(heldSolution.inverse[15]==15,"hold repair must preserve TimeWarp mapping");
    auto natural=identity(30);require(fm::markImageHolds(natural,heldResult,heldResult)==0,"do not replace a hold also present in original");
    auto slowSource=source;for(auto& f:slowSource)for(float& v:f.luma)v*=.02f;
    auto slow=identity(30);require(fm::markImageHolds(slow,slowSource,slowSource)==0,"do not replace genuine low motion");
    for(int i=15;i<=22;++i)heldResult[i]=heldResult[14];heldSolution=identity(30);
    require(fm::markImageHolds(heldSolution,source,heldResult)==8,"long multi-frame hold");
    require(!heldSolution.missing[14]&&!heldSolution.missing[23],"long hold endpoints");
    auto nearest=identity(30);nearest.inverse[15]=16;nearest.missing[15]=1;
    require(fm::markImageHolds(nearest,source,source)==0&&!nearest.missing[16],"nearest future placeholder must not eat right endpoint");
    std::vector<float> mask(fm::W*fm::H,0.f);
    auto original=fm::features(grays[0]),black=original;
    fm::excludeMask(black,mask);require(black.weight==original.weight,"black mask preserves features");
    for(int y=40;y<60;++y)for(int x=80;x<100;++x)mask[y*fm::W+x]=1;
    auto masked=original;fm::excludeMask(masked,mask);
    require(masked.weight[4*fm::TX+5]==0,"white mask excludes center tile");
    require(masked.weight[2*fm::TX+2]>0,"unmasked tile retained");
    auto altered=grays[0];for(int i=0;i<fm::W*fm::H;++i)if(mask[i]>0)altered[i]=float(rng()%1000)/1000;
    auto other=fm::features(altered);fm::excludeMask(other,mask);
    auto maskedCost=fm::costs({masked},{other});require(maskedCost[0]<1e-5,"excluded object cannot affect match cost");
    std::vector<fm::Feature> clean,corrupted;
    for(int frame=0;frame<5;++frame){
        auto g=grays[frame];auto f=fm::features(g);fm::excludeMask(f,mask);clean.push_back(f);
        for(int i=0;i<fm::W*fm::H;++i)if(mask[i]>0)g[i]=float(rng()%1000)/1000;
        f=fm::features(g);fm::excludeMask(f,mask);corrupted.push_back(f);
    }
    auto cleanCosts=fm::costs(clean,clean),corruptCosts=fm::costs(clean,corrupted);
    for(size_t i=0;i<cleanCosts.size();++i)require(std::abs(cleanCosts[i]-corruptCosts[i])<1e-5,"masked pixels affect cost matrix");
    std::fill(mask.begin(),mask.end(),1.f);fm::excludeMask(masked,mask);
    bool rejected=false;try{fm::costs({masked},{masked});}catch(const std::exception&){rejected=true;}
    require(rejected,"all-white mask rejected, never silently ignored");
    std::cout<<"PASS: holds, drops, monotone order, ambiguity, changed-object image matching\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
