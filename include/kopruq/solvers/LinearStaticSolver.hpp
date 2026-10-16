#pragma once
#include "SolverStatus.hpp"
#include "../assembly/MatrixAssembler.hpp"
#include "../loads/Loads.hpp"
#include "../elements/Releases.hpp"
#include "../results/AnalysisResults.hpp"
#include <Eigen/SparseCholesky>
#include <map>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <limits>
namespace kopruq {
// residual_norm = ||Kff*u_f - F_f||_2; relative_residual = residual_norm / (||Kff||_F*||u_f||_2 + ||F_f||_2), 0 when the denominator is 0.
// A solution with any non-finite free displacement is reported as NonFiniteSolution and not recovered.
// fixed = support reactions on the restrained DOFs, in ascending global DOF order.
struct StaticSolution{SolverStatus status;Eigen::VectorXd global,free,reactions;std::vector<Real> fixed;std::size_t total_dofs{},free_dofs{},fixed_dofs{};Real residual_norm{},relative_residual{};AnalysisResults results;};
// One StaticSolution per load case and per combination, all from a single factorization of Kff.
struct MultiCaseSolution{SolverStatus status;std::vector<StaticSolution> cases,combinations;};

// Member-load effects on one element: equivalent nodal loads in local axes, resultant force and moment about node1 in global axes.
struct ElementLoad{detail::Vec12 equivalent_local=detail::Vec12::Zero();Eigen::Vector3d force=Eigen::Vector3d::Zero(),moment_node1=Eigen::Vector3d::Zero();};
// A load case assembled for the global system. force/moment: resultant of all applied loads, moment about the global origin.
struct AppliedLoads{Eigen::VectorXd F;std::unordered_map<Id,ElementLoad> elements;Eigen::Vector3d force=Eigen::Vector3d::Zero(),moment=Eigen::Vector3d::Zero();};

class LinearStaticSolver{public:
using Restraints=kopruq::Restraints;  // kept for source compatibility; see Model.hpp

static Eigen::SparseMatrix<Real> reduceKff(const Eigen::SparseMatrix<Real>&K,const std::vector<std::size_t>&f){std::vector<int>mp((size_t)K.rows(),-1);for(size_t i=0;i<f.size();++i)mp[f[i]]=(int)i;std::vector<Eigen::Triplet<Real>>t;for(int c=0;c<K.outerSize();++c)for(Eigen::SparseMatrix<Real>::InnerIterator it(K,c);it;++it)if(mp[it.row()]>=0&&mp[it.col()]>=0)t.emplace_back(mp[it.row()],mp[it.col()],it.value());Eigen::SparseMatrix<Real>o((int)f.size(),(int)f.size());o.setFromTriplets(t.begin(),t.end());return o;}

static Eigen::VectorXd assembleLoads(const DofManager&d,const std::vector<NodalLoad>&ls){Eigen::VectorXd f=Eigen::VectorXd::Zero((Eigen::Index)d.dof_count());for(const auto&l:ls){if(!l.values.allFinite())throw AssemblyError(AssemblyErrorCode::MissingNode,"nodal load is non-finite");auto a=d.node_dofs(l.node);for(int i=0;i<6;++i)f((Eigen::Index)a[i])+=l.values(i);}return f;}

// Nodal loads, member loads and self-weight of one load case: global load vector (including consistent
// equivalent nodal loads), per-element member-load effects, and the resultant of everything applied.
static AppliedLoads assembleLoadCase(const Model&m,const DofManager&dm,const LoadCase&lc){
  AppliedLoads a;
  a.F=assembleLoads(dm,lc.nodal);
  auto nodes=detail::index_by_id(m.nodes);
  for(const auto&l:lc.nodal){
    const Node*n=detail::find_by_id(nodes,l.node);
    Eigen::Vector3d r(n->x,n->y,n->z),f=l.values.head<3>(),mm=l.values.tail<3>();
    a.force+=f; a.moment+=mm+r.cross(f);
  }
  std::vector<MemberLoad> loads=lc.member;
  if(!std::isfinite(lc.self_weight))throw AssemblyError(AssemblyErrorCode::InvalidLoad,"self-weight factor is non-finite");
  if(lc.self_weight!=0){
    auto mats=detail::index_by_id(m.materials); auto secs=detail::index_by_id(m.sections);
    for(const auto&e:m.beams){
      const Material*mat=detail::find_by_id(mats,e.material); const Section*sec=detail::find_by_id(secs,e.section);
      if(!mat||!sec)throw AssemblyError(AssemblyErrorCode::MissingNode,"element property reference missing");
      Real w=-mat->unit_weight*sec->area*lc.self_weight;
      if(w!=0)loads.push_back(MemberLoad{e.id,LoadAxes::Global,2,w,w,0,1});
    }
  }
  if(loads.empty()&&lc.point.empty())return a;
  std::unordered_map<Id,const Beam3DConnection*> beams; beams.reserve(m.beams.size());
  for(const auto&e:m.beams)beams[e.id]=&e;
  auto matIdx=detail::index_by_id(m.materials); auto secIdx=detail::index_by_id(m.sections);
  struct Frame{const Beam3DConnection*e;Beam3DGeometry g;Beam3DAxes ax;Eigen::Matrix3d R;Real L;bool released;Beam3DMatrix kl;};
  auto frameOf=[&](Id element,const char*what){
    auto it=beams.find(element);
    if(it==beams.end())throw AssemblyError(AssemblyErrorCode::InvalidLoad,std::string(what)+" references missing element");
    const auto&e=*it->second;
    const Node*n1=detail::find_by_id(nodes,e.node1),*n2=detail::find_by_id(nodes,e.node2);
    if(!n1||!n2)throw AssemblyError(AssemblyErrorCode::MissingNode,"element references missing node");
    Frame f{&e,Beam3DGeometry{{n1->x,n1->y,n1->z},{n2->x,n2->y,n2->z},e.reference_y},{},{},0,anyReleased(e.releases),{}};
    f.ax=Beam3D::axes(f.g);
    f.R.row(0)=f.ax.x.transpose(); f.R.row(1)=f.ax.y.transpose(); f.R.row(2)=f.ax.z.transpose();
    f.L=(f.g.node2-f.g.node1).norm();
    if(f.released){
      // Released members pass their loads to the nodes through the condensed load vector.
      const Material*mat=detail::find_by_id(matIdx,e.material); const Section*sec=detail::find_by_id(secIdx,e.section);
      if(!mat||!sec)throw AssemblyError(AssemblyErrorCode::MissingNode,"element property reference missing");
      f.kl=Beam3D::localStiffness(f.L,Beam3DProperties{mat->elastic_modulus,mat->elastic_modulus/(2*(1+mat->poisson_ratio)),sec->area,sec->Iy,sec->Iz,sec->J});
    }
    return f;
  };
  auto accumulate=[&](const Frame&f,const detail::MemberLoadEffect&eff){
    const auto&e=*f.e;
    auto d=dm.element_dofs(e.node1,e.node2);
    const detail::Vec12 fc=f.released?condensedLoad(f.kl,e.releases,eff.equivalent):eff.equivalent;
    for(int b=0;b<4;++b){Eigen::Vector3d v=f.R.transpose()*fc.segment<3>(3*b);for(int i=0;i<3;++i)a.F((Eigen::Index)d[3*b+i])+=v(i);}
    Eigen::Vector3d fg=f.R.transpose()*eff.force, mg=f.R.transpose()*eff.moment_node1;
    auto&el=a.elements[e.id];
    el.equivalent_local+=eff.equivalent; el.force+=fg; el.moment_node1+=mg;
    a.force+=fg; a.moment+=mg+f.g.node1.cross(fg);
  };
  for(const auto&ld:loads){
    if(ld.direction<0||ld.direction>2)throw AssemblyError(AssemblyErrorCode::InvalidLoad,"member load direction must be 0, 1 or 2");
    if(!std::isfinite(ld.w1)||!std::isfinite(ld.w2)||!std::isfinite(ld.start)||!std::isfinite(ld.end)||ld.start<0||ld.end>1||!(ld.start<ld.end))
      throw AssemblyError(AssemblyErrorCode::InvalidLoad,"member load intensity or segment is invalid");
    if(ld.projected&&ld.axes!=LoadAxes::Global)throw AssemblyError(AssemblyErrorCode::InvalidLoad,"projected member loads must be in global axes");
    if(ld.projected&&ld.kind!=MemberLoadKind::Force)throw AssemblyError(AssemblyErrorCode::InvalidLoad,"projected member loads must be forces");
    auto f=frameOf(ld.element,"member load");
    MemberLoad trueLength=ld;
    if(ld.projected){
      // Projection onto the plane normal to the load direction: L_p / L = sqrt(1 - (x . d)^2).
      Real c=f.ax.x(ld.direction);
      Real ratio=std::sqrt(std::max(Real(0),1-c*c));
      trueLength.w1*=ratio; trueLength.w2*=ratio; trueLength.projected=false;
    }
    accumulate(f,detail::memberLoadEffect(trueLength,f.R,f.L));
  }
  for(const auto&ld:lc.point){
    if(ld.direction<0||ld.direction>2)throw AssemblyError(AssemblyErrorCode::InvalidLoad,"member point load direction must be 0, 1 or 2");
    if(!std::isfinite(ld.value)||!std::isfinite(ld.position)||ld.position<0||ld.position>1)
      throw AssemblyError(AssemblyErrorCode::InvalidLoad,"member point load value or position is invalid");
    auto f=frameOf(ld.element,"member point load");
    accumulate(f,detail::pointLoadEffect(ld,f.R,f.L));
  }
  return a;
}

// Results for a displacement field U under an assembled load case; K is the full (unreduced) stiffness matrix.
static AnalysisResults recoverAssembled(const Model&m,const Restraints&rs,const DofManager&dm,const Eigen::SparseMatrix<Real>&K,const AppliedLoads&ap,const Eigen::VectorXd&U){
  Eigen::VectorXd q=K*U-ap.F;AnalysisResults o;auto nodes=detail::index_by_id(m.nodes);o.nodes.reserve(m.nodes.size());o.elements.reserve(m.beams.size());Real sfb=0,sfs=0,smb=0,sms=0;
  for(const auto&n:m.nodes){NodeResults z;z.node_id=n.id;auto d=dm.node_dofs(n.id);auto it=rs.find(n.id);z.restrained=it!=rs.end()?it->second:std::array<bool,6>{};for(int i=0;i<6;++i){z.displacement[i]=U((Eigen::Index)d[i]);z.reaction[i]=z.restrained[i]?q((Eigen::Index)d[i]):0;}o.nodes.push_back(z);}
  std::map<Id,const Material*>ma;for(const auto&x:m.materials)ma[x.id]=&x;std::map<Id,const Section*>se;for(const auto&x:m.sections)se[x.id]=&x;
  for(const auto&e:m.beams){
    const Node*a=detail::find_by_id(nodes,e.node1),*b=detail::find_by_id(nodes,e.node2);auto&p=*ma[e.material];auto&s=*se[e.section];
    Beam3DGeometry g{{a->x,a->y,a->z},{b->x,b->y,b->z},e.reference_y};Beam3DProperties bp{p.elastic_modulus,p.elastic_modulus/(2*(1+p.poisson_ratio)),s.area,s.Iy,s.Iz,s.J};
    auto T=Beam3D::transformation(Beam3D::axes(g));auto kl=Beam3D::localStiffness((g.node2-g.node1).norm(),bp);auto d=dm.element_dofs(e.node1,e.node2);
    auto li=ap.elements.find(e.id);const ElementLoad*el=li==ap.elements.end()?nullptr:&li->second;
    Eigen::Matrix<Real,12,1>u,ul,kul,fl,fg;for(int i=0;i<12;++i)u(i)=U((Eigen::Index)d[i]);ul=T*u;
    Real energy;
    if(anyReleased(e.releases)){
      // Released DOFs: use the element's own end displacements, u_r = K_rr^-1 (f_r - K_ra u_a); their end forces vanish.
      const detail::Vec12 feq=el?el->equivalent_local:detail::Vec12::Zero();
      const detail::Vec12 uf=completeDisplacements(kl,e.releases,ul,feq);
      kul=kl*uf;fl=kul-feq;energy=0.5*ul.dot(condensedStiffness(kl,e.releases)*ul);
    }else{
      kul=kl*ul;
      // End forces exerted by the nodes on the element: k*u minus the member loads' equivalent nodal loads.
      fl=el?(kul-el->equivalent_local).eval():kul;energy=0.5*ul.dot(kul);
    }
    fg=T.transpose()*fl;
    ElementResults er;er.element_id=e.id;for(int i=0;i<12;++i){er.local_end_force[i]=fl(i);er.global_end_force[i]=fg(i);}er.strain_energy=energy;o.element_strain_energy+=er.strain_energy;
    Eigen::Vector3d f1=fg.template head<3>(),f2=fg.template segment<3>(6),m1=fg.template segment<3>(3),m2=fg.template segment<3>(9),r=(g.node2-g.node1).eval();
    Eigen::Vector3d lf=el?el->force:Eigen::Vector3d::Zero(),lm=el?el->moment_node1:Eigen::Vector3d::Zero();
    er.force_balance=f1+f2+lf;er.moment_balance=m1+m2+r.cross(f2)+lm;er.force_balance_norm=er.force_balance.norm();er.moment_balance_norm=er.moment_balance.norm();
    Real fs=f1.norm()+f2.norm()+lf.norm(),ms=m1.norm()+m2.norm()+r.norm()*(f1.norm()+f2.norm())+lm.norm();
    er.normalized_force_error=fs>0?er.force_balance_norm/fs:0;er.normalized_moment_error=ms>0?er.moment_balance_norm/ms:0;sfb+=er.force_balance_norm;sfs+=fs;smb+=er.moment_balance_norm;sms+=ms;
    if(o.elements.empty()||er.normalized_force_error>o.max_element_force_error){o.max_element_force_error=er.normalized_force_error;o.max_force_element_id=e.id;}
    if(o.elements.empty()||er.normalized_moment_error>o.max_element_moment_error){o.max_element_moment_error=er.normalized_moment_error;o.max_moment_element_id=e.id;}
    o.elements.push_back(er);
  }
  o.element_equilibrium_error=sfs>0?sfb/sfs:0;o.element_moment_equilibrium_error=sms>0?smb/sms:0;
  Eigen::Vector3d af=ap.force,ar=Eigen::Vector3d::Zero(),am=ap.moment,arm=Eigen::Vector3d::Zero();
  for(const auto&n:o.nodes){const Node*x=detail::find_by_id(nodes,n.node_id);Eigen::Vector3d r(x->x,x->y,x->z),f(n.reaction[0],n.reaction[1],n.reaction[2]),mm(n.reaction[3],n.reaction[4],n.reaction[5]);ar+=f;arm+=mm+r.cross(f);}
  o.force_balance=af+ar;o.moment_balance=am+arm;o.force_balance_norm=o.force_balance.norm();o.moment_balance_norm=o.moment_balance.norm();
  // Force scale: applied plus reaction forces, plus moments (about the model's bounding-box centre) divided by the model
  // size, so a purely moment-loaded case is measured against its moments instead of against round-off forces.
  Eigen::Vector3d lo=Eigen::Vector3d::Constant(std::numeric_limits<Real>::max()),hi=-lo;for(const auto&n:m.nodes){Eigen::Vector3d p(n.x,n.y,n.z);lo=lo.cwiseMin(p);hi=hi.cwiseMax(p);}
  Real Lc=m.nodes.empty()?0:(hi-lo).norm();Real fscale=af.norm()+ar.norm();if(Lc>0){Eigen::Vector3d c=0.5*(lo+hi);fscale+=((am-c.cross(af)).norm()+(arm-c.cross(ar)).norm())/Lc;}
  o.force_equilibrium_error=o.force_balance_norm/(fscale>0?fscale:1);
  o.moment_equilibrium_error=o.moment_balance_norm/(am.norm()+arm.norm()>0?am.norm()+arm.norm():1);
  // External work of the assembled (consistent) loads; equals the sum of element 0.5*u^T*k*u.
  o.strain_energy=0.5*U.dot(ap.F);Real escale=std::max(std::abs(o.strain_energy),std::abs(o.element_strain_energy));o.energy_error=escale>0?std::abs(o.strain_energy-o.element_strain_energy)/escale:0;o.valid=true;return o;
}

static AnalysisResults recover(const Model&m,const Restraints&rs,const LoadCase&lc,const Eigen::VectorXd&U){DofManager dm(m);auto ap=assembleLoadCase(m,dm,lc);auto K=MatrixAssembler::assemble(m);return recoverAssembled(m,rs,dm,K,ap,U);}
static AnalysisResults recover(const Model&m,const Restraints&rs,const std::vector<NodalLoad>&ls,const Eigen::VectorXd&U){return recover(m,rs,LoadCase{"",ls,{},0},U);}

static StaticSolution solve(const Model&m,const Restraints&rs,const std::vector<NodalLoad>&ls){return solveCases(m,rs,{LoadCase{"",ls,{},0}}).cases.front();}

// Solves every load case with one factorization of Kff, then each combination by superposition of the case
// displacements (recovered with the correspondingly combined loads).
static MultiCaseSolution solveCases(const Model&m,const Restraints&rs,const std::vector<LoadCase>&cases,const std::vector<LoadCombination>&combos={}){
  MultiCaseSolution out;out.cases.resize(cases.size());out.combinations.resize(combos.size());
  auto fail=[&](const SolverStatus&st){out.status=st;for(auto&c:out.cases)c.status=st;for(auto&c:out.combinations)c.status=st;return out;};
  try{
    DofManager dm(m);
    for(auto&c:out.cases)c.total_dofs=dm.dof_count();
    for(auto&c:out.combinations)c.total_dofs=dm.dof_count();
    if(rs.empty())return fail({false,"no restraints",SolverStatusCode::NoRestraints});
    for(const auto&cb:combos)for(const auto&t:cb.terms)if(t.first>=cases.size()||!std::isfinite(t.second))throw AssemblyError(AssemblyErrorCode::InvalidLoad,"load combination term is invalid");
    auto all=rs;for(const auto&n:m.nodes)if(!all.count(n.id))all[n.id]={};
    auto b=MatrixAssembler::classify(dm,all);
    for(auto&c:out.cases){c.free_dofs=b.free.size();c.fixed_dofs=b.fixed.size();}
    for(auto&c:out.combinations){c.free_dofs=b.free.size();c.fixed_dofs=b.fixed.size();}
    std::vector<AppliedLoads> applied;applied.reserve(cases.size());for(const auto&lc:cases)applied.push_back(assembleLoadCase(m,dm,lc));
    Eigen::SparseMatrix<Real>K=MatrixAssembler::assemble(m);Eigen::SparseMatrix<Real>Kff=reduceKff(K,b.free);
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<Real>>fac;fac.compute(Kff);
    if(fac.info()!=Eigen::Success)return fail({false,"singular system",SolverStatusCode::SingularSystem});
    auto freeLoads=[&](const AppliedLoads&ap){Eigen::VectorXd Ff((Eigen::Index)b.free.size());for(size_t i=0;i<b.free.size();++i)Ff((Eigen::Index)i)=ap.F((Eigen::Index)b.free[i]);return Ff;};
    // Residual, scatter, recovery and reactions for a free-DOF solution already in o.free.
    auto finish=[&](StaticSolution&o,const AppliedLoads&ap,const Eigen::VectorXd&Ff){
      if(!o.free.allFinite()){o.status={false,"non-finite solution",SolverStatusCode::NonFiniteSolution};return;}
      {Eigen::VectorXd res=Kff*o.free-Ff;o.residual_norm=res.norm();Real den=Kff.norm()*o.free.norm()+Ff.norm();o.relative_residual=den>0?o.residual_norm/den:0;}
      o.global=Eigen::VectorXd::Zero((Eigen::Index)dm.dof_count());for(size_t i=0;i<b.free.size();++i)o.global((Eigen::Index)b.free[i])=o.free((Eigen::Index)i);
      o.results=recoverAssembled(m,all,dm,K,ap,o.global);
      o.reactions=Eigen::VectorXd::Zero((Eigen::Index)dm.dof_count());for(const auto&n:o.results.nodes){auto d=dm.node_dofs(n.node_id);for(int i=0;i<6;++i)o.reactions((Eigen::Index)d[i])=n.reaction[i];}
      o.fixed.resize(b.fixed.size());for(size_t i=0;i<b.fixed.size();++i)o.fixed[i]=o.reactions((Eigen::Index)b.fixed[i]);
      o.status={true,"linear static solution and recovery completed",SolverStatusCode::Success};
    };
    for(size_t c=0;c<cases.size();++c){auto Ff=freeLoads(applied[c]);out.cases[c].free=fac.solve(Ff);finish(out.cases[c],applied[c],Ff);}
    for(size_t k=0;k<combos.size();++k){
      auto&o=out.combinations[k];
      LoadCase lc;lc.name=combos[k].name;
      Eigen::VectorXd uf=Eigen::VectorXd::Zero((Eigen::Index)b.free.size());
      const SolverStatus*failed=nullptr;
      for(const auto&[idx,f]:combos[k].terms){
        if(!out.cases[idx].status.success){failed=&out.cases[idx].status;break;}
        uf+=f*out.cases[idx].free;
        for(auto l:cases[idx].nodal){l.values*=f;lc.nodal.push_back(l);}
        for(auto l:cases[idx].member){l.w1*=f;l.w2*=f;lc.member.push_back(l);}
        for(auto l:cases[idx].point){l.value*=f;lc.point.push_back(l);}
        lc.self_weight+=f*cases[idx].self_weight;
      }
      if(failed){o.status=*failed;continue;}
      auto ap=assembleLoadCase(m,dm,lc);auto Ff=freeLoads(ap);o.free=uf;finish(o,ap,Ff);
    }
    out.status={true,"linear static solution and recovery completed",SolverStatusCode::Success};
    for(const auto&c:out.cases)if(!c.status.success){out.status=c.status;break;}
    if(out.status.success)for(const auto&c:out.combinations)if(!c.status.success){out.status=c.status;break;}
  }catch(const std::exception&e){return fail({false,e.what(),SolverStatusCode::InvalidModel});}
  return out;
}
};}
