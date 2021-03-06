// EPOS PC SETUP

// PC_BOOT assumes offset "0" to be the entry point of PC_SETUP
// We will achieve this by declaring _start in segment .init

#include <architecture.h>
#include <machine.h>
#include <utility/elf.h>
#include <utility/string.h>

extern "C" void _start();
extern "C" void _init();

// SETUP entry point is in .init (and not in .text), so it will be linked first and will be the first function after the ELF header in the image
extern "C" void _entry() __attribute__ ((section(".init")));
extern "C" void _setup(char * bi);

__BEGIN_SYS

extern OStream kout, kerr;

// "_start" Synchronization Globals
volatile char * Stacks;
volatile bool Stacks_Ready = false;

// Setup Synchronization Globals
volatile bool Paging_Ready = false;

//========================================================================
// SETUP
//
// SETUP is responsible for bringing the machine into a usable state. It
// sets up several IA32 data structures (IDT, GDT, etc), builds a basic
// memory model (flat) and a basic thread model (exclusive task/exclusive
// thread).
//------------------------------------------------------------------------
class Setup
{
private:
    // Physical memory map
    static const unsigned int MEM_BASE          = Memory_Map::MEM_BASE;
    static const unsigned int MEM_TOP           = Memory_Map::MEM_TOP;
    static const unsigned int APIC_PHY          = APIC::LOCAL_APIC_PHY_ADDR;
    static const unsigned int APIC_SIZE         = APIC::LOCAL_APIC_SIZE;
    static const unsigned int IO_APIC_PHY       = APIC::IO_APIC_PHY_ADDR;
    static const unsigned int IO_APIC_SIZE      = APIC::IO_APIC_SIZE;
    static const unsigned int VGA_PHY           = VGA::FB_PHY_ADDR;
    static const unsigned int VGA_SIZE          = VGA::FB_SIZE;

    // Logical memory map
    static const unsigned int IDT       = Memory_Map::IDT;
    static const unsigned int GDT       = Memory_Map::GDT;
    static const unsigned int TSS0      = Memory_Map::TSS0;
    static const unsigned int SYS       = Memory_Map::SYS;
    static const unsigned int PHY_MEM   = Memory_Map::PHY_MEM;
    static const unsigned int IO        = Memory_Map::IO;
    static const unsigned int SYS_PT    = Memory_Map::SYS_PT;
    static const unsigned int SYS_PD    = Memory_Map::SYS_PD;
    static const unsigned int SYS_INFO  = Memory_Map::SYS_INFO;
    static const unsigned int SYS_DATA  = Memory_Map::SYS_DATA;
    static const unsigned int SYS_CODE  = Memory_Map::SYS_CODE;
    static const unsigned int SYS_STACK = Memory_Map::SYS_STACK;

    // Architecture Imports
    typedef CPU::Reg32 Reg32;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Log_Addr Log_Addr;
    typedef CPU::GDT_Entry GDT_Entry;
    typedef CPU::IDT_Entry IDT_Entry;
    typedef CPU::TSS TSS;
    typedef MMU::Page_Flags Flags;
    typedef MMU::Page Page;
    typedef MMU::Page_Table Page_Table;
    typedef MMU::Page_Directory Page_Directory;
    typedef MMU::PT_Entry PT_Entry;

    // System_Info Imports
    typedef System_Info::Boot_Map BM;
    typedef System_Info::Physical_Memory_Map PMM;
    typedef System_Info::Load_Map LM;

public:
    Setup(char * boot_image);

private:
    void build_lm();
    void build_pmm();
    void get_node_id();

    void say_hi();

    void setup_idt();
    void setup_gdt();
    void setup_sys_pt();
    void setup_sys_pd();
    void enable_paging();
    void setup_tss();

    void load_parts();
    void call_next();

    void detect_memory(unsigned int * base, unsigned int * top);
    void detect_pci(unsigned int * base, unsigned int * top);
    void calibrate_timers();

    static void panic() { Machine::panic(); }

private:
    char * bi;
    System_Info * si;
};

//========================================================================
Setup::Setup(char * boot_image)
{
    // Get boot imaged previously loaded and relocated
    bi = reinterpret_cast<char *>(boot_image);

    si = reinterpret_cast<System_Info *>(bi);

    Display::init();
    VGA::init(VGA_PHY); // Display can be Serial_Display, so VGA here!

    if(si->bm.n_cpus > Traits<Machine>::CPUS)
        si->bm.n_cpus = Traits<Machine>::CPUS;

    // Multicore conditional start up
    int cpu_id = CPU::id();

    db<Setup>(TRC) << "Setup(bi=" << reinterpret_cast<void *>(bi) << ",sp=" << reinterpret_cast<void *>(CPU::sp()) << ")" << endl;

    CPU::smp_barrier(si->bm.n_cpus);
    if(cpu_id == 0) { // Boot strap CPU (BSP)

        db<Setup>(INF) << "System_Info=" << *si << endl;

        // Disable hardware interrupt triggering at PIC
        i8259A::reset();

        unsigned int memb, memt;
        detect_memory(&memb, &memt);

        // Calibrate timers
        calibrate_timers();

        // Build the memory model
        build_lm();
        build_pmm();

        // Try to obtain a node id for this machine
        get_node_id();

        // Print basic facts about this EPOS instance
        say_hi();

        // Configure the memory model defined above
        setup_idt();
        setup_gdt();
        setup_sys_pt();
        setup_sys_pd();

        // Enable paging
        // We won't be able to print anything before the remap() bellow
        db<Setup>(INF) << "IP=" << CPU::ip() << endl;
        db<Setup>(INF) << "SP=" << reinterpret_cast<void *>(CPU::sp()) << endl;
        db<Setup>(INF) << "CR0=" << reinterpret_cast<void *>(CPU::cr0()) << endl;
        db<Setup>(INF) << "CR3=" << reinterpret_cast<void *>(CPU::cr3()) << endl;

        enable_paging();

        // Adjust pointers that will still be used to their logical addresses
        bi = reinterpret_cast<char *>(unsigned(bi) | PHY_MEM);
        si = reinterpret_cast<System_Info *>(SYS_INFO);
        memcpy(si, bi, sizeof(System_Info));
        VGA::init(Memory_Map::VGA); // Display can be Serial_Display, so VGA here!
        APIC::remap(Memory_Map::APIC);

        // Configure a TSS for system calls and inter-level interrupt handling
        setup_tss();

        // Load EPOS parts (e.g. INIT, SYSTEM, APP)
        load_parts();

        // Signalize other CPUs that paging is up
        Paging_Ready = true;

    } else { // Additional CPUs (APs)

        // Wait for the Boot CPU to setup page tables
        while(!Paging_Ready);

        enable_paging();
        setup_tss();
    }

    CPU::smp_barrier(si->bm.n_cpus);

    db<Setup>(INF) << "IP=" << CPU::ip() << endl;
    db<Setup>(INF) << "SP=" << reinterpret_cast<void *>(CPU::sp()) << endl;
    db<Setup>(INF) << "CR0=" << reinterpret_cast<void *>(CPU::cr0()) << endl;
    db<Setup>(INF) << "CR3=" << reinterpret_cast<void *>(CPU::cr3()) << endl;

    // SETUP ends here, transfer control to next stage (INIT or APP)
    call_next();

    // SETUP is now part of the free memory and this point should never be
    // reached, but, just in case ... :-)
    panic();
}

