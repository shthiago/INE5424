#ifndef __syscall_handler
#define __syscall_handler

#include <system.h>

#include "agent.h"
#include "message.h"

__BEGIN_SYS
class SyscallHandler {
public:
    SyscallHandler(SyscallMessage * msg): _msg(msg) { }

    void* act() {
        Agent agent = Agent(msg());
        void * result = 0x0;

        switch(msg()->type()) {
            case PRINT:
                agent.print();
                break;

            case THREAD_EXIT_ARG:
                agent.exit();
                break;
        }

        return result;
    }

    SyscallMessage * msg() { return _msg; }

private:
    SyscallMessage * _msg;
};

__END_SYS

#endif // !__syscall_handler