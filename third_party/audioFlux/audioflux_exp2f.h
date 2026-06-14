#pragma once

#include <math.h>

#undef exp2f
#define exp2f(x) powf(2.f, (x))
