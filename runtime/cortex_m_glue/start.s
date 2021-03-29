.thumb
.thumb_func
.global _start
_start:
    @mov r0,=0x10000
    @mov sp,r0
    bl cortexm_entry
    mov r7,#0x1
    mov r0,#0
    swi #0
.word 0xFFFFFFFF
    b .

.thumb_func
.globl PUT32
PUT32:
    str r1,[r0]
    bx lr

.thumb_func
.globl GET32
GET32:
    ldr r0,[r0]
    bx lr

.thumb_func
.globl dummy
dummy:
    bx lr

.thumb_func
.globl write
write:
    push {r7,lr}
    mov r7,#0x04
    swi 0
    pop {r7,pc}
    b .

.end
