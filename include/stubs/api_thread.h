#ifndef api_thread
#define api_thread

#include <system.h>
#include <syscalls/message.h>

__USING_SYS
__BEGIN_API

class APIThread {
public:
    APIThread() {}

    static void exit(int r = 0) {
        SyscallMessage * msg = new SyscallMessage(r);
        msg->type(THREAD_EXIT_ARG);

        CPU::syscall((void*)msg);
    }
};

__END_API

#endif // !api_thread