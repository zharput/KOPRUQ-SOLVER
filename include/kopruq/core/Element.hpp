#pragma once
#include "Types.hpp"
namespace kopruq { struct Element { virtual ~Element()=default; virtual Id id() const=0; }; }
