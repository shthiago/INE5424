#ifndef __thread_stub
#define __thread_stub

#include <system.h>
#include <syscalls/message.h>

__BEGIN_API
__USING_SYS

class ThreadAPI {
public:
    ThreadAPI(int (* entry)(Tn ...), Tn ... an) { 
        Message * msg = new SyscallMessage(int (* entry)(Tn ...), Tn ... an);
        msg->type(CREATE_THREAD_1);

        CPU::syscall((void*) msg);
        _real_id = reinterpret_cast<unsigned long>(CPU::r0());
    }

private:
    unsigned long _real_id;
}

__END_API

#endif // !__thread_stub