//========================================================================
void Setup::build_lm()
{
    // Check size of System_Info
    if(sizeof(System_Info) > sizeof(Page))
        db<Setup>(WRN) << "System_Info is bigger than a page (" << sizeof(System_Info) << ")!" << endl;

    // Get boot image structure
    si->lm.has_stp = (si->bm.setup_offset != -1u);
    si->lm.has_ini = (si->bm.init_offset != -1u);
    si->lm.has_sys = (si->bm.system_offset != -1u);
    si->lm.has_app = (si->bm.application_offset != -1u);
    si->lm.has_ext = (si->bm.extras_offset != -1u);

    // Check SETUP integrity and get the size of its segments
    si->lm.stp_entry = 0;
    si->lm.stp_segments = 0;
    si->lm.stp_code = ~0U;
    si->lm.stp_code_size = 0;
    si->lm.stp_data = ~0U;
    si->lm.stp_data_size = 0;
    if(si->lm.has_stp) {
        ELF * stp_elf = reinterpret_cast<ELF *>(&bi[si->bm.setup_offset]);
        if(!stp_elf->valid()) {
            db<Setup>(ERR) << "SETUP ELF image is corrupted!" << endl;
            panic();
        }

        si->lm.stp_entry = stp_elf->entry();
        si->lm.stp_segments = stp_elf->segments();
        si->lm.stp_code = stp_elf->segment_address(0);
        si->lm.stp_code_size = stp_elf->segment_size(0);
        if(stp_elf->segments() > 1) {
            for(int i = 1; i < stp_elf->segments(); i++) {
                if(stp_elf->segment_type(i) != PT_LOAD)
                    continue;
                if(stp_elf->segment_address(i) < si->lm.stp_data)
                    si->lm.stp_data = stp_elf->segment_address(i);
                si->lm.stp_data_size += stp_elf->segment_size(i);
            }
        }
    }

    // Check INIT integrity and get the size of its segments
    si->lm.ini_entry = 0;
    si->lm.ini_segments = 0;
    si->lm.ini_code = ~0U;
    si->lm.ini_code_size = 0;
    si->lm.ini_data = ~0U;
    si->lm.ini_data_size = 0;
    if(si->lm.has_ini) {
        ELF * ini_elf = reinterpret_cast<ELF *>(&bi[si->bm.init_offset]);
        if(!ini_elf->valid()) {
            db<Setup>(ERR) << "INIT ELF image is corrupted!" << endl;
            panic();
        }

        si->lm.ini_entry = ini_elf->entry();
        si->lm.ini_segments = ini_elf->segments();
        si->lm.ini_code = ini_elf->segment_address(0);
        si->lm.ini_code_size = ini_elf->segment_size(0);
        if(ini_elf->segments() > 1) {
            for(int i = 1; i < ini_elf->segments(); i++) {
                if(ini_elf->segment_type(i) != PT_LOAD)
                    continue;
                if(ini_elf->segment_address(i) < si->lm.ini_data)
                    si->lm.ini_data = ini_elf->segment_address(i);
                si->lm.ini_data_size += ini_elf->segment_size(i);
            }
        }
    }

    // Check SYSTEM integrity and get the size of its segments
    si->lm.sys_entry = 0;
    si->lm.sys_segments = 0;
    si->lm.sys_code = ~0U;
    si->lm.sys_code_size = 0;
    si->lm.sys_data = ~0U;
    si->lm.sys_data_size = 0;
    si->lm.sys_stack = SYS_STACK;
    si->lm.sys_stack_size = Traits<System>::STACK_SIZE * si->bm.n_cpus;
    if(si->lm.has_sys) {
        ELF * sys_elf = reinterpret_cast<ELF *>(&bi[si->bm.system_offset]);
        if(!sys_elf->valid()) {
            db<Setup>(ERR) << "OS ELF image is corrupted!" << endl;
            panic();
        }

        si->lm.sys_entry = sys_elf->entry();
        si->lm.sys_segments = sys_elf->segments();
        si->lm.sys_code = sys_elf->segment_address(0);
        si->lm.sys_code_size = sys_elf->segment_size(0);
        if(sys_elf->segments() > 1) {
            for(int i = 1; i < sys_elf->segments(); i++) {
                if(sys_elf->segment_type(i) != PT_LOAD)
                    continue;
                if(sys_elf->segment_address(i) < si->lm.sys_data)
                    si->lm.sys_data = sys_elf->segment_address(i);
                si->lm.sys_data_size += sys_elf->segment_size(i);
            }
        }

        if(si->lm.sys_code != SYS_CODE) {
            db<Setup>(ERR) << "OS code segment address (" << reinterpret_cast<void *>(si->lm.sys_code) << ") does not match the machine's memory map (" << reinterpret_cast<void *>(SYS_CODE) << ")!" << endl;
            panic();
        }
        if(si->lm.sys_code + si->lm.sys_code_size > si->lm.sys_data) {
            db<Setup>(ERR) << "OS code segment is too large!" << endl;
            panic();
        }
        if(si->lm.sys_data != SYS_DATA) {
            db<Setup>(ERR) << "OS data segment address (" << reinterpret_cast<void *>(si->lm.sys_data) << ") does not match the machine's memory map (" << reinterpret_cast<void *>(SYS_DATA) << ")!" << endl;
            panic();
        }
        if(si->lm.sys_data + si->lm.sys_data_size > si->lm.sys_stack) {
            db<Setup>(ERR) << "OS data segment is too large!" << endl;
            panic();
        }
        if(MMU::page_tables(MMU::pages(si->lm.sys_stack - SYS + si->lm.sys_stack_size)) > 1) {
            db<Setup>(ERR) << "OS stack segment is too large!" << endl;
            panic();
        }
    }

    // Check APPLICATION integrity and get the size of its segments
    si->lm.app_entry = 0;
    si->lm.app_segments = 0;
    si->lm.app_code = ~0U;
    si->lm.app_code_size = 0;
    si->lm.app_data = ~0U;
    si->lm.app_data_size = 0;
    if(si->lm.has_app) {
        ELF * app_elf = reinterpret_cast<ELF *>(&bi[si->bm.application_offset]);
        if(!app_elf->valid()) {
            db<Setup>(ERR) << "Application ELF image is corrupted!" << endl;
            panic();
        }
        si->lm.app_entry = app_elf->entry();
        si->lm.app_code = app_elf->segment_address(0);
        si->lm.app_code_size = app_elf->segment_size(0);
        if(app_elf->segments() > 1) {
            for(int i = 1; i < app_elf->segments(); i++) {
                if(app_elf->segment_type(i) != PT_LOAD)
                    continue;
                if(app_elf->segment_address(i) < si->lm.app_data)
                    si->lm.app_data = app_elf->segment_address(i);
                si->lm.app_data_size += app_elf->segment_size(i);
            }
        }
        if(Traits<System>::multiheap) { // Application heap in data segment
            si->lm.app_data_size = MMU::align_page(si->lm.app_data_size);
            si->lm.app_stack = si->lm.app_data + si->lm.app_data_size;
            si->lm.app_data_size += MMU::align_page(Traits<Application>::STACK_SIZE);
            si->lm.app_heap = si->lm.app_data + si->lm.app_data_size;
            si->lm.app_data_size += MMU::align_page(Traits<Application>::HEAP_SIZE);
        }
        if(si->lm.has_ext) { // Check for EXTRA data in the boot image
            si->lm.app_extra = si->lm.app_data + si->lm.app_data_size;
            si->lm.app_extra_size = si->bm.img_size - si->bm.extras_offset;
            if(Traits<System>::multiheap)
                si->lm.app_extra_size = MMU::align_page(si->lm.app_extra_size);
            si->lm.app_data_size += si->lm.app_extra_size;
        }
    }
}

