#ifndef __TT_PLATFORM_H__
#define __TT_PLATFORM_H__

#ifdef _WIN32
#define SIZET_FMT "I64u"
#define TIMET_FMT "I64d"
#define I64D_FMT "I64d"
#define I64U_FMT "I64u"
#else
#define SIZET_FMT "lu"
#define TIMET_FMT "ld"
#define I64D_FMT "lld"
#define I64U_FMT "llu"
#endif

#endif

