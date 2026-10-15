#pragma once
#include "../core/Element.hpp"
namespace kopruq { class Spring final : public Element { public: explicit Spring(Id value): value_(value) {} Id id() const override { return value_; } private: Id value_; }; }
