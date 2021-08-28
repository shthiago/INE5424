// EPOS Thread Component Declarations

#ifndef __process_h
#define __process_h

#include <architecture.h>
#include <machine.h>
#include <utility/queue.h>
#include <utility/handler.h>
#include <memory.h>
#include <scheduler.h>

extern "C" {
    void __exit();
    void _lock_heap();
    void _unlock_heap();
}

__BEGIN_SYS

class Thread
{
    friend class Init_End;              // context->load()
    friend class Init_System;           // for init() on CPU != 0
    friend class Scheduler<Thread>;     // for link()
    friend class Synchronizer_Common;   // for lock() and sleep()
    friend class Alarm;                 // for lock()
    friend class System;                // for init()
    friend class IC;                    // for link() for priority ceiling
    friend class Clerk<System>;         // for _statistics
    friend void ::_lock_heap();         // for lock()
    friend void ::_unlock_heap();       // for unlock()

protected:
    static const bool smp = Traits<Thread>::smp;
    static const bool monitored = Traits<Thread>::monitored;
    static const bool preemptive = Traits<Thread>::Criterion::preemptive;
    static const bool multitask = Traits<System>::multitask;
    static const bool reboot = Traits<System>::reboot;

    static const unsigned int QUANTUM = Traits<Thread>::QUANTUM;
    static const unsigned int STACK_SIZE = multitask ? Traits<System>::STACK_SIZE : Traits<Application>::STACK_SIZE;
    static const unsigned int USER_STACK_SIZE = Traits<Application>::STACK_SIZE;

    typedef CPU::Log_Addr Log_Addr;
    typedef CPU::Context Context;

public:
    // Thread State
    enum State {
        RUNNING,
        READY,
        SUSPENDED,
        WAITING,
        FINISHING
    };

    // Thread Scheduling Criterion
    typedef Traits<Thread>::Criterion Criterion;
    enum {
        HIGH    = Criterion::HIGH,
        NORMAL  = Criterion::NORMAL,
        LOW     = Criterion::LOW,
        MAIN    = Criterion::MAIN,
        IDLE    = Criterion::IDLE
    };

    // Thread Queue
    typedef Ordered_Queue<Thread, Criterion, Scheduler<Thread>::Element> Queue;

    // Thread Configuration
    // t = 0 => Task::self()
    // ss = 0 => user-level stack on an auto expand segment
    struct Configuration {
        Configuration(const State & s = READY, const Criterion & c = NORMAL, Color a = WHITE, Task * t = 0, unsigned int ss = STACK_SIZE)
        : state(s), criterion(c), color(a), task(t), stack_size(ss) {}

        State state;
        Criterion criterion;
        Color color;
        Task * task;
        unsigned int stack_size;
    };


public:
    template<typename ... Tn>
    Thread(int (* entry)(Tn ...), Tn ... an);
    template<typename ... Tn>
    Thread(const Configuration & conf, int (* entry)(Tn ...), Tn ... an);
    ~Thread();

    const volatile State & state() const { return _state; }
    const volatile Criterion::Statistics & statistics() { return criterion().statistics(); }

    const volatile Criterion & priority() const { return _link.rank(); }
    void priority(const Criterion & p);

    Task * task() const { return _task; }

    int join();
    void pass();
    void suspend();
    void resume();

    static Thread * volatile self() { return running(); }
    static void yield();
    static void exit(int status = 0);

protected:
    void constructor_prologue(Color color, unsigned int stack_size);
    void constructor_epilogue(Log_Addr entry, unsigned int stack_size);

    Criterion & criterion() { return const_cast<Criterion &>(_link.rank()); }
    Queue::Element * link() { return &_link; }

    Criterion begin_isr(IC::Interrupt_Id i) {
        assert(_state == RUNNING);
        Criterion c = criterion();
        _link.rank(int(i));
        return c;
    }
    void end_isr(IC::Interrupt_Id i, const Criterion & c) {
        assert(_state == RUNNING);
        _link.rank(c);
    }

    static Thread * volatile running() { return _scheduler.chosen(); }

    static void lock(Spin * lock = &_lock) {
        CPU::int_disable();
        if(smp)
            lock->acquire();
    }

    static void unlock(Spin * lock = &_lock) {
        if(smp)
            lock->release();
        CPU::int_enable();
    }

    static volatile bool locked() { return (smp) ? _lock.taken() : CPU::int_disabled(); }

    static void sleep(Queue * q);
    static void wakeup(Queue * q);
    static void wakeup_all(Queue * q);

