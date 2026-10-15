#include "kopruq/assembly/MatrixAssembler.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
int main(){std::cout<<"Release sparse benchmark; warmup=2 measured=5\n";for(int n:{100,1000,10000}){kopruq::Model m;m.materials={{1,210e6,.3}};m.sections={{1,.3,.02,.04,.01}};for(int i=0;i<=n;++i)m.nodes.push_back({static_cast<kopruq::Id>(i+1),double(i),0,0});for(int i=0;i<n;++i)m.beams.push_back({static_cast<kopruq::Id>(i+1),static_cast<kopruq::Id>(i+1),static_cast<kopruq::Id>(i+2),1,1,{0,1,0}});for(int i=0;i<2;++i)kopruq::MatrixAssembler::assemble(m);std::vector<double> v;for(int i=0;i<5;++i){auto t=std::chrono::steady_clock::now();auto K=kopruq::MatrixAssembler::assemble(m);v.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());}std::sort(v.begin(),v.end());auto K=kopruq::MatrixAssembler::assemble(m);std::cout<<n<<" elements, nodes="<<m.nodes.size()<<", dofs="<<K.rows()<<", nnz="<<K.nonZeros()<<", median_ms="<<v[2]<<", min_ms="<<v.front()<<", max_ms="<<v.back()<<", bytes="<<K.nonZeros()*(sizeof(double)+2*sizeof(Eigen::Index))<<"\n";}}
