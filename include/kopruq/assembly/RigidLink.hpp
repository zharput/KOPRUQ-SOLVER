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
// Per-DOF links (2026-11-21, MIDAS "DOF of Rigid Link"): only the slave
// DOFs flagged in RigidLink::dofs are tied (rows of G); an untied slave DOF
// is kept as an independent reduced DOF exactly like any non-slave DOF. A
// TIED slave DOF cannot carry its own restraint (its motion is already
// determined by the master, so a restraint would be an independent and
// generally inconsistent extra constraint): checkRestraints() rejects it.
// An untied slave DOF may be restrained. Callers classify free/fixed DOFs
// with classifyRetained(), which skips the eliminated (tied) DOFs.
//
// Node local axes (2026-11-21, Model::node_axes): the same layer also
// rotates a node's retained DOFs into its local axes, U_global = R^T U_local
// per 3-block (R rows = local axes), so restraint flags of such a node act
// along its local axes. A rigid-link slave may not have local axes; a
// master may (its slaves' rows are composed through the master's rotation).
#include "DofManager.hpp"
#include "MatrixAssembler.hpp"
#include <Eigen/Sparse>
#include <array>
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
  std::set<Id> slaveNodes;                  // nodes with at least one tied DOF
  std::map<Id, std::array<bool,6>> tied;    // slave node -> tied DOF mask

  std::size_t reduced_count() const { return static_cast<std::size_t>(T.cols()); }

  static RigidLinkReduction build(const Model& m, const DofManager& dm) {
    RigidLinkReduction r;
    std::map<Id, Id> slaveToMaster;
    std::set<Id> seenIds;
    for (const auto& l : m.rigid_links) {
      if (!seenIds.insert(l.id).second) throw AssemblyError(AssemblyErrorCode::DuplicateNode, "duplicate rigid link id");
      if (l.master == l.slave) throw AssemblyError(AssemblyErrorCode::MissingNode, "rigid link master and slave must be different nodes");
      if (slaveToMaster.count(l.slave)) throw AssemblyError(AssemblyErrorCode::MissingNode, "node " + std::to_string(l.slave) + " is a slave of more than one rigid link");
      bool any = false; for (bool f : l.dofs) any = any || f;
      if (!any) continue; // a link that ties no DOF imposes nothing
      slaveToMaster[l.slave] = l.master;
      r.tied[l.slave] = l.dofs;
    }
    for (const auto& [slave, master] : slaveToMaster)
      if (slaveToMaster.count(master)) throw AssemblyError(AssemblyErrorCode::MissingNode, "rigid link chaining (a master that is itself a slave) is not supported");
    for (const auto& [slave, _] : slaveToMaster) r.slaveNodes.insert(slave);

    auto nodes = detail::index_by_id(m.nodes);
    for (const auto& [id, R] : m.node_axes) {
      if (!detail::find_by_id(nodes, id)) throw AssemblyError(AssemblyErrorCode::MissingNode, "node local axes reference missing node " + std::to_string(id));
      if (!R.allFinite() || (R * R.transpose() - Eigen::Matrix3d::Identity()).cwiseAbs().maxCoeff() > 1e-9 || R.determinant() < 0)
        throw AssemblyError(AssemblyErrorCode::MissingNode, "node " + std::to_string(id) + ": local axes must be a right-handed orthonormal rotation");
      if (r.slaveNodes.count(id)) throw AssemblyError(AssemblyErrorCode::MissingNode, "rigid-link slave node " + std::to_string(id) + " cannot have node local axes");
    }
    // Q(node): 6x6 map from the node's reduced (possibly local) DOFs to its global DOFs.
    auto nodeQ = [&](Id id) {
      Eigen::Matrix<Real,6,6> Q = Eigen::Matrix<Real,6,6>::Identity();
      auto a = m.node_axes.find(id);
      if (a != m.node_axes.end()) { Q.block<3,3>(0,0) = a->second.transpose(); Q.block<3,3>(3,3) = a->second.transpose(); }
      return Q;
    };
    std::vector<std::size_t> retained;
    r.fullToReduced_.clear();
    for (const auto& n : m.nodes) {
      auto t = r.tied.find(n.id);
      auto d = dm.node_dofs(n.id);
      for (int i = 0; i < 6; ++i) {
        if (t != r.tied.end() && t->second[(std::size_t)i]) continue;
        r.fullToReduced_[d[i]] = retained.size(); retained.push_back(d[i]);
      }
    }
    std::vector<Eigen::Triplet<Real>> trips;
    trips.reserve(retained.size() + slaveToMaster.size() * 36 + m.node_axes.size() * 18);
    for (const auto& n : m.nodes) {
      auto d = dm.node_dofs(n.id);
      if (!m.node_axes.count(n.id)) {
        for (int i = 0; i < 6; ++i) { auto f = r.fullToReduced_.find(d[i]); if (f != r.fullToReduced_.end()) trips.emplace_back(static_cast<int>(d[i]), static_cast<int>(f->second), Real(1)); }
        continue;
      }
      const auto Q = nodeQ(n.id);
      for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
          if (Q(i, j) != 0) trips.emplace_back(static_cast<int>(d[i]), static_cast<int>(r.fullToReduced_.at(d[j])), Q(i, j));
    }
    for (const auto& [slaveId, masterId] : slaveToMaster) {
      const Node* sn = detail::find_by_id(nodes, slaveId);
      const Node* mn = detail::find_by_id(nodes, masterId);
      if (!sn || !mn) throw AssemblyError(AssemblyErrorCode::MissingNode, "rigid link references missing node");
      Eigen::Vector3d e(sn->x - mn->x, sn->y - mn->y, sn->z - mn->z);
      Eigen::Matrix<Real,6,6> G = Eigen::Matrix<Real,6,6>::Identity();
      G.block<3,3>(0,3) = -detail::rigidLinkSkew(e);
      const Eigen::Matrix<Real,6,6> GQ = (G * nodeQ(masterId)).eval(); // slave global <- master reduced
      auto sd = dm.node_dofs(slaveId), md = dm.node_dofs(masterId);
      const auto& mask = r.tied.at(slaveId);
      for (int i = 0; i < 6; ++i)
        if (mask[(std::size_t)i])
        for (int j = 0; j < 6; ++j)
          if (GQ(i, j) != 0) trips.emplace_back(static_cast<int>(sd[i]), static_cast<int>(r.fullToReduced_.at(md[j])), GQ(i, j));
    }
    r.T.resize(static_cast<int>(dm.dof_count()), static_cast<int>(retained.size()));
    r.T.setFromTriplets(trips.begin(), trips.end());
    r.T.makeCompressed();
    return r;
  }

  bool isTied(Id node, int dof) const { auto t = tied.find(node); return t != tied.end() && t->second[(std::size_t)dof]; }

  // Throws when a TIED slave DOF is restrained (see the header comment).
  void checkRestraints(const std::map<Id, std::array<bool,6>>& rs) const {
    for (const auto& [node, flags] : rs)
      for (int i = 0; i < 6; ++i)
        if (flags[(std::size_t)i] && isTied(node, i))
          throw AssemblyError(AssemblyErrorCode::MissingNode, "rigid-link slave node " + std::to_string(node) + " cannot carry its own restraints on a tied degree of freedom");
  }

  // Free/fixed FULL DOF indices of every retained (non-eliminated) DOF, in the same order as
  // MatrixAssembler::classify; `rs` must list every node (unrestrained nodes with all-false flags).
  BoundaryMap classifyRetained(const DofManager& dm, const std::map<Id, std::array<bool,6>>& rs) const {
    BoundaryMap out;
    for (const auto& [id, b] : rs) {
      auto d = dm.node_dofs(id);
      for (int i = 0; i < 6; ++i) { if (isTied(id, i)) continue; (b[(std::size_t)i] ? out.fixed : out.free).push_back(d[i]); }
    }
    return out;
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
