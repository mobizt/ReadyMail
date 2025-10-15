// Vector.h
#pragma once

#include "VectorConfig.h"
#include "StaticVector.h"
#include "DynamicVector.h"

#ifdef USE_STATIC_VECTOR
template<typename T, size_t Capacity>
using Vector = StaticVector<T, Capacity>;
#else
template<typename T>
using Vector = DynamicVector<T>;
#endif
