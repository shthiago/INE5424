#include <time.h>
#include <process.h>

using namespace EPOS;
OStream cout;

Thread * main_thread;

void print_task_ad_and_segments() {
    Task * current_task = Task::self();

    Segment * current_task_code_segment = current_task->code_segment();
    CPU::Log_Addr current_task_code_init = current_task->code();
    cout << "The code segment of the current task is at " << static_cast<void *>(current_task_code_init) << endl
         << " and it ends at " << static_cast<void *>(current_task_code_init + current_task_code_segment->size()) << endl
         << "The physical address of it is " << MMU::Translation(current_task_code_init) << endl << endl;


    Segment * current_task_data_segment = current_task->data_segment();
    CPU::Log_Addr current_task_data_init = current_task->data();
    cout << "The data segment of the current task is at " << static_cast<void *>(current_task_data_init) << endl
         << " and it ends at " << static_cast<void *>(current_task_data_init + current_task_data_segment->size()) << endl
         << "The physical address of it is " << MMU::Translation(current_task_data_init) << endl << endl;

    Address_Space * current_task_address_space = current_task->address_space();
    cout << "The current task address space (page directory) is at " << current_task_address_space->pd() << endl
         << "The physical address of it is " << MMU::Translation(current_task_address_space->pd()) << endl;
}

int new_task_main_thread() {
    cout << "============================================================" << endl;
    cout << "This is the Task created by the main task, running a function!\n" << endl
         << "I will print my address space and segments addresses" << endl;
    print_task_ad_and_segments();

    return 0;
}

int main() {
    cout << "Testing New Task creation and switching" << endl;

    cout << "============================================================" << endl;
    cout << "This is the main Task running in the main function in the app" << endl;
    print_task_ad_and_segments();

    Segment * current_code_seg = Task::self()->code_segment();
    Segment * current_data_seg = Task::self()->code_segment();

    Segment * code_seg_copy = new (SYSTEM) Segment(sizeof(current_code_seg), Segment::Flags::SYS);
    Segment * data_seg_copy = new (SYSTEM) Segment(sizeof(current_data_seg), Segment::Flags::SYS);

    // Attach the new segments to the current ad to make the copy
    Address_Space current_address_space(MMU::current());
    CPU::Log_Addr * code_seg_addr = current_address_space.attach(code_seg_copy);
    CPU::Log_Addr * data_seg_addr = current_address_space.attach(data_seg_copy);

    memcpy(code_seg_addr, current_code_seg, sizeof(*current_code_seg));
    memcpy(data_seg_addr, current_data_seg, sizeof(*current_data_seg));

    new Task(code_seg_copy, data_seg_copy, &new_task_main_thread);

    Thread::self()->yield();

    cout << "End of main task thread." << endl;
}