//========================================================================
void Setup::build_pmm()
{
    // Allocate (reserve) memory for all entities we have to setup.
    // We'll start at the highest address to make possible a memory model
    // on which the application's logical and physical address spaces match.

    Phy_Addr top_page = MMU::pages(si->bm.mem_top);

    // IDT (1 x sizeof(Page))
    top_page -= 1;
    si->pmm.idt = top_page * sizeof(Page);

    // GDT (1 x sizeof(Page))
    top_page -= 1;
    si->pmm.gdt = top_page * sizeof(Page);

    // System Page Table (1 x sizeof(Page))
    top_page -= 1;
    si->pmm.sys_pt = top_page * sizeof(Page);

    // System Page Directory (1 x sizeof(Page))
    top_page -= 1;
    si->pmm.sys_pd = top_page * sizeof(Page);

    // System Info (1 x sizeof(Page))
    top_page -= 1;
    si->pmm.sys_info = top_page * sizeof(Page);

    // TSSs (1 x sizeof(Page) x CPUs)
    top_page -= Traits<Machine>::CPUS;
    si->pmm.tss = top_page * sizeof(Page);

    // Page tables to map the whole physical memory
    // = NP/NPTE_PT * sizeof(Page)
    //   NP = size of physical memory in pages
    //   NPTE_PT = number of page table entries per page table
    top_page -= MMU::page_tables(MMU::pages(si->bm.mem_top - si->bm.mem_base));
    si->pmm.phy_mem_pts = top_page * sizeof(Page);

    // Page tables to map the IO address space
    // = NP/NPTE_PT * sizeof(Page)
    // NP = size of PCI address space in pages
    // NPTE_PT = number of page table entries per page table
    detect_pci(&si->bm.mio_base, &si->bm.mio_top);
    unsigned int io_size = MMU::pages(si->bm.mio_top - si->bm.mio_base);
    io_size += APIC_SIZE / sizeof(Page); // Add room for APIC (4 kB, 1 page)
    io_size += IO_APIC_SIZE / sizeof(Page); // Add room for IO_APIC (4 kB, 1 page)
    io_size += VGA_SIZE / sizeof(Page); // Add room for VGA (32 kB, 8 pages)
    top_page -= MMU::page_tables(io_size);
    si->pmm.io_pts = top_page * sizeof(Page);

    // SYSTEM code segment
    top_page -= MMU::pages(si->lm.sys_code_size);
    si->pmm.sys_code = top_page * sizeof(Page);

    // SYSTEM data segment
    top_page -= MMU::pages(si->lm.sys_data_size);
    si->pmm.sys_data = top_page * sizeof(Page);

    // SYSTEM stack segment
    top_page -= MMU::pages(si->lm.sys_stack_size);
    si->pmm.sys_stack = top_page * sizeof(Page);

    // The memory allocated so far will "disappear" from the system as we set usr_mem_top as follows:
    si->pmm.usr_mem_base = si->bm.mem_base;
    si->pmm.usr_mem_top = top_page * sizeof(Page);

    // Free chuncks (passed to MMU::init)
    si->pmm.free1_base = MMU::align_page(si->lm.app_code + si->lm.app_code_size);
    si->pmm.free1_top = MMU::align_page(640 * 1024);
    // Skip VRAM and ROMs
    si->pmm.free2_base = MMU::align_page(1024 * 1024);
    si->pmm.free2_top = MMU::align_page(si->lm.app_data);
    si->pmm.free3_base = MMU::align_page(si->lm.app_data + si->lm.app_data_size);
    si->pmm.free3_top = MMU::align_page(si->bm.mem_top);

    // Test if we didn't overlap SETUP and the boot image
    if(si->pmm.usr_mem_top <= si->lm.stp_code + si->lm.stp_code_size + si->lm.stp_data_size) {
        db<Setup>(ERR) << "SETUP would have been overwritten!" << endl;
        panic();
    }
}

//========================================================================
void Setup::get_node_id()
{
    // If we didn't get our node's id in the boot image, we'll to try to
    // get if from an eventual BOOPT reply used to boot up the system before
    // we allocate more memory
    // if(si->bm.host_id == (unsigned short) -1)
    // get_bootp_info(&si->bm.host_id);
}

