/*
 * GRR hv stage stack setup code
 */

.section .bss

/* 8K stack for the hv code in .bss */
.align 4096
grr_hv_stack_base:
.skip 8192
grr_hv_stack_top:

.section .text

.global grr_hv_entry
grr_hv_entry:

/* Setup stack */
movq $grr_hv_stack_top, %rsp

/* Call C code */
call grr_hv_main

/* If we get here something went wrong */
cli
1: hlt
jmp 1b
