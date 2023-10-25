/*
 对于BootLoader来讲, 他不知道什么是kernel, 他只按照设定位置开始运行程序, 所以我们需要将kernel程序写入到指定的位置 0x1badb002 没有原因, 太爷爷们的规定
 注: 开头不会被翻译成机器指令, 而是给汇编器一种特殊知识, 称之为汇编指示,或者伪操作
*/
.set MAGIC, 0x1badb002  /*GRUB魔术块*/
.set FLAGS, (1<<0 | 1<<1) /*GRUB标志块*/
.set CHECKSUM, -(MAGIC + FLAGS)  /*校验块*/

/*
 Boot程序按照Mutileboot 规范来编译内核，才可以被GRUB引导 
 setting our magic number
*/
.section .multiboot
    .long MAGIC
    .long FLAGS
    .long CHECKSUM


.section .text

.extern kernelMain
.extern callConstructors

.global loader
loader:
    mov $kernel_stack, %esp
    call callConstructors
    push %eax
    push %ebx
    call kernelMain

_stop:
    cli
    hlt
    jmp _stop


.section .bss
.space 2*1024*1024 ; # 2 MiB
kernel_stack:


