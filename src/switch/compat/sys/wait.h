// gow_nx: sustituto de <sys/wait.h> para Switch (no hay procesos hijos).
#ifndef GOWNX_SYS_WAIT_H
#define GOWNX_SYS_WAIT_H
#include <sys/types.h>
#define WNOHANG 1
#define WIFEXITED(s) 1
#define WEXITSTATUS(s) (s)
#define WIFSIGNALED(s) 0
#define WTERMSIG(s) 0
#ifdef __cplusplus
extern "C" {
#endif
pid_t waitpid(pid_t pid, int *status, int options);
#ifdef __cplusplus
}
#endif
#endif