//========================================================================
void Setup::say_hi()
{
    db<Setup>(TRC) << "Setup::say_hi()" << endl;
    db<Setup>(INF) << "System_Info=" << *si << endl;

    kout << endl;

    if(!si->lm.has_app) {
        db<Setup>(ERR) << "No APPLICATION in boot image, you don't need EPOS!" << endl;
        panic();
    }
    if(!si->lm.has_sys)
        db<Setup>(INF) << "No SYSTEM in boot image, assuming EPOS is a library!" << endl;

    kout << "Setting up this machine as follows: " << endl;
    kout << "  Processor:    " << Traits<Machine>::CPUS << " x IA32 at " << Traits<CPU>::CLOCK / 1000000 << " MHz (BUS clock = " << Traits<CPU>::CLOCK / 1000000 << " MHz)" << endl;
    kout << "  Memory:       " << (si->bm.mem_top - si->bm.mem_base) / 1024 << " KB [" << (void *)si->bm.mem_base << ":" << (void *)si->bm.mem_top << "]" << endl;
    kout << "  User memory:  " << (si->pmm.usr_mem_top - si->pmm.usr_mem_base) / 1024 << " KB [" << (void *)si->pmm.usr_mem_base << ":" << (void *)si->pmm.usr_mem_top << "]" << endl;
    kout << "  I/O space:    " << (si->bm.mio_top - si->bm.mio_base) / 1024 << " KB [" << (void *)si->bm.mio_base << ":" << (void *)si->bm.mio_top << "]" << endl;
    kout << "  Node Id:      ";
    if(si->bm.node_id != -1)
        kout << si->bm.node_id << " (" << Traits<Build>::NODES << ")" << endl;
    else
        kout << "will get from the network!" << endl;
    kout << "  Position:     ";
    if(si->bm.space_x != -1)
        kout << "(" << si->bm.space_x << "," << si->bm.space_y << "," << si->bm.space_z << ")" << endl;
    else
        kout << "will get from the network!" << endl;
    if(si->lm.has_stp)
        kout << "  Setup:        " << si->lm.stp_code_size + si->lm.stp_data_size << " bytes" << endl;
    if(si->lm.has_ini)
        kout << "  Init:         " << si->lm.ini_code_size + si->lm.ini_data_size << " bytes" << endl;
    if(si->lm.has_sys)
        kout << "  OS code:      " << si->lm.sys_code_size << " bytes" << "\tdata: " << si->lm.sys_data_size << " bytes" << "   stack: " << si->lm.sys_stack_size << " bytes" << endl;
    if(si->lm.has_app)
        kout << "  APP code:     " << si->lm.app_code_size << " bytes" << "\tdata: " << si->lm.app_data_size << " bytes" << endl;
    if(si->lm.has_ext)
        kout << "  Extras:       " << si->lm.app_extra_size << " bytes" << endl;

    kout << endl;
}

//========================================================================
void Setup::enable_paging()
{
    // Set IDTR (limit = 1 x sizeof(Page))
    CPU::idtr(sizeof(Page) - 1, IDT);

    // Reload GDTR with its linear address (one more absurd from Intel!)
    CPU::gdtr(sizeof(Page) - 1, GDT);

    // Set CR3 (PDBR) register
    CPU::cr3(si->pmm.sys_pd);

    // Enable paging
    Reg32 aux = CPU::cr0();
    aux &= CPU::CR0_CLEAR;
    aux |= CPU::CR0_SET;
    CPU::cr0(aux);

    // The following relative jump is to break the IA32 prefetch queue
    // (in case cr0() was a macro and didn't do it when returning)
    // and also to start using logical addresses
    ASM("ljmp %0, %1 + 1f" : : "i"(CPU::SEL_FLT_CODE), "i"(PHY_MEM));
    ASM("1:");

    // Reload segment registers with GDT_FLT_DATA
    ASM("" : : "a" (CPU::SEL_FLT_DATA));
    ASM("movw %ax, %ds");
    ASM("movw %ax, %es");
    ASM("movw %ax, %fs");
    ASM("movw %ax, %gs");
    ASM("movw %ax, %ss");

    // Set stack pointer to its logical address
    ASM("orl %0, %%esp" : : "i" (PHY_MEM));

    // Flush TLB to ensure we've got the right memory organization
    MMU::flush_tlb();
}

//========================================================================
void Setup::setup_idt()
{
    db<Setup>(TRC) << "setup_idt(idt=" << (void *)si->pmm.idt << ")" << endl;

    // Get the physical address for the IDT
    IDT_Entry * idt = reinterpret_cast<IDT_Entry *>((void *)si->pmm.idt);

    // Clear IDT
    memset(idt, 0, sizeof(Page));

    // Adjust handler addresses to logical addresses
    Log_Addr panic_h = Log_Addr(&panic) | PHY_MEM;

    // Map all handlers to panic()
    for(unsigned int i = 0; i < CPU::IDT_ENTRIES; i++)
        idt[i] = IDT_Entry(CPU::GDT_SYS_CODE, panic_h, CPU::SEG_IDT_ENTRY);

    db<Setup>(INF) << "IDT[0  ]=" << idt[0] << " (" << panic_h << ")" << endl;

    db<Setup>(INF) << "IDT[255]=" << idt[255] << " (" << panic_h << ")" << endl;
}

