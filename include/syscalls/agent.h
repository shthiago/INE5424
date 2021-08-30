#ifndef __agent
#define __agent

#include <utility/list.h>
#include <architecture/cpu.h>
#include "message.h"

__BEGIN_SYS

class Agent {
public:
    Agent(SyscallMessage * msg): _msg(msg) {}

    template<typename ... Tn>
    unsigned long create_thread_1(int (* entry)(Tn ...), Tn ... an) {
        Thread * t = new Thread(entry, an...);
        return reinterpret_cast<unsigned long>(t);
    }

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
    // static void exit(int r) { }
    // static volatile bool wait_next() {  }

    void print() {
        kout << msg()->text();
    }

    SyscallMessage * msg() { return _msg; }

private:
    SyscallMessage * _msg;
};

__END_SYS

#endif // !__agent