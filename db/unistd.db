# Function data for unistd.h			-*- mode: conf-space -*-

: #include <unistd.h>

errno	EACCES,EBUSY,EFAULT,EIO,EISDIR,ELOOP,ENAMETOOLONG,ENOENT,ENOMEM,ENOTDIR,EPERM
fail	-1
name	unlink
params	const char *pathname
return	int

errno	EACCES,ELOOP,ENAMETOOLONG,ENOENT,ENOTDIR,EROFS,EFAULT,EIO,ENOMEM
fail	-1
name	access
params	const char *pn, int mo
return	int

errno	EACCES,EFAULT,EIO,ELOOP,ENAMETOOLONG,ENOENT,ENOMEM,ENOTDIR
fail	-1
name	chdir
params	const char *pn
return	int

errno	EACCES,EFAULT,EIO,ELOOP,ENAMETOOLONG,ENOENT,ENOMEM,ENOTDIR,EPERM
fail	-1
name	chown
params	const char *pn, uid_t o, gid_t g
return	int

errno	EINTR,EIO,ENOSPC,EDQUOT
fail	-1
name	close
params	int fd
return	int

errno	EBADF,EMFILE
fail	-1
name	dup
params	int fd
return	int

errno	EBADF,EMFILE,EIO
fail	-1
name	dup2
params	int fd1, int fd2
return	int

errno	EACCES,EAGAIN,EFAULT,EIO,EISDIR,ELIBBAD,ELOOP,EMFILE,ENOENT,ENOEXEC,EPERM
fail	-1
name	execv
params	const char *file, char *const *argv
return	int

errno	EACCES,EAGAIN,EFAULT,EIO,EISDIR,ELIBBAD,ELOOP,EMFILE,ENOENT,ENOEXEC,EPERM
fail	-1
name	execve
params	const char *file, char *const *argv, char *const *envp
return	int

errno	EACCES,EAGAIN,EFAULT,EIO,EISDIR,ELIBBAD,ELOOP,EMFILE,ENOENT,ENOEXEC,EPERM
fail	-1
name	execvp
params	const char *file, char *const *argv
return	int

errno	EAGAIN,ENOMEM
fail	-1
name	fork
return	pid_t

errno	EACCES,ENOENT,ENOMEM
fail	NULL
name	getcwd
params	char *buf, size_t size
return	char*

errno	EPERM
fail	-1
name	gethostname
params	char *name, size_t len
return	int

errno	EACCES,EDQUOT,EEXIST,EFAULT,EIO,ELOOP,EMLINK,ENAMETOOLONG,ENOENT,ENOMEM,ENOTDIR,EPERM,EROFS
fail	-1
name	link
params	const char *old, const char *new
return	int

errno	EAGAIN,EFAULT,EINTR,EIO,EISDIR
fail	-1
name	read
params	int fd, void *m, size_t n
return	ssize_t

errno	EACCES,EINVAL,EIO,ELOOP,ENAMETOOLONG,ENOENT,ENOTDIR
fail	-1
name	readlink
params	const char *path, char *buf, size_t size
return	ssize_t


errno	EACCES,EBUSY,EFAULT,EINVAL,ELOOP,ENAMETOOLONG,ENOENT,ENOMEM,ENOTDIR,ENOTEMPTY,EPERM,EPERM,EROFS
fail	-1
name	rmdir
params	const char *pathname
return	int

errno	EACCES,EDQUOT,EEXIST,EINVAL,ELOOP,ENAMETOOLONG
fail	-1
name	mkdir
params	const char *pathname, mode_t mode
return	int

errno	EAGAIN,EDQUOT,EFAULT,EINTR,EIO,ENOSPC
fail	-1
name	write
params	int fd, const void *m, size_t n
return	ssize_t
