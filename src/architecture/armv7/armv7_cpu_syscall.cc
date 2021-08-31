#include <architecture/armv7/armv7_cpu.h>

__BEGIN_SYS
void CPU::syscall(void * message) {
    db<CPU>(WRN) << "Entrando em CPU::syscall" << endl;
    ASM("push {lr}\n");
    CPU::r0(reinterpret_cast<CPU::Reg>(message));
    ASM("svc 0x0");
    ASM("pop {lr}\n");
    db<CPU>(WRN) << "Saindo de CPU::syscall" << endl;
}
__END_SYS