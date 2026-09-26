#pragma once
#include <vector>
#include <functional>

namespace fm {
constexpr int W=192,H=108,TX=12,TY=9,TILES=TX*TY,PW=16,PH=12,DIM=PW*PH*3;
struct Feature { std::vector<float> values; std::vector<float> weight; std::vector<unsigned char> valid; std::vector<float> luma; };
struct Solution {
    std::vector<int> path, inverse, missing, imageHeld;
    std::vector<double> confidence;
    int holds=0, skips=0, reviews=0;
    double penalty=0;
};
using Progress=std::function<bool(double)>;
Feature features(const std::vector<float>& gray);
void excludeMask(Feature& feature,const std::vector<float>& alpha);
std::vector<double> costs(const std::vector<Feature>& source,const std::vector<Feature>& result,Progress progress={});
Solution solve(const std::vector<double>& cost,int resultCount,int sourceCount,double regularization=.15);
// Inspect the final output timeline as well as index gaps: distinct Change
// frame numbers can still contain the same held image after lossy encoding.
int markImageHolds(Solution& solution,const std::vector<Feature>& source,const std::vector<Feature>& result);
}
