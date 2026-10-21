#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); std::exit(1); } } while(false)
#define NEAR(a,b) CHECK(std::abs((a)-(b))<1e-9)
