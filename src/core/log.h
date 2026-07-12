#ifndef KATHTPP_LOG_H
#define KATHTPP_LOG_H

#include <cstdio>

#ifndef KATHTPP_LOG_LEVEL
#define KATHTPP_LOG_LEVEL 1
#endif

#define KATHTPP_LOG_ERR(...)   \
  do { if (KATHTPP_LOG_LEVEL >= 1) fprintf(stderr, "kathttp[E] " __VA_ARGS__); } while (0)
#define KATHTPP_LOG_WARN(...)  \
  do { if (KATHTPP_LOG_LEVEL >= 2) fprintf(stderr, "kathttp[W] " __VA_ARGS__); } while (0)
#define KATHTPP_LOG_INFO(...)  \
  do { if (KATHTPP_LOG_LEVEL >= 3) fprintf(stderr, "kathttp[I] " __VA_ARGS__); } while (0)
#define KATHTPP_LOG_DEBUG(...) \
  do { if (KATHTPP_LOG_LEVEL >= 4) fprintf(stderr, "kathttp[D] " __VA_ARGS__); } while (0)

#endif /* KATHTPP_LOG_H */
