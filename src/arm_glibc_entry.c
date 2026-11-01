#include <stdint.h>
#include <stddef.h>
extern int main(int,char **);
extern int __libc_start_main(int (*)(int,char **),int,char **,
                            void (*)(void),void (*)(void),void (*)(void),void *);
extern void _exit(int) __attribute__((noreturn));
/* No application constructors are used in these small probes. */
void lx06_libc_start(uintptr_t *initial_stack,void (*loader_fini)(void)) {
    __libc_start_main(main,(int)initial_stack[0],(char **)(initial_stack+1),
                     NULL,NULL,loader_fini,initial_stack);
    _exit(127);
}
