/* Minimal ARM EABI entry; C bridge handles the libc startup call ABI. */
.syntax unified
.arm
.text
.global _start
.type _start,%function
_start:
    mov r1, r0
    mov r0, sp
    bic sp, sp, #7
    bl lx06_libc_start
1:  b 1b
.size _start, .-_start
.section .note.GNU-stack,"",%progbits
