#ifndef __syscall_handler
#define __syscall_handler

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

            case CREATE_THREAD_1:
                // deserializar os argumetnos e jogar pra dentro (???)
                void * func_ret = agent.create_thread_1();
                ret = reinterpret_cast<void*>(func_ret);
                break;

            case THREAD_EXIT_NO_ARG:
            case THREAD_EXIT_ARG:
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