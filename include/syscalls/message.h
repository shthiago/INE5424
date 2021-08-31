#ifndef __message_syscall
#define __message_syscall

#include <system.h>

__BEGIN_SYS

enum SyscallType {
    THREAD_EXIT_ARG,

    PRINT,
};

class SyscallMessage {
public:
    SyscallMessage(const char * text): _text(text) { }
    SyscallMessage(int integer): _integer(integer) { }


    SyscallType type() { return _type; }
    void type(SyscallType type) { _type = type; }

    const char * text() {return _text; }

    int integer() { return _integer; }


private:
    SyscallType _type;
    const char * _text;
    int _integer;
};

__END_SYS

#endif // __message_syscall