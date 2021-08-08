// EPOS Segment Test Program
#include <memory.h>

using namespace EPOS;

const unsigned ES1_SIZE = 10000;
const unsigned ES2_SIZE = 100000;

OStream cout;

void test_mmu(Address_Space *self) {
    cout << "Starting MMU test... " << endl;

    cout << "Creating three data segments..." << endl;
    Segment * es1 = new (SYSTEM) Segment(ES1_SIZE);
    cout << "  segment 1 created, size:" << ES1_SIZE << " bytes." << endl;
    Segment * es2 = new (SYSTEM) Segment(ES2_SIZE);
    cout << "  segment 2 created, size:" << ES2_SIZE << " bytes." << endl;
    Segment * es3 = new (SYSTEM) Segment(ES1_SIZE);    
    cout << "  segment 3 created, size:" << ES1_SIZE << " bytes." << endl;

    cout << "Attaching this segments to the address space..." << endl;
    CPU::Log_Addr * extra1 = self->attach(es1);
    cout << "  logical address of segment 1 is " << extra1 << "." <<  endl;
    CPU::Log_Addr * extra2 = self->attach(es2);
    cout << "  logical address of segment 2 is " << extra2 << "." << endl;
    CPU::Log_Addr * extra3 = self->attach(es3);
    cout << "  logical address of segment 3 is " << extra3 << "." << endl;

    cout << "Set segments bits to 0... ";
    memset(extra1, 0, ES1_SIZE);
    cout << "segment 1 reseted... ";
    memset(extra2, 0, ES2_SIZE);
    cout << "segment 2 reseted... ";
    memset(extra3, 0, ES1_SIZE);
    cout << "segment 3 reseted... ";
    cout << " all segmentes reseted;" << endl;
    
    cout << "Detaching the first segment...";
    self->detach(es1);
    cout << "segment 1 detached." << endl;
    
    cout << "Deleting the first segment...";
    delete es2;
    cout << "segment 1 deleted." << endl;
    
    cout << "Creating a fourth data segment ..." << endl;
    Segment * es4 = new (SYSTEM) Segment(ES2_SIZE);
    cout << "  segment 4 created, size:" << ES2_SIZE << " bytes." << endl;
    
    cout << "Attaching the fourth segment to the address space..." << endl;
    CPU::Log_Addr * extra4 = self->attach(es4);
    cout << "  logical address of segment 4 is " << extra4 << "." << endl;
    
    cout << "Set segment 4 bits to 0... ";
    memset(extra4, 0, ES2_SIZE);
    cout << "segment 4 reseted." << endl;
    
    cout << "Detaching other segments...";
    self->detach(es2);
    cout << "segment 2 detached...";
    self->detach(es3);
    cout << "segment 3 detached...";
    self->detach(es4);
    cout << "segment 4 detached...";
    cout << "  all segments detached" << endl;
    
    cout << "Deleting segments 2, 3 and 4... ";
    delete es2;
    delete es3;
    delete es4;
    cout << "  segments deleted." << endl;
    
    cout << "MMU test finalized" << endl;
}

int main() {
    cout << "P1 MMU tests" << endl;
    cout << "Page directory address: "
         << reinterpret_cast<void *>(CPU::pdp()) << "" << endl;
    Address_Space self(MMU::current());
    test_mmu(&self);
    cout << "The end" << endl;
    return 0;
}
