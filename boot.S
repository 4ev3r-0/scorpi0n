.global _start
.global main

.section .text

_start:
    movi a1, _stack_top
    call0 main

1:
    j 1b
