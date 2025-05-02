#ifndef VERBOSE_H
#define VERBOSE_H

extern int verbose_level;

#define LOG_ERR  0
#define LOG_INFO 1
#define LOG_DBG  2

#define VERBOSE(level, ...)                                     \
  do                                                            \
    if(level <= verbose_level)                                  \
      fprintf(stderr, __VA_ARGS__);                             \
  while(0)

#endif
