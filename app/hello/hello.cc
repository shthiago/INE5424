#include <utility/ostream.h>
#include <syscalls/message.h>
#include <stubs/thread_stub.h>

using namespace EPOS;

OStream cout;

int print_hello() {
    cout << "Hello, world!" << endl;
}

int main()
{
    ThreadAPI t1 = new ThreadAPI(t1);

    cout << "Deu boa aqui" << endl;
    return 0;
}
