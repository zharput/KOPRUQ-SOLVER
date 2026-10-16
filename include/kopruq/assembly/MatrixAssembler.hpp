#pragma once
#include "DofManager.hpp"
#include "../elements/Beam3D.hpp"
#include "../elements/Releases.hpp"
#include <Eigen/Sparse>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
namespace kopruq {
namespace detail {
// ID -> pointer index built once per call, replacing per-element linear scans.
// Later entries overwrite earlier ones, matching the previous scan's last-match result.
template<class T> std::unordered_map<Id,const T*> index_by_id(const std::vector<T>& v){ std::unordered_map<Id,const T*> r; r.reserve(v.size()); for(const auto& x:v) r[x.id]=&x; return r; }
template<class T> const T* find_by_id(const std::unordered_map<Id,const T*>& idx,Id id){ auto it=idx.find(id); return it==idx.end()?nullptr:it->second; }
}
struct BoundaryMap { std::vector<std::size_t> free,fixed; };
struct ComponentReport { std::vector<std::vector<Id>> components; std::vector<Id> isolated; };
class MatrixAssembler {
 public:
  static BoundaryMap classify(const DofManager& dm,const std::map<Id,std::array<bool,6>>& rs){ BoundaryMap out; for(const auto& [id,b]:rs){ auto d=dm.node_dofs(id); for(int i=0;i<6;++i)(b[i]?out.fixed:out.free).push_back(d[i]);} return out; }
  static BoundaryMap classify(const DofManager& dm,const std::vector<std::pair<Id,std::array<bool,6>>>& rs){ BoundaryMap out; std::set<Id> seen; for(const auto& [id,b]:rs){if(!seen.insert(id).second)throw AssemblyError(AssemblyErrorCode::MissingNode,"duplicate restraint");auto d=dm.node_dofs(id);for(int i=0;i<6;++i)(b[i]?out.fixed:out.free).push_back(d[i]);}return out; }
  static ComponentReport components(const Model&m){ DofManager dm(m); std::map<Id,std::set<Id>> a; for(const auto&n:m.nodes)a[n.id]; for(const auto&e:m.beams){if(!a.count(e.node1)||!a.count(e.node2))throw AssemblyError(AssemblyErrorCode::MissingNode,"missing connectivity node");a[e.node1].insert(e.node2);a[e.node2].insert(e.node1);} ComponentReport r;std::set<Id> done;for(const auto&[id,_]:a)if(!done.count(id)){std::vector<Id> c,q{id};done.insert(id);while(!q.empty()){Id x=q.back();q.pop_back();c.push_back(x);for(Id y:a[x])if(done.insert(y).second)q.push_back(y);}std::sort(c.begin(),c.end());if(c.size()==1&&a[id].empty())r.isolated.push_back(id);r.components.push_back(c);}return r; }
  static Eigen::SparseMatrix<Real> assemble(const Model&m){ DofManager dm(m);std::set<Id> seen;std::vector<Eigen::Triplet<Real>> t;t.reserve(m.beams.size()*144);auto nodes=detail::index_by_id(m.nodes);auto mats=detail::index_by_id(m.materials);auto secs=detail::index_by_id(m.sections);for(const auto&e:m.beams){if(!seen.insert(e.id).second)throw AssemblyError(AssemblyErrorCode::DuplicateNode,"duplicate element id");const Node*n1=detail::find_by_id(nodes,e.node1),*n2=detail::find_by_id(nodes,e.node2);if(!n1||!n2)throw AssemblyError(AssemblyErrorCode::MissingNode,"element references missing node");const Material*mat=detail::find_by_id(mats,e.material);const Section*sec=detail::find_by_id(secs,e.section);if(!mat||!sec)throw AssemblyError(AssemblyErrorCode::MissingNode,"element property reference missing");mat->validate();sec->validate();Beam3DProperties p{mat->elastic_modulus,mat->elastic_modulus/(2*(1+mat->poisson_ratio)),sec->area,sec->Iy,sec->Iz,sec->J};Beam3DGeometry g{{n1->x,n1->y,n1->z},{n2->x,n2->y,n2->z},e.reference_y};Beam3DMatrix k;if(anyReleased(e.releases)){Beam3D::validate(g,p);auto T=Beam3D::transformation(Beam3D::axes(g));k=T.transpose()*condensedStiffness(Beam3D::localStiffness((g.node2-g.node1).norm(),p),e.releases)*T;}else k=Beam3D::globalStiffness(g,p);auto d=dm.element_dofs(e.node1,e.node2);for(int i=0;i<12;++i)for(int j=0;j<12;++j)if(k(i,j)!=0)t.emplace_back((int)d[i],(int)d[j],k(i,j));}Eigen::SparseMatrix<Real>K((int)dm.dof_count(),(int)dm.dof_count());K.setFromTriplets(t.begin(),t.end());K.makeCompressed();return K; }
}; }
