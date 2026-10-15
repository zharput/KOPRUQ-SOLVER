#pragma once
#include "../core/Element.hpp"
namespace kopruq { class Truss3D final : public Element { public: explicit Truss3D(Id value): value_(value) {} Id id() const override { return value_; } private: Id value_; }; }
