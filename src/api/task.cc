#include <process.h>

__BEGIN_SYS

// Declare _current
Task * volatile Task::_current;


// Methods
Task::~Task()
{
    db<Task>(TRC) << "~Task(this=" << this << ")" << endl;

    while(!_threads.empty())
        delete _threads.remove()->object();

    delete _as;
}

__END_SYS