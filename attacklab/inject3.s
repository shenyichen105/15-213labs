sub $0x30,%rsp #move stack pointer down 0x30
mov $0x5561dc97, %rdi # move pointer for char[] of cookie number to %rdi
ret # transfer control