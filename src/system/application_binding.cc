// EPOS Application Binding

#include <utility/spin.h>
#include <utility/ostream.h>
#include <architecture/cpu.h>
#include <system.h>
#include <stubs/api_thread.h>
#include <syscalls/message.h>


// Global objects
__BEGIN_SYS
OStream kerr;
__END_SYS


// Bindings
extern "C" {
    void _panic() { _API::APIThread::exit(-1); }
    void _exit(int s) { _API::APIThread::exit(s); for(;;); }

    // Utility methods that differ from kernel and user space.
    // Heap
    static _UTIL::Simple_Spin _heap_spin;
    void _lock_heap() { _heap_spin.acquire(); }
    void _unlock_heap() { _heap_spin.release();}
}

__USING_SYS;
extern "C" {
    void _syscall(void * m) { CPU::syscall(m); }
    void _print(const char * s) {
        SyscallMessage * msg = new SyscallMessage(s);
        CPU::syscall((void*) msg);
    }
    void _print_preamble() {}
    void _print_trailler(bool error) {}
}
