#ifndef __message
#define __message

__BEGIN_SYS

enum SyscallType {
    CREATE_THREAD_1,

    THREAD_EXIT_NO_ARG,
    THREAD_EXIT_ARG,

    PRINT,
};

class SyscallMessage {
private:
    static const unsigned int MAX_PARAMETERS_SIZE = 256;

public:
    SyscallMessage(const char * text): _text(text) { }
    template<typename ... Tn>
    SyscallMessage(Tn && ... an) { out(an ...); }

    SyscallType type() { return _type; }
    void type(SyscallType type) { _type = type; }

    const char * text() {return _text; }

    template<typename ... Tn>
    void in(Tn && ... an) {
        typename IF<(SIZEOF<Tn ...>::Result <= MAX_PARAMETERS_SIZE), int, void>::Result index = 0;
        DESERIALIZE(_parms, index, an ...);
    }

    template<typename ... Tn>
    void out(const Tn & ... an) {
        typename IF<(SIZEOF<Tn ...>::Result <= MAX_PARAMETERS_SIZE), int, void>::Result index = 0;
        SERIALIZE(_parms, index, an ...);
    }

private:
    SyscallType _type;
    const char * _text;

    char _parms[MAX_PARAMETERS_SIZE];
};

__END_SYS

#endif // __message