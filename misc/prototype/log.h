#ifndef __LOG_H__
#define __LOG_H__

#ifdef ENABLE_LOG_DEBUG
#define LOG_DEBUG(x) printf x;
#else
#define LOG_DEBUG(x)
#endif

#ifdef ENABLE_LOG_INFO
#define LOG_INFO(x) printf x;
#else
#define LOG_INFO(x)
#endif

#ifdef ENABLE_LOG_WARNING
#define LOG_WARNING(x) { printf("\033[31m"); printf x; printf("\033[0m"); }
#else
#define LOG_WARNING(x)
#endif

#endif
