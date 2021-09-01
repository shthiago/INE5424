#include <utility/ostream.h>
#include <syscalls/message.h>

__USING_SYS

OStream cout;

int main()
{   
    cout << "Hello, syscall!" << endl;

    return 0;
}
