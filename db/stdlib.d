# Function data for stdlib.h			-*- mode: conf-space -*-

: #include <stdlib.h>

errno	ENOMEM
fail	NULL
name	malloc
params	size_t size
return	void*

errno	ENOMEM
fail	NULL
name	calloc
params	size_t nmemb, size_t size
return	void*

errno	ENOMEM
fail	NULL
name	realloc
params	void *ptr, size_t size
return	void*

