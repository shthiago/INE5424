#include <architecture/armv7/armv7_cpu.h>
#include <utility/ostream.h>

__BEGIN_SYS
void CPU::syscalled() {
    CPU::Reg r = CPU::r0();
    db<CPU>(WRN) << "CPU::r0 = " << *reinterpret_cast<int*>(r) << endl;
    
}
__END_SYS