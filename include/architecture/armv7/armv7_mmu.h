// EPOS ARMv7 MMU Mediator Declarations

#ifndef __armv7_mmu_h
#define __armv7_mmu_h

#include <architecture/mmu.h>
#include <system/memory_map.h>

__BEGIN_SYS
// 12 bits para codificar 4096(2**12) entradas na pd
// 8 bits para codificar 256(2**8) entradas nas pts
// 12 bits para endereçar 4096(2**12) bytes nas paginas

class ARMv7_MMU: public MMU_Common<12, 8, 12>
{
    friend class CPU;

private:
    typedef Grouping_List<Frame> List;

    static const bool colorful = Traits<MMU>::colorful;
    static const unsigned int COLORS = Traits<MMU>::COLORS;
    static const unsigned int MEM_BASE = Memory_Map::MEM_BASE;
    static const unsigned int APP_LOW = Memory_Map::APP_LOW;
    static const unsigned int PHY_MEM = Memory_Map::PHY_MEM;

public:
    // Page Flags
    class Page_Flags
    {
    // pdf ref: https://moodle.ufsc.br/pluginfile.php/4417210/mod_resource/content/1/DEN0013D_cortex_a_series_PG.pdf
    // pags 143~145(em numeracao de pg do pdf)
    // outro link interessante: https://sudonull.com/post/11570-Virtual-memory-in-ARMv7
    public:
        enum {
            XN    = 1 << 0, // Execute Never(1=n deve executar, 0=deve executar)
            P     = 1 << 1, // Ainda n entendi mto bem; se baseando em small pages, deve ser present????
            B     = 1 << 2, // bufferable bit(1=bufferable, 0=no bufferable)
            C     = 1 << 3, // cacheable bit(1=cacheable, 0 =no cacheable)
            APX   = 1 << 9, // trabalha em conjunto com bits do campo AP para definir permissões; para prev modes(0=read&write, 1=read)
            // as duas flags a seguir dizem respeito ao acesso de applicacoes umprev a regioes de mem
            UMPREV_ACCESS   = 1 << 5, // umprev mode tem acesso; corresponte ao segundo bit(dir para esquerda) de AP
            RW_UMPREV = 1 << 4, // para umprev mode(0=read, 1=read&write);
            S     = 1 << 10, // Shareability (1=compartilhavel; 0=n compartilhavel)
            NG    = 1 << 11, // non Global bit. (0=acessivel a todos os procs, 1=regiao pertencente a um proc especifico)
            // conjuntos de flags q implementam permissoes
            PRIVILED_ACCESS_ONLY = 1 << 4,
            NO_USER_MODE_WRITE = 1 << 5,
            FULL_ACCESS = (1 << 4) | (1 << 5),
            PRIVILEGED_READ_ONLY = (1 << 9) | (1 << 4)),
            READ_ONLY = (1 << 9) | (1 << 5)),
            // conjuntos de flags q implementam mem_types
            STRONGLY_ORDERED = 0,
            SHAREABLE_DEVICE = B | XN,
            NORMAL_1 = C, // Outer and Inner write-through, no allocate on write
            NORMAL_2 = B | C, // Outer and Inner write-back, no allocate on write
            NORMAL_3 = 1 << 6, // Outer and Inner non-cacheable
            NON_SHAREABLE_DEVICE = (1 << 7) | XN, // Outer and Inner non-cacheable
            // Mascara para ajudar a limpar o campo de flags de uma PTE
            MASK_PTE = (1 << 12) - 1
            // Mascara para ajudar a limpar o campo de flags de uma PDE
            MASK_PDE = (1 << 10) - 1
        };

    public:
        Page_Flags() {}
        // converte um inteiro para uma flag específica
        Page_Flags(unsigned int f) : _flags(f) {}
        // converte uma flag da MMU_Common para uma flag das págs da arquitetura
        Page_Flags(Flags f) : _flags(V |
                                     ((f & Flags::RD)  ? 0 : 0) |
                                     ((f & Flags::RW)  ? RW_UMPREV : 0) |
                                     ((f & Flags::EX)  ? 0 : XN)|
                                     ((f & Flags::USR) ? UMPREV_ACCESS : 0) |
                                     ((f & Flags::CWT) ? NORMAL_1 : NORMAL_2) |
                                     ((f & Flags::CD)  ? NORMAL_3 : 0) |
                                     ((f & Flags::CT)  ? 0 : 0) |
                                     ((f & Flags::IO)  ? 0 : 0) |
                                     ((F & Flags::SYS) ? APX : 0)) {}

        operator unsigned int() const { return _flags; }

        friend Debug & operator<<(Debug & db, const Page_Flags & f) { db << hex << f._flags; return db; }

    private:
        unsigned int _flags;
    };

    // Page_Table
    class Page_Table
    {
    public:
        Page_Table() {}

        // retorna um indice na tab de pags
        PT_Entry & operator[](unsigned int i) { return _entry[i]; }
        // retorna o end logico dessa tabela de pag
        Page_Table & log() { return *static_cast<Page_Table *>(phy2log(this)); }

        // mapeia uma quantidade (to - from) de paginas nessa tabela
        void map(int from, int to, Page_Flags flags, Color color) {
            // retorna um ponteiro para a area alocada
            Phy_Addr * addr = alloc(to - from, color);
            // se for diferente de 0, significa que uma região de memória continua foi alocada
            if(addr)
                remap(addr, from, to, flags);
            // se n alocou com sucesso, percorre as tabela
            else
                for( ; from < to; from++) {
                    // converte o end de uma entrada da tab de pag para logico
                    Log_Addr * pte = phy2log(&_entry[from]);
                    // aloca 1 frame(pag), converte o seu end para entrada de pt e armazena na pte obtida na linha anterior
                    *pte = phy2pte(alloc(1, color), flags);
                }
        }

        // mapeia uma região continua da memória
        void map_contiguous(int from, int to, Page_Flags flags, Color color) {
            remap(alloc(to - from, color), from, to, flags);
        }

        
        // remapeia um segmento de memoria continuo para entradas de pt
        void remap(Phy_Addr addr, int from, int to, Page_Flags flags) {
            // alinha o endereço base da memoria alocada
            addr = align_page(addr);
            for( ; from < to; from++) {
                // converte o end de uma entrada da tab de pag para logico
                Log_Addr * pte = phy2log(&_entry[from]);
                // adiciona o end base atual(addr) da memoria alocada na pte obtida na linha anterior
                *pte = phy2pte(addr, flags);
                // soma o tamanho de uma pagina ao end base(addr). Isso garante que a memoria alocada seja enderaçada em tamanhos
                // multiplos do tamanho de uma pagina
                addr += sizeof(Page);
            }
        }

        // libera entradas da tab de pags
        void unmap(int from, int to) {
            for( ; from < to; from++) {
                free(_entry[from]);
                Log_Addr * pte = phy2log(&_entry[from]);
                *pte = 0;
            }
        }

        // utilizado para imprimir entradas de uma tabela(acho)
        friend OStream & operator<<(OStream & os, Page_Table & pt) {
            os << "{\n";
            int brk = 0;
            for(unsigned int i = 0; i < PT_ENTRIES; i++)
                if(pt[i]) {
                    os << "[" << i << "]=" << pte2phy(pt[i]) << "  ";
                    if(!(++brk % 4))
                        os << "\n";
                }
            os << "\n}";
            return os;
        }

    private:
        PT_Entry _entry[PT_ENTRIES]; // the Phy_Addr in each entry passed through phy2pte()
    };


    // Page Directory
    class Page_Directory;

    // Chunk (for Segment)
    class Chunk;

    // Directory (for Address_Space)
    class Directory;

    // DMA_Buffer
    class DMA_Buffer;

    // Class Translation performs manual logical to physical address translations for debugging purposes only
    class Translation;

