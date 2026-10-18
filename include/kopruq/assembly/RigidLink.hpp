#pragma once
// General rigid links (master-slave multi-point constraints): a slave
// node's 6 DOFs are entirely determined by a master node's 6 DOFs via the
// same rigid-body kinematics as a member's eccentric end offset
// (kopruq/elements/Offsets.hpp):
//   u_slave = u_master + theta_master x e,   theta_slave = theta_master,
// e = slave position - master position (from node coordinates, not
// user-specified). One master may have many slaves (e.g. a rigid
// diaphragm); a slave may have only one master, and chaining (a master
// that is itself a slave) is rejected.
//
// Implemented as a global DOF-elimination layer (static/Guyan reduction)
// wrapped entirely around the EXISTING, unmodified assembled K/F/U:
//   K_reduced = T^T K T,   F_reduced = T^T F,   U_full = T U_reduced,
// where T maps the "reduced" DOF space (every DOF except a slave's) to
// the full DOF space. MatrixAssembler::assemble/assembleMass/
// assembleLoadCase and LinearStaticSolver::recoverAssembled never see
// this reduction -- they always receive/return full-DOF quantities
// exactly as when no rigid links exist, so nothing about a model with an
// empty `rigid_links` is touched by this file.
//
// A slave node cannot carry its own restraints: its motion is already
// fully determined by its master, so restraining it directly would be an
// independent (and generally inconsistent) extra constraint. Validated by
// the caller (LinearStaticSolver::solveCasesWithRigidLinks), not here.
#include "DofManager.hpp"
#include "MatrixAssembler.hpp"
#include <Eigen/Sparse>
#include <map>
#include <set>
#include <vector>

namespace kopruq {

namespace detail {
inline Eigen::Matrix3d rigidLinkSkew(const Eigen::Vector3d& e) {
  Eigen::Matrix3d s;
  s <<      0, -e.z(),  e.y(),
         e.z(),      0, -e.x(),
        -e.y(),  e.x(),      0;
  return s;
}
} // namespace detail

class RigidLinkReduction {
 public:
  Eigen::SparseMatrix<Real> T;              // full_dofs x reduced_dofs; U_full = T * U_reduced
  std::set<Id> slaveNodes;

  std::size_t reduced_count() const { return static_cast<std::size_t>(T.cols()); }

  static RigidLinkReduction build(const Model& m, const DofManager& dm) {
    RigidLinkReduction r;
    std::map<Id, Id> slaveToMaster;
    std::set<Id> seenIds;
    for (const auto& l : m.rigid_links) {
      if (!seenIds.insert(l.id).second) throw AssemblyError(AssemblyErrorCode::DuplicateNode, "duplicate rigid link id");
      if (l.master == l.slave) throw AssemblyError(AssemblyErrorCode::MissingNode, "rigid link master and slave must be different nodes");
      if (slaveToMaster.count(l.slave)) throw AssemblyError(AssemblyErrorCode::MissingNode, "node " + std::to_string(l.slave) + " is a slave of more than one rigid link");
      slaveToMaster[l.slave] = l.master;
    }
    for (const auto& [slave, master] : slaveToMaster)
      if (slaveToMaster.count(master)) throw AssemblyError(AssemblyErrorCode::MissingNode, "rigid link chaining (a master that is itself a slave) is not supported");
    for (const auto& [slave, _] : slaveToMaster) r.slaveNodes.insert(slave);

    auto nodes = detail::index_by_id(m.nodes);
    std::vector<std::size_t> retained;
    r.fullToReduced_.clear();
    for (const auto& n : m.nodes) {
      if (r.slaveNodes.count(n.id)) continue;
      auto d = dm.node_dofs(n.id);
      for (int i = 0; i < 6; ++i) { r.fullToReduced_[d[i]] = retained.size(); retained.push_back(d[i]); }
    }
    std::vector<Eigen::Triplet<Real>> trips;
    trips.reserve(retained.size() + slaveToMaster.size() * 36);
    for (std::size_t i = 0; i < retained.size(); ++i) trips.emplace_back(static_cast<int>(retained[i]), static_cast<int>(i), Real(1));
    for (const auto& [slaveId, masterId] : slaveToMaster) {
      const Node* sn = detail::find_by_id(nodes, slaveId);
      const Node* mn = detail::find_by_id(nodes, masterId);
      if (!sn || !mn) throw AssemblyError(AssemblyErrorCode::MissingNode, "rigid link references missing node");
      Eigen::Vector3d e(sn->x - mn->x, sn->y - mn->y, sn->z - mn->z);
      Eigen::Matrix<Real,6,6> G = Eigen::Matrix<Real,6,6>::Identity();
      G.block<3,3>(0,3) = -detail::rigidLinkSkew(e);
      auto sd = dm.node_dofs(slaveId), md = dm.node_dofs(masterId);
      for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
          if (G(i, j) != 0) trips.emplace_back(static_cast<int>(sd[i]), static_cast<int>(r.fullToReduced_.at(md[j])), G(i, j));
    }
    r.T.resize(static_cast<int>(dm.dof_count()), static_cast<int>(retained.size()));
    r.T.setFromTriplets(trips.begin(), trips.end());
    r.T.makeCompressed();
    return r;
  }

  Eigen::SparseMatrix<Real> reduceMatrix(const Eigen::SparseMatrix<Real>& K) const { return (T.transpose() * K * T).eval(); }
  Eigen::VectorXd reduceVector(const Eigen::VectorXd& F) const { return T.transpose() * F; }
  Eigen::VectorXd expand(const Eigen::VectorXd& Ureduced) const { return T * Ureduced; }
  std::size_t fullToReduced(std::size_t fullIdx) const {
    auto it = fullToReduced_.find(fullIdx);
    if (it == fullToReduced_.end()) throw AssemblyError(AssemblyErrorCode::MissingNode, "degree of freedom belongs to a rigid-link slave and cannot be restrained directly");
    return it->second;
  }

 private:
  std::map<std::size_t, std::size_t> fullToReduced_;
};

} // namespace kopruq
