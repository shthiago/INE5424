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
            PRIVILEGED_READ_ONLY = (1 << 9) | (1 << 4),
            READ_ONLY = (1 << 9) | (1 << 5),
            // conjuntos de flags q implementam mem_types
            STRONGLY_ORDERED = 0,
            SHAREABLE_DEVICE = B | XN,
            NORMAL_1 = C, // Outer and Inner write-through, no allocate on write
            NORMAL_2 = B | C, // Outer and Inner write-back, no allocate on write
            NORMAL_3 = 1 << 6, // Outer and Inner non-cacheable
            NON_SHAREABLE_DEVICE = (1 << 7) | XN, // Outer and Inner non-cacheable
            // Mascara para ajudar a limpar o campo de flags de uma PTE
            MASK_PTE = (1 << 12) - 1,
            V    = 1 << 0, // Valid
            R    = 1 << 1, // Readable
            W    = 1 << 2, // Writable
            X    = 1 << 3, // Executable
            U    = 1 << 4, // User accessible
            MIO  = 1 << 9, // I/O (reserved for use by supervisor RSW)
            SYS  = (V | R | W | X),
            APP  = (V | R | W | X | U),
            IO   = (SYS | MIO),
            // Mascara para ajudar a limpar o campo de flags de uma PDE
            MASK_PDE = (1 << 10) - 1
        };

    public:
        Page_Flags() {}
        // converte um inteiro para uma flag específica
        Page_Flags(unsigned int f) : _flags(f) {}
        // converte uma flag da MMU_Common para uma flag das págs da arquitetura
        Page_Flags(Flags f) : _flags(P |
                                     ((f & Flags::RD)  ? 0 : 0) |
                                     ((f & Flags::RW)  ? RW_UMPREV : 0) |
                                     ((f & Flags::EX)  ? 0 : XN)|
                                     ((f & Flags::USR) ? UMPREV_ACCESS : 0) |
                                     ((f & Flags::CWT) ? NORMAL_1 : NORMAL_2) |
                                     ((f & Flags::CD)  ? NORMAL_3 : 0) |
                                     ((f & Flags::CT)  ? 0 : 0) |
                                     ((f & Flags::IO)  ? 0 : 0) |
                                     ((f & Flags::SYS) ? APX : 0)) {}

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

        
        // remapeia um segmento de memoria continuo para entradas de pt, esse segmento começa em addr
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
        // TODO: checar
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

    // Chunk (for Segment)
    // abstrai um segmento de memoria
    class Chunk
    {
    public:
        Chunk() {}

        Chunk(unsigned int bytes, Flags flags, Color color = WHITE)
        : _from(0),
          _to(pages(bytes)), // numero de pag necessarias para enderecar n bytes
          _pts(page_tables(_to - _from)), // numero de pag tables necessarias para enderecar n paginas
          _flags(Page_Flags(flags)), // flags do segmento
          _pt(calloc(_pts, WHITE)) // cria page table no qual esse segmento é mapeado
        {
            // n achei exatamente o que seriam as flags do ARMv7 que indicam mapeamento continuo.
            // sendo assim só deixei a opção de usar map para mapeamento de pags
            _pt->map(_from, _to, _flags, color);
        }

        Chunk(Phy_Addr phy_addr, unsigned int bytes, Flags flags)
        : _from(0),
          _to(pages(bytes)), // numero de pag necessarias para enderecar n bytes
          _pts(page_tables(_to - _from)), // numero de pag tables necessarias para enderecar n paginas
          _flags(Page_Flags(flags)), // flags do segmento
          _pt(calloc(_pts, WHITE)) // cria page table no qual esse segmento é mapeado
        {
            // mapeia um segmento de memória que começa em phy_addr, e tem n bytes de tamanho
            _pt->remap(phy_addr, _from, _to, flags);
        }

        // parece apenas criar uma estrutura Chunk para uma região que já foi mapeada. pt deve conter o end
        // do segmento que foi mapeado.
        Chunk(Phy_Addr pt, unsigned int from, unsigned int to, Flags flags)
        : _from(from),
          _to(to),
          _pts(page_tables(_to - _from)),
          _flags(flags),
          _pt(pt) {}

        ~Chunk() {
            // n tem flag para IO nem CT nessa implementacao
            for( ; _from < _to; _from++)
                free((*_pt)[_from]);
            free(_pt, _pts);
        }

        unsigned int pts() const { return _pts; }
        Page_Flags flags() const { return _flags; }
        Page_Table * pt() const { return _pt; }
        unsigned int size() const { return (_to - _from) * sizeof(Page); }

        
        // n tendi
        Phy_Addr phy_address() const {
            return (_flags & Flags::CT) ? Phy_Addr(indexes((*_pt)[_from])) : Phy_Addr(false);
        }

        // atualiza o tamanho de um chunk; para isso altera as entradas na pte que se referem a ele
        int resize(unsigned int amount) {

            unsigned int pgs = pages(amount);

            Color color = colorful ? phy2color(_pt) : WHITE;

            unsigned int free_pgs = _pts * PT_ENTRIES - _to;
            if(free_pgs < pgs) { // resize _pt
                unsigned int pts = _pts + page_tables(pgs - free_pgs);
                Page_Table * pt = calloc(pts, color);
                memcpy(phy2log(pt), phy2log(_pt), _pts * sizeof(Page));
                free(_pt, _pts);
                _pt = pt;
                _pts = pts;
            }
            _pt->map(_to, _to + pgs, _flags, color);
            _to += pgs;

            return pgs * sizeof(Page);
        }

    private:
        unsigned int _from;
        unsigned int _to;
        unsigned int _pts;
        Page_Flags _flags; // flags das paginas que compoe esse segmento de memoria
        Page_Table * _pt; // this is a physical address
    };

    // Page Directory
    typedef Page_Table Page_Directory;

    // Directory (for Address_Space)
    class Directory
    {
    public:
        Directory() : _pd(calloc(1, WHITE)), _free(true) {
            for(unsigned int i = directory(PHY_MEM); i < PD_ENTRIES; i++)
                (*_pd)[i] = (*_master)[i];
        }

        Directory(Page_Directory * pd) : _pd(pd), _free(false) {}

        ~Directory() { if(_free) free(_pd); }

        Phy_Addr pd() const { return _pd; }

        // ativa o address space representado por essa tabela de diretorio
        void activate() const { CPU::pdp(pd()); }

        // 'atacha' segmento de memoria a esse address space
        Log_Addr attach(const Chunk & chunk, unsigned int from = directory(APP_LOW)) {
            for(unsigned int i = from; i < PD_ENTRIES; i++)
                if(attach(i, chunk.pt(), chunk.pts(), chunk.flags()))
                    return i << DIRECTORY_SHIFT;
            return Log_Addr(false);
        }

        Log_Addr attach(const Chunk & chunk, Log_Addr addr) {
            unsigned int from = directory(addr);
            if(attach(from, chunk.pt(), chunk.pts(), chunk.flags()))
                return from << DIRECTORY_SHIFT;
            return Log_Addr(false);
        }

        void detach(const Chunk & chunk) {
            for(unsigned int i = 0; i < PD_ENTRIES; i++) {
                if(indexes(pte2phy((*_pd)[i])) == indexes(chunk.pt())) {
                    detach(i, chunk.pt(), chunk.pts());
                    return;
                }
            }
            db<MMU>(WRN) << "MMU::Directory::detach(pt=" << chunk.pt() << ") failed!" << endl;
        }

        void detach(const Chunk & chunk, Log_Addr addr) {
            unsigned int from = directory(addr);
            if(indexes(pte2phy((*_pd)[from])) != indexes(chunk.pt())) {
                db<MMU>(WRN) << "MMU::Directory::detach(pt=" << chunk.pt() << ",addr=" << addr << ") failed!" << endl;
                return;
            }
            detach(from, chunk.pt(), chunk.pts());
        }

        Phy_Addr physical(Log_Addr addr) {
            PD_Entry pde = (*_pd)[directory(addr)];
            Page_Table * pt = static_cast<Page_Table *>(pde2phy(pde));
            PT_Entry pte = pt->log()[page(addr)];
            return pte | offset(addr);
        }

    private:
        bool attach(unsigned int from, const Page_Table * pt, unsigned int n, Page_Flags flags) {
            for(unsigned int i = from; i < from + n; i++)
                if(_pd->log()[i])
                    return false;
            for(unsigned int i = from; i < from + n; i++, pt++)
                _pd->log()[i] = phy2pde(Phy_Addr(pt));
            return true;
        }

        void detach(unsigned int from, const Page_Table * pt, unsigned int n) {
            for(unsigned int i = from; i < from + n; i++)
                _pd->log()[i] = 0;
        }

    private:
        Page_Directory * _pd;  // this is a physical address, but operator*() returns a logical address
        bool _free;
    };

    // DMA_Buffer
    class DMA_Buffer;

    // Class Translation performs manual logical to physical address translations for debugging purposes only
    class Translation
    {
    public:
        Translation(Log_Addr addr, bool pt = false, Page_Directory * pd = 0): _addr(addr), _show_pt(pt), _pd(pd) {}

        friend OStream & operator<<(OStream & os, const Translation & t) {
            Page_Directory * pd = t._pd ? t._pd : current();
            PD_Entry pde = pd->log()[directory(t._addr)];
            Page_Table * pt = static_cast<Page_Table *>(pde2phy(pde));
            PT_Entry pte = pt->log()[page(t._addr)];

            os << "{addr=" << static_cast<void *>(t._addr) << ",pd=" << pd << ",pd[" << directory(t._addr) << "]=" << pde << ",pt=" << pt;
            if(t._show_pt)
                os << "=>" << pt->log();
            os << ",pt[" << page(t._addr) << "]=" << pte << ",f=" << pte2phy(pte) << ",*addr=" << hex << *static_cast<unsigned int *>(t._addr) << "}";
            return os;
        }

    private:
        Log_Addr _addr;
        bool _show_pt;
        Page_Directory * _pd;
    };

