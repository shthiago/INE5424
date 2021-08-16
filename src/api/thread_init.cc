// EPOS Thread Initialization

#include <machine/timer.h>
#include <machine/ic.h>
#include <system.h>
#include <process.h>

__BEGIN_SYS

extern "C" { void __epos_app_entry(); }

void Thread::init()
{
    db<Init, Thread>(TRC) << "Thread::init()" << endl;

    typedef int (Main)();

    System_Info * si = System::info();
    Main * main;

    if(Traits<System>::multitask)
        main = reinterpret_cast<Main *>(si->lm.app_entry);
    else
        // If EPOS is a library, then adjust the application entry point to __epos_app_entry, which will directly call main().
        // In this case, _init will have already been called, before Init_Application to construct MAIN's global objects.
        main = reinterpret_cast<Main *>(__epos_app_entry);

    CPU::smp_barrier();

    Criterion::init();

    // For multicore systems, avoid creating Idle threads for non-BSP CPUs
    // before the main task is ready to be set as the task for theese idle threads
    static volatile bool task_main_ready_setup = false;

    if (CPU::id() == 0) {
        // In case of multi core system, grant that only the first core (BSP) will setup the main,
        // avoiding copies of main task
        if (Traits<System>::multitask) {
            // Running in a task-based setup, create main Task running main code in a thread
            Address_Space * task_as = new (SYSTEM) Address_Space(MMU::current());
            Segment * task_code_seg = new (SYSTEM) Segment(si->lm.app_code, si->lm.app_code_size, Segment::Flags::APPC);
            Segment * task_data_seg = new (SYSTEM) Segment(si->lm.app_data, si->lm.app_data_size, Segment::Flags::APPD);
            Log_Addr code = si->lm.app_code;
            Log_Addr data = si->lm.app_data;

            // Create main as task
            new (SYSTEM) Task(task_as, task_code_seg, task_data_seg, main, code, data);

            task_main_ready_setup = true;

        } else {
            // Not in a task-based setup, create main as a simple thread
            new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN), main);
        }


        // Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
        new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::IDLE), &Thread::idle);

    } else {
        if (Traits<System>::multitask) {
            // Wait for the main task to be ready, so Idle can set it as its task
            while (!task_main_ready_setup);
        }

        // For cores that are not the BSP, just create a Thread to be the idle, and run it
        new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::IDLE), &Thread::idle);
    }

    CPU::smp_barrier();

    if(smp) {
        if(CPU::id() == 0)
            IC::int_vector(IC::INT_RESCHEDULER, rescheduler);
        IC::enable(IC::INT_RESCHEDULER);
    }


    // The installation of the scheduler timer handler does not need to be done after the
    // creation of threads, since the constructor won't call reschedule() which won't call
    // dispatch that could call timer->reset()
    // Letting reschedule() happen during thread creation is also harmless, since MAIN is
    // created first and dispatch won't replace it nor by itself neither by IDLE (which
    // has a lower priority)
    if(Criterion::timed)
        _timer = new (SYSTEM) Scheduler_Timer(QUANTUM, time_slicer);

    // No more interrupts until we reach init_end
    CPU::int_disable();

    // Transition from CPU-based locking to thread-based locking
    CPU::smp_barrier();
    This_Thread::not_booting();
}

__END_SYS
