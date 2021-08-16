#include <time.h>
#include <process.h>

using namespace EPOS;
OStream cout;

Thread * main_thread;

int func_thread_a() {
    cout << "This is thread A! Suspending..." << endl;
    Thread::self()->suspend();

    cout << "This is thread A again! Returning..." << endl;
    return 0;
}

int func_thread_b() {
    cout << "This is thread B! Suspending..." << endl;
    Thread::self()->suspend();

    cout << "This is thread B again! Returning..." << endl;
    return 1;
}

int func_thread_c() {
    cout << "This is thread C! Suspending..." << endl;
    Thread::self()->suspend();

    cout << "This is thread C again! Returning..." << endl;
    return 2;
}


int main() {
    cout << "Testing Task creation" << endl;

    main_thread = Thread::self();

    Task * current_task = Task::self();

    Segment * current_task_code_segment = current_task->code_segment();
    CPU::Log_Addr current_task_code_init = current_task->code();
    cout << "The code segment of the current task is at " << static_cast<void *>(current_task_code_init)
         << " and it ends at " << static_cast<void *>(current_task_code_init + current_task_code_segment->size())
         << endl;


    Segment * current_task_data_segment = current_task->data_segment();
    CPU::Log_Addr current_task_data_init = current_task->data();
    cout << "The data segment of the current task is at " << static_cast<void *>(current_task_data_init)
         << " and it ends at " << static_cast<void *>(current_task_data_init + current_task_data_segment->size())
         << endl;

    Address_Space * current_task_address_space = current_task->address_space();
    cout << "The current task address space (page directory) is at " << current_task_address_space->pd() << endl;

    cout << endl << "-----" << endl << "Testing creation of 3 threads from task" << endl;

    Thread * thread_a = new Thread(&func_thread_a);
    Thread * thread_b = new Thread(&func_thread_b);
    Thread * thread_c = new Thread(&func_thread_c);

    cout << "Waiting on main task..." << endl;

    Alarm::delay(1000000);

    cout << "This is main thread. Other threads ran and suspended themselves. Will wait them to finish after suspend" << endl;

    Alarm::delay(1000000);

    thread_a->resume();
    Thread::yield();
    thread_b->resume();
    Thread::yield();
    thread_c->resume();
    
    int thread_a_return = thread_a->join();
    int thread_b_return = thread_b->join();
    int thread_c_return = thread_c->join();

    cout << "Thread a return " << thread_a_return << endl;
    cout << "Thread b return " << thread_b_return << endl;
    cout << "Thread c return " << thread_c_return << endl;

    cout << "Test finished successfully!" << endl;
}