public:
    ARMv7_MMU() {}

    // aloca uma quantidade especifica de frames
    static Phy_Addr alloc(unsigned int frames = 1, Color color = WHITE) {
        Phy_Addr phy(false);

        if(frames) {
            List::Element * e = _free[color].search_decrementing(frames);
            if(e) {
                phy = e->object() + e->size();
                db<MMU>(TRC) << "MMU::alloc(frames=" << frames << ",color=" << color << ") => " << phy << endl;
            } else
                if(colorful)
                    db<MMU>(INF) << "MMU::alloc(frames=" << frames << ",color=" << color << ") => failed!" << endl;
                else
                    db<MMU>(WRN) << "MMU::alloc(frames=" << frames << ",color=" << color << ") => failed!" << endl;
        }

        return phy;
    }
    // zera e aloca uma quantidade especifica de frames
    static Phy_Addr calloc(unsigned int frames = 1, Color color = WHITE) {
        Phy_Addr phy = alloc(frames, color);
        memset(phy2log(phy), 0, sizeof(Frame) * frames);
        return phy;
    }
    // adiciona o espaço ocupado por n frames de volta a lista de mem livre
    static void free(Phy_Addr frame, int n = 1) {
        // Clean up MMU flags in frame address
        frame = indexes(frame);
        Color color = colorful ? phy2color(frame) : WHITE;

        db<MMU>(TRC) << "MMU::free(frame=" << frame << ",color=" << color << ",n=" << n << ")" << endl;

        if(frame && n) {
            List::Element * e = new (phy2log(frame)) List::Element(frame, n);
            List::Element * m1, * m2;
            _free[color].insert_merging(e, &m1, &m2);
        }
    }
    static void white_free(Phy_Addr frame, int n) {
        // Clean up MMU flags in frame address
        frame = indexes(frame);

        db<MMU>(TRC) << "MMU::free(frame=" << frame << ",color=" << WHITE << ",n=" << n << ")" << endl;

        if(frame && n) {
            List::Element * e = new (phy2log(frame)) List::Element(frame, n);
            List::Element * m1, * m2;
            _free[WHITE].insert_merging(e, &m1, &m2);
        }
    }
    static unsigned int allocable(Color color = WHITE) { return _free[color].head() ? _free[color].head()->size() : 0; }

    // retorna a tabela de dir atual
    static Page_Directory * volatile current() { return reinterpret_cast<Page_Directory * volatile>(CPU::pdp()); }
    // retorna a traducao completa de um end logico para fisico
    // TODO adapt to 1MB pages if we need to modify
    static Phy_Addr physical(Log_Addr addr) {
        Page_Directory * pd = current();
        Page_Table * pt = pd->log()[directory(addr)];
        return pt->log()[page(addr)] | offset(addr);
    }
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
    static Phy_Addr pde2phy(PD_Entry entry) { return entry & ~Page_Flags::MASK_PDE; }

    static void flush_tlb() { /* TODO */ }
    static void flush_tlb(Log_Addr addr) { /* TODO */}

    //converte end fisico para color
    static Log_Addr phy2log(Phy_Addr phy) { return Log_Addr((MEM_BASE == PHY_MEM) ? phy : (MEM_BASE > PHY_MEM) ? phy - (MEM_BASE - PHY_MEM) : phy + (PHY_MEM - MEM_BASE)); }
    // converte end log para fisico
    static Phy_Addr log2phy(Log_Addr log) { return Phy_Addr((MEM_BASE == PHY_MEM) ? log : (MEM_BASE > PHY_MEM) ? log + (MEM_BASE - PHY_MEM) : log - (PHY_MEM - MEM_BASE)); }
    // converte de end fisico para end log com color
    static Color phy2color(Phy_Addr phy) { return WHITE ;} // Ever white
    // converte de end log para end log com color
    static Color log2color(Log_Addr log) { return WHITE; } // Ever white

private:
    static void init();

private:
    static List _free[colorful * COLORS + 1]; // +1 for WHITE
    static Page_Directory * _master;
};

class MMU: public IF<Traits<System>::multitask, ARMv7_MMU, No_MMU>::Result {};

__END_SYS

#endif
