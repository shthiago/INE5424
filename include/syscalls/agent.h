#ifndef __syscall_agent
#define __syscall_agent

#include <system.h>
#include <process.h>
#include "message.h"

__BEGIN_SYS

class Agent {
public:
    Agent(SyscallMessage * msg): _msg(msg) {}
    // template<typename ... Tn>
    // void * create_thread(const Configuration & conf, int (* entry)(Tn ...), Tn ... an);

    // int state() {  }
    // int priority() {  }
    // void priority(int p) { }
    // int join() {  }
    // int pass() {  }
    // void suspend() {  }
    // void resume() {  }
    // static int yield() {  }
    void exit() { 
        // _SYS::Thread::exit(msg()->integer());
    };
    // static volatile bool wait_next() {  }

    void print() {
        kout << msg()->text();
    }

    SyscallMessage * msg() { return _msg; }

private:
    SyscallMessage * _msg;
};

__END_SYS

#endif // !__syscall_agent