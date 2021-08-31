#include <utility/ostream.h>
#include <syscalls/message.h>

__USING_SYS

OStream cout;

int main()
{   
    const char * hello = "Hello, syscall!\n";
    SyscallMessage * msg = new SyscallMessage(hello);
    msg->type(PRINT);
    CPU::syscall((void*) msg);

    return 0;
}
