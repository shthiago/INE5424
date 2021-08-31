#include <utility/ostream.h>
#include <stubs/api_thread.h>

using namespace EPOS;

OStream cout;

int print_hello() {
    cout << "Hello, world!" << endl;

    return 0;
}

int main()
{
    cout << "Vou sair agora." << endl;

    APIThread::exit(0);

    cout << "Deu ruim, não era pra printar isso não." << endl;

    return 0;
}
