# Function data for stdio.h			-*- mode: conf-space -*-

: #include <stdio.h>

errno	EINTR,EIO,ENOSPC,EDQUOT,ENOSPC,EPIPE
fail	EOF
name	fclose
params	FILE *f
return	int

errno	ENOMEM
fail	NULL
name	fdopen
params	int fd, const char *mode
return	FILE *

errno	EINTR,EIO,ENOSPC
fail	EOF
name	fflush
params	FILE *f
return	int

errno	EINTR,EIO
fail	EOF
name	fgetc
params	FILE *f
return	int

errno	EBADF,EINTR,EIO,EOVERFLOW,ENOMEM,ENXIO
fail	NULL
name	fgets
params	char *b, int s, FILE * f
return	char*

errno	ENOMEM,EACCES
fail	NULL
name	fopen
params	const char *f, const char *m
return	FILE *

errno	EINTR,EIO,ENOSPC,ENOMEM
fail	EOF
name	fputc
params	int c, FILE *f
return	int

errno	EINTR,EIO,ENOSPC,ENOMEM
fail	EOF
name	putc
params	int c, FILE *f
return	int

errno	EINTR,EIO,ENOSPC,ENOMEM
fail	EOF
name	fputs
params	const char *s, FILE *f
return	int

errno	EINTR,EIO,ENOMEM
fail	0
name	fread
params	void *m, size_t s, size_t n, FILE *f
return	size_t

errno	EINTR,EIO,ENOSPC,ENOMEM
fail	0
name	fwrite
params	const void *m, size_t s, size_t n, FILE *f
return	size_t

errno	ENOMEM
fail	-1
name	getdelim
params	char **p, size_t *n, int d, FILE *f
return	ssize_t

errno	ENOMEM
fail	-1
name	getline
params	char **p, size_t *n, FILE *f
return	ssize_t

errno	EINTR,EIO,ENOSPC,ENOMEM
fail	-42
name	puts
params	const char *s
return	int

errno	EACCES,EBUSY,EFAULT,EINVAL,EIO,EISDIR,ELOOP,ENAMETOOLONG,ENOENT,ENOMEM,ENOTDIR,ENOTEMPTY,EPERM,EROFS
fail	-1
name	remove
params	const char *fn
return	int

errno	EACCES,EBUSY,EDQUOT,EFAULT,ELOOP,EMLINK,ENAMETOOLONG,ENOENT,ENOMEM,ENOSPC,ENOTEMPTY,EEXIST,EPERM,EACCES,EROFS
fail	-1
name	rename
params	const char *a, const char *b
return	int
