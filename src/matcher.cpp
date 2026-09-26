#include "matcher.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <thread>

namespace fm {
static int reflect(int x,int n){while(x<0||x>=n){if(x<0)x=-x; if(x>=n)x=2*n-x-2;}return x;}
static std::vector<float> blur(const std::vector<float>& in,double sigma){
    int radius=int(std::ceil(4*sigma));std::vector<float> kernel(2*radius+1);double sum=0;
    for(int k=-radius;k<=radius;++k){kernel[k+radius]=float(std::exp(-k*k/(2*sigma*sigma)));sum+=kernel[k+radius];}
    for(float& x:kernel)x=float(x/sum);
    std::vector<float> tmp(W*H),out(W*H);
    for(int y=0;y<H;++y)for(int x=0;x<W;++x){float v=0;for(int k=-radius;k<=radius;++k)v+=kernel[k+radius]*in[y*W+reflect(x+k,W)];tmp[y*W+x]=v;}
    for(int y=0;y<H;++y)for(int x=0;x<W;++x){float v=0;for(int k=-radius;k<=radius;++k)v+=kernel[k+radius]*tmp[reflect(y+k,H)*W+x];out[y*W+x]=v;}
    return out;
}
Feature features(const std::vector<float>& gray){
    if(gray.size()!=W*H)throw std::runtime_error("Invalid analysis image size");
    auto g=blur(gray,.7), mean=blur(g,3.);
    auto square=g;for(float& v:square)v*=v;auto variance=blur(square,3.);
    Feature f;f.luma=g;f.values.resize(TILES*DIM);f.weight.resize(TILES);
    for(int t=0;t<TILES;++t){int tx=t%TX,ty=t/TX;float texture=0;
        float* dst=&f.values[t*DIM];int index=0;
        for(int yy=0;yy<PH;++yy)for(int xx=0;xx<PW;++xx){int x=tx*PW+xx,y=ty*PH+yy,p=y*W+x;
            float var=std::max(0.f,variance[p]-mean[p]*mean[p]);texture+=std::sqrt(var);
            auto v=[&](int a,int b){return g[reflect(b,H)*W+reflect(a,W)];};
            float gx=v(x+1,y-1)+2*v(x+1,y)+v(x+1,y+1)-v(x-1,y-1)-2*v(x-1,y)-v(x-1,y+1);
            float gy=v(x-1,y+1)+2*v(x,y+1)+v(x+1,y+1)-v(x-1,y-1)-2*v(x,y-1)-v(x+1,y-1);
            dst[index++]=std::clamp((g[p]-mean[p])/std::sqrt(var+.0004f),-3.f,3.f);
            dst[index++]=gx/std::sqrt(var+.002f);dst[index++]=gy/std::sqrt(var+.002f);
        }
        float avg=std::accumulate(dst,dst+DIM,0.f)/DIM,norm=0;
        for(int k=0;k<DIM;++k){dst[k]-=avg;norm+=dst[k]*dst[k];}
        norm=std::max(std::sqrt(norm),1e-6f);for(int k=0;k<DIM;++k)dst[k]/=norm;
        f.weight[t]=(tx==0||ty==0||tx==TX-1||ty==TY-1)?0.f:std::clamp(texture/(PW*PH*.04f),0.f,1.f);
    }return f;
}
// Exclude descriptor samples whose filter support touches the white mask.
// Partially masked tiles keep their remaining pixels; pairwise NCC below uses
// only the intersection of valid samples, with its own mean and normalization.
void excludeMask(Feature& feature,const std::vector<float>& alpha){
    if(alpha.size()!=W*H)throw std::runtime_error("Invalid exclusion mask size");
    if(std::none_of(alpha.begin(),alpha.end(),[](float x){return x>.001f;}))return;
    constexpr int pad=16;
    std::vector<int> prefix((W+1)*(H+1),0);
    for(int y=0;y<H;++y)for(int x=0;x<W;++x)
        prefix[(y+1)*(W+1)+x+1]=(alpha[y*W+x]>.001f)+prefix[y*(W+1)+x+1]+prefix[(y+1)*(W+1)+x]-prefix[y*(W+1)+x];
    feature.valid.assign(TILES*PW*PH,0);
    for(int t=0;t<TILES;++t){int tx=t%TX,ty=t/TX,count=0;
        for(int yy=0;yy<PH;++yy)for(int xx=0;xx<PW;++xx){
            int x=tx*PW+xx,y=ty*PH+yy,x0=std::max(0,x-pad),x1=std::min(W,x+pad+1),y0=std::max(0,y-pad),y1=std::min(H,y+pad+1);
            bool valid=prefix[y1*(W+1)+x1]-prefix[y0*(W+1)+x1]-prefix[y1*(W+1)+x0]+prefix[y0*(W+1)+x0]==0;
            feature.valid[t*PW*PH+yy*PW+xx]=valid;count+=valid;
        }
        feature.weight[t]=(tx==0||ty==0||tx==TX-1||ty==TY-1||count<16)?0.f:float(count)/(PW*PH);
    }
}
std::vector<double> costs(const std::vector<Feature>& source,const std::vector<Feature>& result,Progress progress){
    const int n=int(source.size()),m=int(result.size());
    if(!n||!m)throw std::runtime_error("Empty analysis sequence");
    const bool maskedAnalysis=std::any_of(source.begin(),source.end(),[](const Feature& f){return !f.valid.empty();}) || std::any_of(result.begin(),result.end(),[](const Feature& f){return !f.valid.empty();});
    std::array<float,TILES> motion{};
    for(int t=0;t<TILES;++t){
        if(n==1||maskedAnalysis){motion[t]=1;continue;}
        for(int i=1;i<n;++i){float sum=0;for(int k=0;k<DIM;++k){float d=source[i].values[t*DIM+k]-source[i-1].values[t*DIM+k];sum+=d*d;}motion[t]+=std::sqrt(sum)/(n-1);}
    }
    auto sorted=motion;std::sort(sorted.begin(),sorted.end());float ref=std::max(sorted[int(.75*(TILES-1))],1e-5f);
    for(float& x:motion)x=std::clamp(x/ref,.15f,1.f);
    std::vector<double> out(size_t(m)*n);
    std::atomic<int> next{0},done{0};std::atomic<bool> stop{false},insufficient{false};
    auto worker=[&]{
        while(!stop){int j=next++;if(j>=m)break;bool any=false;
            for(int i=0;i<n;++i){
                std::array<std::pair<float,float>,TILES> tiles;int used=0;
                const bool pairMasked=!source[i].valid.empty()||!result[j].valid.empty();
                for(int t=0;t<TILES;++t){
                    if(pairMasked&&(t%TX==0||t%TX==TX-1||t/TX==0||t/TX==TY-1))continue;
                    float weight=pairMasked?1.f:std::min(source[i].weight[t],result[j].weight[t])*motion[t];
                    if(weight<=.02f)continue;
                    const float* a=&source[i].values[t*DIM];const float* b=&result[j].values[t*DIM];float dot=0;
                    if(source[i].valid.empty()&&result[j].valid.empty()){
                        for(int k=0;k<DIM;++k)dot+=a[k]*b[k];
                    }else{
                        double sa=0,sb=0,saa=0,sbb=0,sab=0;int count=0;
                        for(int p=0;p<PW*PH;++p){
                            int index=t*PW*PH+p;
                            if((!source[i].valid.empty()&&!source[i].valid[index])||(!result[j].valid.empty()&&!result[j].valid[index]))continue;
                            for(int c=0;c<3;++c){double av=a[p*3+c],bv=b[p*3+c];sa+=av;sb+=bv;saa+=av*av;sbb+=bv*bv;sab+=av*bv;++count;}
                        }
                        if(count<48)continue;
                        double va=saa-sa*sa/count,vb=sbb-sb*sb/count;
                        if(va<1e-10||vb<1e-10)continue;
                        dot=float((sab-sa*sb/count)/std::sqrt(va*vb));
                        weight*=float(count)/DIM;
                    }
                    tiles[used++]={std::clamp(1-dot,0.f,2.f),weight};
                }
                if(used<8){out[size_t(j)*n+i]=10;continue;}any=true;
                std::sort(tiles.begin(),tiles.begin()+used,[](auto a,auto b){return a.first<b.first;});
                int keep=std::max(8,int(std::floor(used*.75)));double total=0,weights=0;
                for(int k=0;k<keep;++k){total+=tiles[k].first*tiles[k].second;weights+=tiles[k].second;}
                out[size_t(j)*n+i]=total/weights;
            }
            if(!any)insufficient=true;++done;
        }
    };
    int threads=std::min({8,m,int(std::max(1u,std::thread::hardware_concurrency()))});
    std::vector<std::thread> pool;for(int t=0;t<threads;++t)pool.emplace_back(worker);
    while(done<m&&!stop){if(progress&&!progress(double(done)/m))stop=true;std::this_thread::sleep_for(std::chrono::milliseconds(30));}
    for(auto& t:pool)t.join();
    if(stop)throw std::runtime_error("Analysis cancelled");
    if(insufficient)throw std::runtime_error("Insufficient unmasked image detail for reliable matching");
    return out;
}
static double median(std::vector<double> values){if(values.empty())return .01;std::sort(values.begin(),values.end());size_t n=values.size();return n%2?values[n/2]:(values[n/2-1]+values[n/2])*.5;}
struct Difference { double mean=0, high=0; };
static Difference imageDifference(const Feature& a,const Feature& b){
    if(a.luma.size()!=W*H||b.luma.size()!=W*H)throw std::runtime_error("Missing hold-analysis pixels");
    std::vector<double> differences;differences.reserve(W*H);double sum=0;
    for(int y=PH;y<H-PH;++y)for(int x=PW;x<W-PW;++x){
        int t=(y/PH)*TX+x/PW,index=t*PW*PH+(y%PH)*PW+x%PW;
        if((!a.valid.empty()&&!a.valid[index])||(!b.valid.empty()&&!b.valid[index]))continue;
        double d=std::abs(a.luma[y*W+x]-b.luma[y*W+x]);differences.push_back(d);sum+=d;
    }
    if(differences.size()<W*H/20)return {1,1};
    auto hi=differences.begin()+size_t(.95*(differences.size()-1));
    std::nth_element(differences.begin(),hi,differences.end());
    return {sum/differences.size(),*hi};
}
int markImageHolds(Solution& s,const std::vector<Feature>& source,const std::vector<Feature>& result){
    const int n=int(source.size());
    if(s.inverse.size()!=source.size()||s.missing.size()!=source.size())throw std::runtime_error("Invalid hold-analysis timeline");
    for(int j:s.inverse)if(j<0||j>=int(result.size()))throw std::runtime_error("Invalid hold-analysis lookup");
    s.imageHeld.assign(n,0);if(n<2)return 0;
    std::vector<Difference> output(n),reference(n);
    for(int i=1;i<n;++i){
        output[i]=imageDifference(result[s.inverse[i-1]],result[s.inverse[i]]);
        reference[i]=imageDifference(source[i-1],source[i]);
    }
    const auto originalMissing=s.missing;int count=0;std::vector<int> candidate(n,0);
    for(int i=1;i<n;++i){
        // Existing nearest-frame placeholders must not consume the next real
        // endpoint. They are already covered by the normal gap flags.
        if(originalMissing[i]||originalMissing[i-1]||s.inverse[i]<=s.inverse[i-1])continue;
        const auto d=output[i];
        if(d.mean>.006||d.high>.025)continue;
        // Require original motion and a pronounced drop in changed-video
        // motion. This avoids filling genuine still shots / slow movement.
        if(reference[i].mean<.008||d.mean>.2*reference[i].mean)continue;
        candidate[i]=1;
    }
    for(int start=1;start<n;){
        if(!candidate[start]){++start;continue;}
        int end=start;while(end+1<n&&candidate[end+1])++end;
        std::vector<double> nearby;
        for(int k=std::max(1,start-4);k<=std::min(n-1,end+4);++k)if(k<start||k>end)nearby.push_back(output[k].mean);
        const int next=end+1;
        if(nearby.empty()){start=next;continue;}
        std::sort(nearby.begin(),nearby.end());double motion=nearby[size_t(.75*(nearby.size()-1))];
        for(int i=start;i<=end;++i)if(motion>=.008&&output[i].mean<=.2*motion){
            s.missing[i]=1;s.imageHeld[i]=1;++count;
        }
        start=next;
    }
    return count;
}
static std::vector<double> forward(const std::vector<double>& c,int m,int n,double penalty,std::vector<int>* back){
    std::vector<double> f(c.size());std::copy(c.begin(),c.begin()+n,f.begin());
    if(back)back->assign(c.size(),-1);
    for(int j=1;j<m;++j){const double* prev=&f[size_t(j-1)*n];double prefix=std::numeric_limits<double>::infinity();int prefixIndex=-1;
        for(int i=0;i<n;++i){double v=prev[i]+penalty;int pred=i;
            if(i>0&&prev[i-1]<v){v=prev[i-1];pred=i-1;}
            if(i>1){int k=i-2;double adjusted=prev[k]-penalty*k;if(adjusted<prefix){prefix=adjusted;prefixIndex=k;}
                double jump=prefix+penalty*(i-1);if(jump<v){v=jump;pred=prefixIndex;}}
            f[size_t(j)*n+i]=v+c[size_t(j)*n+i];if(back)(*back)[size_t(j)*n+i]=pred;
        }
    }return f;
}
Solution solve(const std::vector<double>& cost,int m,int n,double regularization){
    if(m<1||n<1||cost.size()!=size_t(m)*n)throw std::runtime_error("Invalid cost matrix");
    for(double x:cost)if(!std::isfinite(x))throw std::runtime_error("Non-finite matching cost");
    std::vector<double> adjacent;
    for(int j=0;j<m;++j){const double* row=&cost[size_t(j)*n];int i=int(std::min_element(row,row+n)-row);
        if(i)adjacent.push_back(std::abs(row[i-1]-row[i]));if(i<n-1)adjacent.push_back(std::abs(row[i+1]-row[i]));}
    double unit=std::max(median(adjacent),.0001),penalty=unit*regularization;
    std::vector<int> back;auto f=forward(cost,m,n,penalty,&back);
    Solution s;s.penalty=penalty;s.path.resize(m);s.confidence.resize(m);
    s.path[m-1]=int(std::min_element(f.end()-n,f.end())-(f.end()-n));
    for(int j=m-1;j>0;--j)s.path[j-1]=back[size_t(j)*n+s.path[j]];
    std::vector<double> reversed(cost.rbegin(),cost.rend());auto b=forward(reversed,m,n,penalty,nullptr);
    for(int j=0;j<m;++j){int i=s.path[j];double chosen=f[size_t(j)*n+i]+b[size_t(m-1-j)*n+n-1-i]-cost[size_t(j)*n+i];
        double other=std::numeric_limits<double>::infinity();
        for(int k=0;k<n;++k)if(k!=i)other=std::min(other,f[size_t(j)*n+k]+b[size_t(m-1-j)*n+n-1-k]-cost[size_t(j)*n+k]);
        double margin=n>1?std::max(0.,other-chosen):0.;s.confidence[j]=margin/(margin+unit);
        if(s.confidence[j]<.25||cost[size_t(j)*n+i]>.45)++s.reviews;
        if(j){int d=i-s.path[j-1];if(d<0)throw std::runtime_error("Reverse path rejected");if(d==0)++s.holds;if(d>1)s.skips+=d-1;}
    }
    std::vector<int> best(n,-1);
    for(int j=0;j<m;++j){int i=s.path[j];if(best[i]<0||cost[size_t(j)*n+i]<cost[size_t(best[i])*n+i])best[i]=j;}
    s.inverse.resize(n);s.missing.resize(n);
    for(int i=0;i<n;++i){int nearest=-1,distance=n+1;for(int k=0;k<n;++k)if(best[k]>=0&&std::abs(k-i)<distance){nearest=k;distance=std::abs(k-i);}
        s.inverse[i]=best[nearest];s.missing[i]=(nearest!=i);
        if(i&&s.inverse[i]<s.inverse[i-1])throw std::runtime_error("Reverse lookup rejected");
    }return s;
}
}
