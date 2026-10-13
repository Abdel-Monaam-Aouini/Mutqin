#include "mutqin.h"

#ifndef MUTQIN_VERSION_STRING
#error "MUTQIN_VERSION_STRING must be defined by the build system"
#endif

extern "C" const char* mq_version(void) { return MUTQIN_VERSION_STRING; }
