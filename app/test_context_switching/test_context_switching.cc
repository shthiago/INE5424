#include <time.h>
#include <process.h>

using namespace EPOS;
OStream cout;

Thread * main_thread;

void print_task_ad_and_segments() {
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
}

int new_task_main_thread() {
    cout << "============================================================" << endl;
    cout << "This is the main Task running in the main function in the app" << endl;
    print_task_ad_and_segments();

    return 0;
}

int main() {
    cout << "Testing New Task creation and switching" << endl;

    cout << "============================================================" << endl;
    cout << "This is the main Task running in the main function in the app" << endl;
    print_task_ad_and_segments();

    new Task(Task::self()->code_segment(), Task::self()->data_segment(), &new_task_main_thread);

    cout << "End of main task thread." << endl;

    Thread::self()->yield();

    cout << "End of main task thread." << endl;
}