    static void reschedule();
    static void reschedule(unsigned int cpu);
    static void rescheduler(IC::Interrupt_Id interrupt);
    static void time_slicer(IC::Interrupt_Id interrupt);

    static void dispatch(Thread * prev, Thread * next, bool charge = true);

    static void for_all_threads(bool (* function)(Criterion &), const Thread * exclude = 0) {
        bool ret = true;
        for(Queue::Iterator i = _scheduler.begin(); ret && (i != _scheduler.end()); ++i)
            if((i->object() != exclude) && (!Criterion::track_idle && (i->object()->priority() != IDLE)))
                ret = function(i->object()->criterion());
    }
    static void for_all_threads_in_task(const Task * task, bool (* function)(Criterion &), const Thread * exclude = 0);
    static void for_all_threads_in_cpu(unsigned int cpu, bool (* function)(Criterion &), const Thread * exclude = 0);

    static bool collector(Criterion & c) { return c.collect(); }
    static bool charger(Criterion & c) { return c.charge(); }
    static bool awarder(Criterion & c) { return c.award(); }

    static int idle();

private:
    static void init();

protected:
    Task * _task;
    Segment * _user_stack;

    char * _stack;
    Context * volatile _context;
    volatile State _state;
    Queue * _waiting;
    Thread * volatile _joining;
    Queue::Element _link;

    static volatile unsigned int _thread_count;
    static Scheduler_Timer * _timer;
    static Scheduler<Thread> _scheduler;
    static Spin _lock;
};


// Task (only used in multitasking configurations)
class Task
{
    friend class Thread;        // for insert()

private:
    static const bool multitask = Traits<System>::multitask;

    typedef CPU::Log_Addr Log_Addr;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Context Context;
    typedef Thread::Queue Queue;

protected:
    // This constructor is only used by Thread::init()
    template<typename ... Tn>
    Task(Address_Space * as, Segment * cs, Segment * ds, int (* entry)(Tn ...), const Log_Addr & code, const Log_Addr & data, Tn ... an)
    : _as(as), _cs(cs), _ds(ds), _entry(entry), _code(code), _data(data), _id(1) {
        db<Task, Init>(TRC) << "Task(as=" << _as << ",cs=" << _cs << ",ds=" << _ds << ",entry=" << _entry << ",code=" << _code << ",data=" << _data << ") => " << this << endl;

        _current = this;
        activate();
        _main = new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN, WHITE, this, 0), entry, an ...);
    }

public:
    template<typename ... Tn>
    Task(Segment * cs, Segment * ds, int (* entry)(Tn ...), Tn ... an)
    : _as (new (SYSTEM) Address_Space), _cs(cs), _ds(ds), _entry(entry), _code(_as->attach(_cs)), _data(_as->attach(_ds)), _id(0) {
        db<Task>(TRC) << "Task(as=" << _as << ",cs=" << _cs << ",ds=" << _ds << ",entry=" << _entry << ",code=" << _code << ",data=" << _data << ") => " << this << endl;

        _main = new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::MAIN, WHITE, this, 0), entry, an ...);
    }
    template<typename ... Tn>
    Task(const Thread::Configuration & conf, Segment * cs, Segment * ds, int (* entry)(Tn ...), Tn ... an)
    : _as (new (SYSTEM) Address_Space), _cs(cs), _ds(ds), _entry(entry), _code(_as->attach(_cs)), _data(_as->attach(_ds)), _id(0) {
        db<Task>(TRC) << "Task(as=" << _as << ",cs=" << _cs << ",ds=" << _ds << ",entry=" << _entry << ",code=" << _code << ",data=" << _data << ") => " << this << endl;

        _main = new (SYSTEM) Thread(Thread::Configuration(conf.state, conf.criterion, WHITE, this, 0), entry, an ...);
    }
    ~Task();

    Address_Space * address_space() const { return _as; }

    Segment * code_segment() const { return _cs; }
    Segment * data_segment() const { return _ds; }

    Log_Addr code() const { return _code; }
    Log_Addr data() const { return _data; }

    Thread * main() const { return _main; }

    static Task * volatile self() { return current(); }

    int id() { return _id; };
    void id(int id) { _id = id; };

    static void fork(int (* entry)()) {
        Task * current = Task::self();
        Segment * current_code_seg = current->code_segment();
        Segment * current_data_seg = current->data_segment();

        Segment * code_seg_copy = new (SYSTEM) Segment(current_code_seg->size(), Segment::Flags::SYS);
        Segment * data_seg_copy = new (SYSTEM) Segment(current_data_seg->size(), Segment::Flags::SYS);

        // Attach the new segments to the current ad to make the copy
        // Detach after
        Address_Space * current_address_space = current->address_space();

        CPU::Log_Addr code_seg_addr = current_address_space->attach(code_seg_copy);
        memcpy(code_seg_addr, current->code(), current_code_seg->size());
        current_address_space->detach(code_seg_copy);

        CPU::Log_Addr data_seg_addr = current_address_space->attach(data_seg_copy);
        memcpy(data_seg_addr, current->data(), current_data_seg->size());
        current_address_space->detach(data_seg_copy);

        new (SYSTEM) Task(Thread::Configuration(Thread::READY, Thread::NORMAL), code_seg_copy, data_seg_copy, entry);
    }

