#include <syscalls/handler.h>
#include <architecture/armv7/armv7_cpu.h>
#include <process.h>
__BEGIN_SYS
void CPU::syscalled() {
    SyscallHandler handler = SyscallHandler(reinterpret_cast<SyscallMessage*>(CPU::r0()));
    ASM("push {r1-r12}  \n");
    void* ret = handler.act();
    CPU::r0(reinterpret_cast<Reg>(ret));
    ASM("pop {r1-r12}  \n");
}
__END_SYS