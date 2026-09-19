// Always-on test assertion (unaffected by NDEBUG).
#pragma once
#include <stdio.h>
#include <stdlib.h>
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); exit(1); } } while (0)
