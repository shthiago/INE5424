#include <architecture/armv7/armv7_cpu.h>

__BEGIN_SYS
void CPU::syscall(void * message) {
    db<CPU>(WRN) << "Syscall called" << endl;

    CPU::r0(reinterpret_cast<Reg>(message));
    ASM("svc 0x0");
}
__END_SYS