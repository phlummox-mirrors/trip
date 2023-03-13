#include "../macs.h"
#include <stdlib.h>
DEF(void*, malloc, (size_t size), ( size), (NULL), ENOMEM)
DEF(void*, calloc, (size_t nmemb, size_t size), ( nmemb, size), (NULL), ENOMEM)
DEF(void*, realloc, (void *ptr, size_t size), ( ptr, size), (NULL), ENOMEM)
