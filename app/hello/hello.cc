#include <utility/ostream.h>

using namespace EPOS;

OStream cout;

int main()
{
    cout << "Hello world!" << endl;
    
    int val = 42;
    CPU::syscall((void*) &val);

    cout << "Hello, again!" << endl;

    return 0;
}
