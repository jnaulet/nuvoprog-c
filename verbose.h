#ifndef VERBOSE_H
#define VERBOSE_H

extern int verbose_level;

#define LEVEL_ERR  0
#define LEVEL_INFO 1
#define LEVEL_DBG  2

#define VERBOSE(level, ...)                                     \
  do                                                            \
    if(level <= verbose_level)                                  \
      fprintf(stderr, __VA_ARGS__);                             \
  while(0)

#endif