//========================================================================
void Setup::setup_gdt()
{
    db<Setup>(TRC) << "setup_gdt(gdt=" << (void *)si->pmm.gdt << ")" << endl;

    // Get the physical address for the GDT
    GDT_Entry * gdt = reinterpret_cast<GDT_Entry *>((void *)si->pmm.gdt);

    // Clear GDT
    memset(gdt, 0, sizeof(Page));

    // GDT_Entry(base, limit, {P,DPL,S,TYPE})
    gdt[CPU::GDT_NULL]      = GDT_Entry(0,        0, 0);
    gdt[CPU::GDT_FLT_CODE]  = GDT_Entry(0,  0xfffff, CPU::SEG_FLT_CODE);
    gdt[CPU::GDT_FLT_DATA]  = GDT_Entry(0,  0xfffff, CPU::SEG_FLT_DATA);
    gdt[CPU::GDT_APP_CODE]  = GDT_Entry(0,  0xfffff, CPU::SEG_APP_CODE);
    gdt[CPU::GDT_APP_DATA]  = GDT_Entry(0,  0xfffff, CPU::SEG_APP_DATA);
    for(unsigned int i = 0; i < Traits<Machine>::CPUS; i++)
        gdt[CPU::GDT_TSS0 + i] = GDT_Entry(TSS0 + i * sizeof(Page), 0xfff, CPU::SEG_TSS0);

    db<Setup>(INF) << "GDT[NULL=" << CPU::GDT_NULL     << "]=" << gdt[CPU::GDT_NULL] << endl;
    db<Setup>(INF) << "GDT[SYCD=" << CPU::GDT_SYS_CODE << "]=" << gdt[CPU::GDT_SYS_CODE] << endl;
    db<Setup>(INF) << "GDT[SYDT=" << CPU::GDT_SYS_DATA << "]=" << gdt[CPU::GDT_SYS_DATA] << endl;
    db<Setup>(INF) << "GDT[APCD=" << CPU::GDT_APP_CODE << "]=" << gdt[CPU::GDT_APP_CODE] << endl;
    db<Setup>(INF) << "GDT[APDT=" << CPU::GDT_APP_DATA << "]=" << gdt[CPU::GDT_APP_DATA] << endl;
    for(unsigned int i = 0; i < Traits<Machine>::CPUS; i++)
        db<Setup>(INF) << "GDT[TSS" << i << "=" << CPU::GDT_TSS0  + i << "]=" << gdt[CPU::GDT_TSS0 + i] << endl;
}

//========================================================================
void Setup::setup_sys_pt()
{
    db<Setup>(TRC) << "Setup::setup_sys_pt(pmm="
                   << "{idt="     << (void *)si->pmm.idt
                   << ",gdt="     << (void *)si->pmm.gdt
                   << ",pt="      << (void *)si->pmm.sys_pt
                   << ",pd="      << (void *)si->pmm.sys_pd
                   << ",si="      << (void *)si->pmm.sys_info
                   << ",sysc={b=" << (void *)si->pmm.sys_code << ",s=" << MMU::pages(si->lm.sys_code_size) << "}"
                   << ",sysd={b=" << (void *)si->pmm.sys_data << ",s=" << MMU::pages(si->lm.sys_data_size) << "}"
                   << ",syss={b=" << (void *)si->pmm.sys_stack << ",s=" << MMU::pages(si->lm.sys_stack_size) << "}"
                   << "})" << endl;

    // Get the physical address for the System Page Table
    PT_Entry * sys_pt = reinterpret_cast<PT_Entry *>((void *)si->pmm.sys_pt);

    // Clear the System Page Table
    memset(sys_pt, 0, sizeof(Page));

    // IDT
    sys_pt[MMU::page(IDT)] = si->pmm.idt | Flags::SYS;

    // GDT
    sys_pt[MMU::page(GDT)] = si->pmm.gdt | Flags::SYS;

    // TSSs
    for(unsigned int i = 0; i < Traits<Machine>::CPUS; i++)
        sys_pt[MMU::page(TSS0) + i] = (si->pmm.tss + i * sizeof(Page)) | Flags::SYS;

    // Set an entry to this page table, so the system can access it later
    sys_pt[MMU::page(SYS_PT)] = si->pmm.sys_pt | Flags::SYS;

    // System Page Directory
    sys_pt[MMU::page(SYS_PD)] = si->pmm.sys_pd | Flags::SYS;

    // System Info
    sys_pt[MMU::page(SYS_INFO)] = si->pmm.sys_info | Flags::SYS;

    unsigned int i;
    PT_Entry aux;

    // SYSTEM code
    for(i = 0, aux = si->pmm.sys_code; i < MMU::pages(si->lm.sys_code_size); i++, aux = aux + sizeof(Page))
        sys_pt[MMU::page(SYS_CODE) + i] = aux | Flags::SYS;

    // SYSTEM data
    for(i = 0, aux = si->pmm.sys_data; i < MMU::pages(si->lm.sys_data_size); i++, aux = aux + sizeof(Page))
        sys_pt[MMU::page(SYS_DATA) + i] = aux | Flags::SYS;

    // SYSTEM stack (used only during init and for the ukernel model)
    for(i = 0, aux = si->pmm.sys_stack; i < MMU::pages(si->lm.sys_stack_size); i++, aux = aux + sizeof(Page))
        sys_pt[MMU::page(SYS_STACK) + i] = aux | Flags::SYS;

    db<Setup>(INF) << "SYS_SPT=" << *reinterpret_cast<Page_Table *>(sys_pt) << endl;
}

