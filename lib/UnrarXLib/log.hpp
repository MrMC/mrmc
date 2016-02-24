#ifndef _RAR_LOG_
#define _RAR_LOG_

void InitLogOptions(char *LogName);

#ifndef RAR_SILENT
void RarLog(const char *ArcName,const char *Format,...);
#endif

#ifdef RAR_SILENT
#ifdef __GNUC__
#define RarLog(args...)
#else
inline void RarLog(const char *a,const char *b,const char *c=NULL,const char *d=NULL) {}
#endif
#endif

#endif
