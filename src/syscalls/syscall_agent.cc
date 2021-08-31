#include <syscalls/agent.h>

#include <process.h>

__BEGIN_SYS

void Agent::exit() {
    Thread::exit(msg()->integer()); 
}

__END_SYS