//========================================================================
void Setup::setup_sys_pd()
{
    db<Setup>(TRC) << "setup_sys_pd(bm="
                   << "{memb="  << (void *)si->bm.mem_base
                   << ",memt="  << (void *)si->bm.mem_top
                   << ",miob="  << (void *)si->bm.mio_base
                   << ",miot="  << (void *)si->bm.mio_top
                   << "{si="    << (void *)si->pmm.sys_info
                   << ",spt="   << (void *)si->pmm.sys_pt
                   << ",spd="   << (void *)si->pmm.sys_pd
                   << ",mem="   << (void *)si->pmm.phy_mem_pts
                   << ",io="    << (void *)si->pmm.io_pts
                   << ",umemb=" << (void *)si->pmm.usr_mem_base
                   << ",umemt=" << (void *)si->pmm.usr_mem_top
                   << ",sysc="  << (void *)si->pmm.sys_code
                   << ",sysd="  << (void *)si->pmm.sys_data
                   << ",syss="  << (void *)si->pmm.sys_stack
                   << ",apct="  << (void *)si->pmm.app_code_pts
                   << ",apdt="  << (void *)si->pmm.app_data_pts
                   << ",fr1b="  << (void *)si->pmm.free1_base
                   << ",fr1t="  << (void *)si->pmm.free1_top
                   << ",fr2b="  << (void *)si->pmm.free2_base
                   << ",fr2t="  << (void *)si->pmm.free2_top
                   << "})" << endl;

    // Get the physical address for the System Page Directory
    PT_Entry * sys_pd = reinterpret_cast<PT_Entry *>((void *)si->pmm.sys_pd);

    // Clear the System Page Directory
    memset(sys_pd, 0, sizeof(Page));

    // Calculate the number of page tables needed to map the physical memory
    unsigned int mem_size = MMU::pages(si->bm.mem_top - si->bm.mem_base);
    int n_pts = MMU::page_tables(mem_size);

    // Map the whole physical memory into the page tables pointed by phy_mem_pts
    PT_Entry * pts = reinterpret_cast<PT_Entry *>(si->pmm.phy_mem_pts);
    for(unsigned int i = MMU::pages(si->bm.mem_base); i < mem_size; i++)
        pts[i] = (i * sizeof(Page)) | Flags::APP;

    // Attach all physical memory starting at PHY_MEM
    assert((MMU::directory(MMU::align_directory(PHY_MEM)) + n_pts) < (MMU::PD_ENTRIES - 4)); // check if it would overwrite the OS
    for(unsigned int i = MMU::directory(MMU::align_directory(PHY_MEM)), j = 0; i < MMU::directory(MMU::align_directory(PHY_MEM)) + n_pts; i++, j++)
        sys_pd[i] = (si->pmm.phy_mem_pts + j * sizeof(Page)) | Flags::SYS;

    // Calculate the number of page tables needed to map the IO address space
    unsigned int io_size = MMU::pages(si->bm.mio_top - si->bm.mio_base);
    io_size += APIC_SIZE / sizeof(Page); // Add room for APIC (4 kB, 1 page)
    io_size += VGA_SIZE / sizeof(Page); // Add room for VGA (64 kB, 16 pages)
    n_pts = MMU::page_tables(io_size);

    // Map IO address space into the page tables pointed by io_pts
    pts = reinterpret_cast<PT_Entry *>(si->pmm.io_pts);
    unsigned int i = 0;
    for(; i < (APIC_SIZE / sizeof(Page)); i++)
        pts[i] = (APIC_PHY + i * sizeof(Page)) | Flags::APIC;
    for(unsigned int j = 0; i < ((APIC_SIZE / sizeof(Page)) + (IO_APIC_SIZE / sizeof(Page))); i++, j++)
        pts[i] = (IO_APIC_PHY + j * sizeof(Page)) | Flags::APIC;
    for(unsigned int j = 0; i < ((APIC_SIZE / sizeof(Page)) + (IO_APIC_SIZE / sizeof(Page)) + (VGA_SIZE / sizeof(Page))); i++, j++)
        pts[i] = (VGA_PHY + j * sizeof(Page)) | Flags::VGA;
    for(unsigned int j = 0; i < io_size; i++, j++)
        pts[i] = (si->bm.mio_base + j * sizeof(Page)) | Flags::PCI;

    // Attach devices' memory at Memory_Map::IO
    assert((MMU::directory(MMU::align_directory(IO)) + n_pts) < (MMU::PD_ENTRIES - 3)); // check if it would overwrite the OS
    for(unsigned int i = MMU::directory(MMU::align_directory(IO)), j = 0; i < MMU::directory(MMU::align_directory(IO)) + n_pts; i++, j++)
        sys_pd[i] = (si->pmm.io_pts + j * sizeof(Page)) | Flags::PCI;

    // Attach the OS (i.e. sys_pt)
    sys_pd[MMU::directory(SYS)] = si->pmm.sys_pt | Flags::SYS;

    // Attach memory starting at MEM_BASE
    for(unsigned int i = MMU::directory(MMU::align_directory(si->bm.mem_base)); i < MMU::directory(MMU::align_directory(si->bm.mem_top)); i++)
        sys_pd[i] = (si->pmm.phy_mem_pts + i * sizeof(Page)) | Flags::APP;

    db<Setup>(INF) << "SYS_PD=" << *reinterpret_cast<Page_Table *>(sys_pd) << endl;
}

//========================================================================
void Setup::setup_tss()
{
    // Get current CPU's TSS logical address (after enabling paging)
    unsigned int cpu_id = CPU::id();
    TSS * tss = reinterpret_cast<TSS *>(TSS0 + cpu_id * sizeof(Page));

    db<Setup>(TRC) << "setup_tss(tss" << cpu_id << "=" << Log_Addr(tss) << ")" << endl;

    // Clear TSS
    memset(tss, 0, sizeof(Page));

    // Configure only the segment selectors and the kernel stack
    tss->ss0 = CPU::SEL_SYS_DATA;
    tss->esp0 = SYS_STACK + Traits<System>::STACK_SIZE; // APs' tss->esp0 will be reconfigured later by CPU::Context::load()
    tss->cs = (CPU::GDT_SYS_CODE << 3) | CPU::PL_APP;
    tss->ss = (CPU::GDT_SYS_DATA << 3) | CPU::PL_APP;
    tss->ds = tss->ss;
    tss->es = tss->ss;
    tss->fs = tss->ss;
    tss->gs = tss->ss;

    // Load TR with TSS
    CPU::Reg16 tr = ((CPU::GDT_TSS0 + cpu_id) << 3) | CPU::PL_SYS;
    CPU::tr(tr);
    tr = CPU::tr();

    db<Setup>(INF) << "TR=" << tr << ",TSS" << cpu_id << "={ss0=" << tss->ss0 << ",esp0=" << Log_Addr(tss->esp0) << "}" << endl;
}

//========================================================================
void Setup::load_parts()
{
    // Load INIT
    if(si->lm.has_ini) {
        db<Setup>(TRC) << "Setup::load_init()" << endl;
        ELF * ini_elf = reinterpret_cast<ELF *>(&bi[si->bm.init_offset]);
        if(ini_elf->load_segment(0) < 0) {
            db<Setup>(ERR) << "INIT code segment was corrupted during SETUP!" << endl;
            panic();
        }
        for(int i = 1; i < ini_elf->segments(); i++)
            if(ini_elf->load_segment(i) < 0) {
                db<Setup>(ERR) << "INIT data segment was corrupted during SETUP!" << endl;
                panic();
            }
    }

    // Load SYSTEM
    if(si->lm.has_sys) {
        db<Setup>(TRC) << "Setup::load_os()" << endl;
        ELF * sys_elf = reinterpret_cast<ELF *>(&bi[si->bm.system_offset]);
        if(sys_elf->load_segment(0) < 0) {
            db<Setup>(ERR) << "OS code segment was corrupted during SETUP!" << endl;
            panic();
        }
        for(int i = 1; i < sys_elf->segments(); i++)
            if(sys_elf->load_segment(i) < 0) {
                db<Setup>(ERR) << "OS data segment was corrupted during SETUP!" << endl;
                panic();
            }
    }

    // Load APP
    if(si->lm.has_app) {
        ELF * app_elf = reinterpret_cast<ELF *>(&bi[si->bm.application_offset]);
        db<Setup>(TRC) << "Setup::load_app()" << endl;
        if(app_elf->load_segment(0) < 0) {
            db<Setup>(ERR) << "Application code segment was corrupted during SETUP!" << endl;
            panic();
        }
        for(int i = 1; i < app_elf->segments(); i++)
            if(app_elf->load_segment(i) < 0) {
                db<Setup>(ERR) << "Application data segment was corrupted during SETUP!" << endl;
                panic();
            }
    }

    // Load EXTRA
    if(si->lm.has_ext)
        memcpy(Log_Addr(si->lm.app_extra), &bi[si->bm.extras_offset], si->lm.app_extra_size);
}

