// EPOS Test Program

using namespace EPOS;

// E6b
int main() {
    OStream cout;

    char * test = (char *) malloc(262044 * sizeof(char));

    cout << "Cleaning up the mess..." << endl;

    free(test);

    cout << "I'm done, bye!" << endl;

    return 0;
}
