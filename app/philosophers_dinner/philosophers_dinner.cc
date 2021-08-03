// EPOS Semaphore Component Test Program

#include <machine/display.h>
#include <time.h>
#include <synchronizer.h>
#include <real-time.h>

using namespace EPOS;

const int iterations = 10;

Mutex table;

Thread * phil[5];
Semaphore * chopstick[5];

OStream cout;

int philosopher(int n, int l, int c);

int main()
{
    for(int i = 0; i < 5; i++)
        chopstick[i] = new Semaphore;

    cout << "\nThe test provide some clues of what scheduler is doing just for reference,\n"
    << "for example, suspending, scheduling, destroying and etc, but the true flow\n"
    << "is a bit more complex, so don't be surprised if a process was scheduled twice,\n"
    << "cuz there are lot of ways (functions) that a process can be removed\n"
    << "from scheduler. The real test consist in check if the `_chosen` process\n"
    << "was really the one with the lesser period.\n" << endl;
    cout << "Task 1 created" << endl;
    phil[0] = new Periodic_Thread(100 * 1000, &philosopher, 0, 5, 32);
    cout << "Task 2 created" << endl;
    phil[1] = new Periodic_Thread(80 * 1000, &philosopher, 1, 10, 44);
    cout << "Task 3 created" << endl;
    phil[2] = new Periodic_Thread(60 * 1000, &philosopher, 2, 16, 39);
    cout << "Task 4 created" << endl;
    phil[3] = new Periodic_Thread(40 * 1000, &philosopher, 3, 16, 24);
    cout << "Task 5 created" << endl;
    phil[4] = new Periodic_Thread(20 * 1000, &philosopher, 4, 10, 20);

    for(int i = 0; i < 5; i++) {
        phil[i]->join();
    }

    for(int i = 0; i < 5; i++)
        delete chopstick[i];
    for(int i = 0; i < 5; i++)
        delete phil[i];

    return 0;
}

int philosopher(int n, int l, int c)
{
    int first = (n < 4)? n : 0;
    int second = (n < 4)? n + 1 : 4;

    for(int i = iterations; i > 0; i--) {
        Delay thinking(1000000);

        chopstick[first]->p();   // get first chopstick
        chopstick[second]->p();  // get second chopstick

        Delay eating(500000);

        chopstick[first]->v();   // release first chopstick
        chopstick[second]->v();  // release second chopstick
    }

    return iterations;
}