//========================================================================
void Setup::call_next()
{
    int cpu_id = CPU::id();

    // Check for next stage and obtain the entry point
    register Log_Addr ip;
    if(si->lm.has_ini) {
        db<Setup>(TRC) << "Executing system's global constructors ..." << endl;
        reinterpret_cast<void (*)()>((void *)si->lm.sys_entry)();
        ip = si->lm.ini_entry;
    } else if(si->lm.has_sys)
        ip = si->lm.sys_entry;
    else
        ip = si->lm.app_entry;

    // Arrange a stack for each CPU to support stage transition
    // Bootstrap CPU uses a full stack, while non-boot get reduced ones
    // The 2 integers on the stacks are room for return addresses used
    // in some EPOS architectures
    register Log_Addr sp = SYS_STACK + Traits<System>::STACK_SIZE * (cpu_id + 1) - 2 * sizeof(int);

    db<Setup>(TRC) << "Setup::call_next(ip=" << ip << ",sp=" << sp << ") => ";
    if(si->lm.has_ini)
        db<Setup>(TRC) << "INIT" << endl;
    else if(si->lm.has_sys)
        db<Setup>(TRC) << "SYSTEM" << endl;
    else
        db<Setup>(TRC) << "APPLICATION" << endl;

    db<Setup>(INF) << "SETUP ends here!" << endl;

    CPU::smp_barrier(si->bm.n_cpus);

    // Set SP and call next stage
    CPU::sp(sp);
    static_cast<void (*)()>(ip)();

    if(CPU::id() == 0) { // Boot strap CPU (BSP)
        // This will only happen when INIT was called and Thread was disabled
        // Note we don't have the original stack here anymore!
        reinterpret_cast<void (*)()>(si->lm.app_entry)();
    }
}

//========================================================================
void Setup::detect_memory(unsigned int * base, unsigned int * top)
{
    db<Setup>(TRC) << "Setup::detect_memory()" << endl;

    unsigned int i;
    unsigned int * mem = reinterpret_cast<unsigned int *>(MEM_BASE / sizeof(int));
    for(i = Traits<Machine>::INIT; i < MEM_TOP; i += 16 * sizeof(MMU::Page))
        mem[i /  sizeof(int)] = i;

    for(i = Traits<Machine>::INIT; i < MEM_TOP; i += 16 * sizeof(MMU::Page))
        if(mem[i / sizeof(int)] != i) {
            db<Setup>(ERR) << "Less memory was detected (" << i / 1024 << " kb) than specified in the configuration (" << MEM_TOP / 1024 << " kb)!" << endl;
            break;
        }

    *base = MEM_BASE;
    *top = i;

    db<Setup>(INF) << "Memory={base=" << reinterpret_cast<void *>(*base) << ",top=" << reinterpret_cast<void *>(*top) << "}" << endl;
}

//========================================================================
// Detects the size of the PCI physical memory range (apperture) necessary
// to map all PCI devices.
// Sets base to the lowest address and top to the highest. Both in bytes.
void Setup::detect_pci(unsigned int * base, unsigned int * top)
{
    db<Setup>(TRC) << "Setup::detect_pci()" << endl;

    // Scan the PCI bus looking for devices with memory mapped regions
    *base = ~0U;
    *top = 0U;
    for(int bus = 0; bus <= Traits<PCI>::MAX_BUS; bus++) {
        for(int dev_fn = 0; dev_fn <= Traits<PCI>::MAX_DEV_FN; dev_fn++) {
            PCI::Locator loc(bus, dev_fn);
            PCI::Header hdr;
            PCI::header(loc, &hdr);
            if(hdr) {
                db<Setup>(INF) << "PCI" << hdr << endl;
                for(unsigned int i = 0; i < PCI::Region::N; i++) {
                    PCI::Region * reg = &hdr.region[i];
                    if(*reg) {
                        db<Setup>(INF) << "  reg[" << i << "]=" << *reg << endl;
                        if(reg->memory) {
                            if(reg->phy_addr < *base)
                                *base = reg->phy_addr;
                            if((reg->phy_addr + reg->size) > *top)
                                *top = reg->phy_addr + reg->size;
                        }
                    }
                }
            }
        }
    }

    db<Setup>(INF) << "PCI address space={base=" << reinterpret_cast<void *>(*base) << ",top=" << reinterpret_cast<void *>(*top) << "}" << endl;
}

//========================================================================
void Setup::calibrate_timers()
{
    db<Setup>(TRC) << "Setup::calibrate_timers()" << endl;

    // Disable speaker so we can use channel 2 of i8253
    i8255::port_b(i8255::port_b() & ~(i8255::SPEAKER | i8255::I8253_GATE2));

    // Program i8253 channel 2 to count 50 ms
    i8253::config(2, i8253::CLOCK/20, false, false);

    // Enable i8253 channel 2 counting
    i8255::port_b(i8255::port_b() | i8255::I8253_GATE2);

    // Read CPU clock counter
    TSC::Time_Stamp t0 = TSC::time_stamp();

    // Wait for i8253 counting to finish
    while(!(i8255::port_b() & i8255::I8253_OUT2));

    // Read CPU clock counter again
    TSC::Time_Stamp t1 = TSC::time_stamp(); // ascending

    // The measurement was for 50ms, scale it to 1s
    si->tm.cpu_clock = (t1 - t0) * 20;
    db<Setup>(INF) << "Setup::calibrate_timers:CPU clock=" << si->tm.cpu_clock / 1000000 << " MHz" << endl;

    // Disable speaker so we can use channel 2 of i8253
    i8255::port_b(i8255::port_b() & ~(i8255::SPEAKER | i8255::I8253_GATE2));

    // Program i8253 channel 2 to count 50 ms
    i8253::config(2, i8253::CLOCK/20, false, false);

    // Program APIC_Timer to count as long as it can
    APIC_Timer::config(0, APIC_Timer::Count(-1), false, false);

    // Enable i8253 channel 2 counting
    i8255::port_b(i8255::port_b() | i8255::I8253_GATE2);

    // Read APIC_Timer counter
    APIC_Timer::Count t3 = APIC_Timer::read(0); // descending

    // Wait for i8253 counting to finish
    while(!(i8255::port_b() & i8255::I8253_OUT2));

    // Read APIC_Timer counter again
    APIC_Timer::Count t2 = APIC_Timer::read(0);

    si->tm.bus_clock = (t3 - t2) * 20 * 16; // APIC_Timer is prescaled by 16
    db<Setup>(INF) << "Setup::calibrate_timers:BUS clock=" << si->tm.bus_clock / 1000000 << " MHz" << endl;
}

