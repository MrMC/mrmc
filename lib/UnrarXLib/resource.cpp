#include "rar.hpp"



#if !defined(RAR_SILENT) || !defined(RARDLL)
const char *St(MSGID StringId)
{
  return(StringId);
}
#endif