private:
    void activate() const { _as->activate(); }

    void insert(Thread * t) { _threads.insert(new (SYSTEM) Queue::Element(t)); }
    void remove(Thread * t) { Queue::Element * el = _threads.remove(t); if(el) delete el; }

    static Task * volatile current() { return _current; }
    static void current(Task * t) { _current = t; }

    static void init();

private:
    Address_Space * _as;
    Segment * _cs;
    Segment * _ds;
    Log_Addr _entry;
    Log_Addr _code;
    Log_Addr _data;
    Thread * _main;
    Queue _threads;
    int _id;

    static Task * volatile _current;
};


// A Java-like Active Object
class Active: public Thread
{
public:
    Active(): Thread(Configuration(Thread::SUSPENDED), &entry, this) {}
    virtual ~Active() {}

    virtual int run() = 0;

    void start() { resume(); }

private:
    static int entry(Active * runnable) { return runnable->run(); }
};


// An event handler that triggers a thread (see handler.h)
class Thread_Handler : public Handler
{
public:
    Thread_Handler(Thread * h) : _handler(h) {}
    ~Thread_Handler() {}

    void operator()() { _handler->resume(); }

private:
    Thread * _handler;
};


// Thread inline methods that depend on Task
template<typename ... Tn>
inline Thread::Thread(int (* entry)(Tn ...), Tn ... an)
: _task(Task::self()), _user_stack(0), _state(READY), _waiting(0), _joining(0), _link(this, NORMAL)
{
    constructor_prologue(WHITE, STACK_SIZE);
    _context = CPU::init_stack(0, _stack + STACK_SIZE, &__exit, entry, an ...);
    constructor_epilogue(entry, STACK_SIZE);
}

template<typename ... Tn>
inline Thread::Thread(const Configuration & conf, int (* entry)(Tn ...), Tn ... an)
: _task(conf.task ? conf.task : Task::self()), _state(conf.state), _waiting(0), _joining(0), _link(this, conf.criterion)
{
    if(multitask && !conf.stack_size) { // auto-expand, user-level stack
        constructor_prologue(WHITE, STACK_SIZE);
        _user_stack = new (SYSTEM) Segment(USER_STACK_SIZE);

        // Attach the thread's user-level stack to the current address space so we can initialize it
        Log_Addr ustack = Task::self()->address_space()->attach(_user_stack);

        // Initialize the thread's user-level stack and determine a relative stack pointer (usp) from the top of the stack
        Log_Addr usp = ustack + USER_STACK_SIZE;
        if(conf.criterion == MAIN)
            usp -= CPU::init_user_stack(usp, 0, an ...); // the main thread of each task must return to crt0 to call _fini (global destructors) before calling __exit
        else
            usp -= CPU::init_user_stack(usp, &__exit, an ...); // __exit will cause a Page Fault that must be properly handled

        // Detach the thread's user-level stack from the current address space
        Task::self()->address_space()->detach(_user_stack, ustack);

        // Attach the thread's user-level stack to its task's address space so it will be able to access it when it runs
        ustack = _task->address_space()->attach(_user_stack);

        // Determine an absolute stack pointer (usp) from the top of the thread's user-level stack considering the address it will see it when it runs
        usp = ustack + USER_STACK_SIZE - usp;

        // Initialize the thread's system-level stack
        _context = CPU::init_stack(usp, _stack + STACK_SIZE, &__exit, entry, an ...);
    } else { // single-task scenarios and idle thread, which is a kernel thread, don't have a user-level stack
        constructor_prologue(conf.color, conf.stack_size);
        _user_stack = 0;
        _context = CPU::init_stack(0, _stack + conf.stack_size, &__exit, entry, an ...);
    }

    constructor_epilogue(entry, STACK_SIZE);
}

__END_SYS

#endif