__END_SYS

using namespace EPOS::S;

//========================================================================
// _entry
//
// "_start" MUST BE PC_SETUP's first function, since PC_BOOT assumes
// offset "0" to be the entry point. It is a kind of bridge between the
// assembly world of PC_BOOT and the C++ world of PC_SETUP. It's main
// tasks are:
//
// - reload PC_SETUP from its ELF header (to the address it was compiled
//   for);
// - setup a stack for PC_SETUP (one for each CPU)
// - direct non-boot CPUs into the trampoline code inside PC_BOOT
//
// The initial stack pointer is inherited from PC_BOOT (i.e.,
// somewhere below 0x7c00).
//
// We can't "kout" here because the data segment is unreachable
// and "kout" has static data.
//
// THIS FUNCTION MUST BE RELOCATABLE, because it won't run at the
// address it has been compiled for.
//------------------------------------------------------------------------
void _entry()
{
    // Set EFLAGS
    CPU::flags(CPU::flags() & CPU::FLAG_CLEAR);

    // Disable interrupts
    CPU::int_disable();

    // Initialize the APIC (if present)
    APIC::reset(APIC::LOCAL_APIC_PHY_ADDR);

    // The boot strap loaded the boot image at BOOT_IMAGE_ADDR
    char * bi = reinterpret_cast<char *>(Traits<Machine>::IMAGE);

    // Get the System_Info (first thing in the boot image)
    System_Info * si = reinterpret_cast<System_Info *>(bi);

    // Check if we are booting from a ramdisk and adjust the boot image pointer accordingly
    if(si->bm.img_size > 2880 * 512) { // larger than a floppy
        bi = reinterpret_cast<char *>(Traits<Machine>::RAMDISK + Traits<Machine>::BOOT_LENGTH_MAX);
        si = reinterpret_cast<System_Info *>(bi);
    }

    // Multicore conditional start up
    if(APIC::id() == 0) { // Boot strap CPU (BSP)
        // Check SETUP integrity and get information about its ELF structure
        ELF * elf = reinterpret_cast<ELF *>(&bi[si->bm.setup_offset]);
        if(!elf->valid())
            Machine::panic();
        char * entry = reinterpret_cast<char *>(elf->entry());

        // Test if we can access the address for which SETUP has been compiled
        *entry = 'G';
        if(*entry != 'G')
            Machine::panic();

        // Load SETUP considering the address in the ELF header
        // Be careful: by reloading SETUP, global variables have been reset to
        // the values stored in the ELF data segment
        // Also check if this wouldn't destroy the boot image
        int size = elf->segment_size(0);
        if(elf->segment_address(0) <= reinterpret_cast<unsigned int>(&bi[si->bm.img_size]))
            Machine::panic();
        if(elf->load_segment(0) < 0)
            Machine::panic();
        APIC::remap(APIC::LOCAL_APIC_PHY_ADDR);

        // Move the boot image to after SETUP, so there will be nothing else below SETUP to be preserved
        // SETUP code + data + 1 stack per CPU
        register char * dst = MMU::align_page(entry + size + Traits<Machine>::CPUS * sizeof(MMU::Page));
        memcpy(dst, bi, si->bm.img_size);

        // Passes a pointer to the just allocated stack pool to other CPUs
        Stacks = dst;

        // Initialize shared CPU counter
        si->bm.n_cpus = 1;

        // Broadcast INIT IPI to all APs excluding self
        APIC::ipi_init(si->bm.cpu_status);

        // Broadcast STARTUP IPI to all APs excluding self
        // Non-boot CPUs will run a simplified boot strap just to
        // trampoline them into protected mode
        // PC_BOOT arranged for this code and stored it at 0x3000
        // ipi_start() waits for cpu_status to be incremented by the finc
        // further down in this code
        APIC::ipi_start(0x3000, si->bm.cpu_status);

        Stacks_Ready = true;

    } else { // Additional CPUs (APs)
        // Each AP increments the CPU counter
        CPU::finc(si->bm.n_cpus);

        // Inform BSP that this AP has been initialized
        CPU::finc(si->bm.cpu_status[APIC::id()]);

        // Wait for BSP's ACK
        while(si->bm.cpu_status[APIC::id()] != 2);

        if(APIC::id() >= int(Traits<Machine>::CPUS)) {
            db<Setup>(WRN) << "More CPUs were detected than the current " << "configuration supports (" << Traits<Machine>::CPUS << ")." << endl;
            db<Setup>(WRN) << "Disabling CPU " << APIC::id() << "!" << endl;

            CPU::int_disable();
            CPU::halt();
        }

        // Wait for the boot strap CPU to get us a stack
        while(!Stacks_Ready);
    }

    // Setup a single page stack for SETUP after its data segment
    // Boot strap CPU gets the highest address stack
    // SP = "entry" + "size" + #CPU * sizeof(Page)
    // Be careful: we'll loose our old stack now, so everything we still
    // need to reach Setup() must be in regs or globals!
    register char * sp = const_cast<char *>(Stacks) - sizeof(MMU::Page) * APIC::id();
    ASM("movl %0, %%esp" : : "r" (sp));

    // Pass the boot image to SETUP
    ASM("pushl %0" : : "r" (Stacks));

    // Call setup()
    // the assembly is necessary because the compiler generates
    // relative calls and we need an absolute one
    ASM("call *%0" : : "r" (&_setup));
}

void _setup(char * bi)
{
    if(!Traits<System>::multicore || (APIC::id() == 0)) {
        kerr  << endl;
        kout  << endl;
    }

    // Multicore sanity check
    if(!Traits<System>::multicore && (APIC::id() != 0)) {
        db<Setup>(WRN) << "Multicore disable by config, halting this CPU (" << APIC::id() << ")!" << endl;

        CPU::int_disable();
        CPU::halt();
    }

    Setup setup(bi);
}