public:
    ARMv7_MMU() {}

    // aloca uma quantidade especifica de frames
    static Phy_Addr alloc(unsigned int frames = 1, Color color = WHITE);
    // zera e aloca uma quantidade especifica de frames
    static Phy_Addr calloc(unsigned int frames = 1, Color color = WHITE);
    // adiciona o espaço ocupado por n frames de volta a lista de mem livre
    static void free(Phy_Addr frame, int n = 1);
    static void white_free(Phy_Addr frame, int n);
    static unsigned int allocable(Color color = WHITE);

    // retorna a tabela de dir atual
    static Page_Directory * volatile current();
    // retorna a traducao completa de um end logico para fisico
    static Phy_Addr physical(Log_Addr addr);
    // converte end físico para pte, aplicando as flags passadas como parametro. Considera que o end fisico está alinhado corretamente
    static PT_Entry phy2pte(Phy_Addr frame, Page_Flags flags) { return frame | flags; }
    // converte end de pte(page table entry) para fisico
    static Phy_Addr pte2phy(PT_Entry entry){ return entry & ~Page_Flags::MASK_PTE; }
    // convert end fisico para pde(page directory entry)
    // até o momento o uso de dóminios n se mostrou necessário, logo esse campo será sempre tratado
    // como zeros. o bit 9(P - present?) e o bit 0(responsável por dizer q a entrada é um pointer para 2 level pte) são setados.
    // Considera q o end fisico esta alinhado corretamente
    static PD_Entry phy2pde(Phy_Addr frame) { return frame | (1 << 9) | (1 << 0); }
    // converte end de pde(page directory entry) para fisico
    static Phy_Addr pde2phy(PD_Entry entry); { return entry & ~Page_Flags::MASK_PDE; }

    static void flush_tlb();
    static void flush_tlb(Log_Addr addr);

    //converte end fisico para color
    static Log_Addr phy2log(Phy_Addr phy) { return Log_Addr((MEM_BASE == PHY_MEM) ? phy : (MEM_BASE > PHY_MEM) ? phy - (MEM_BASE - PHY_MEM) : phy + (PHY_MEM - MEM_BASE)); }
    // converte end log para fisico
    static Phy_Addr log2phy(Log_Addr log) { return Phy_Addr((MEM_BASE == PHY_MEM) ? log : (MEM_BASE > PHY_MEM) ? log + (MEM_BASE - PHY_MEM) : log - (PHY_MEM - MEM_BASE)); }
    // converte de end fisico para end log com color
    static Color phy2color(Phy_Addr phy);
    // converte de end log para end log com color
    static Color log2color(Log_Addr log);

private:
    static void init();

private:
    static List _free[colorful * COLORS + 1]; // +1 for WHITE
    static Page_Directory * _master;
};

class MMU: public IF<Traits<System>::multitask, ARMv7_MMU, No_MMU>::Result {};

__END_SYS

#endif
