#pragma once
#pragma once
#include "../core/Model.hpp"
#include <array>
#include <map>
#include <vector>
#include <algorithm>
#include <set>
#include <stdexcept>
namespace kopruq { enum class AssemblyErrorCode { EmptyModel, DuplicateNode, MissingNode, Overflow, InvalidLoad }; struct AssemblyError:std::invalid_argument { AssemblyErrorCode code; AssemblyError(AssemblyErrorCode c,const std::string&m):std::invalid_argument(m),code(c){} }; class DofManager { std::map<Id,std::size_t> offsets_; std::size_t count_{}; public: explicit DofManager(const Model&m){if(m.nodes.empty())throw AssemblyError(AssemblyErrorCode::EmptyModel,"structural model is empty"); std::vector<Id> ids; std::set<Id> seen; for(const auto&n:m.nodes){if(n.id==0||!seen.insert(n.id).second)throw AssemblyError(AssemblyErrorCode::DuplicateNode,"duplicate node id");ids.push_back(n.id);} std::sort(ids.begin(),ids.end()); for(std::size_t i=0;i<ids.size();++i)offsets_[ids[i]]=i*kDofPerNode;count_=ids.size()*kDofPerNode;} std::size_t dof_count()const{return count_;} std::array<std::size_t,6> node_dofs(Id id)const{auto it=offsets_.find(id);if(it==offsets_.end())throw AssemblyError(AssemblyErrorCode::MissingNode,"missing node");std::array<std::size_t,6>r{};for(int i=0;i<6;++i)r[i]=it->second+i;return r;} std::array<std::size_t,12> element_dofs(Id a,Id b)const{auto x=node_dofs(a),y=node_dofs(b);std::array<std::size_t,12>r{};for(int i=0;i<6;++i){r[i]=x[i];r[i+6]=y[i];}return r;} }; }
