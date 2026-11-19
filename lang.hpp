// ============================================================================
//  FlowyEditor - cross-platform text editor on SDL2 + SDL2_ttf
//  Targets: Windows / Linux / macOS / Android / iOS (one source file)
//  Languages: C, C++, NASM, Lua (+ plain text)
// ============================================================================
#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <atomic>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <filesystem>
#include <mutex>
#include <set>
#include <thread>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(__ANDROID__) || defined(__IPHONEOS__) || defined(__IOS__)
#define FLOWY_MOBILE 1
#else
#define FLOWY_MOBILE 0
#endif

using U32 = std::u32string;
static int TABW = 4;
static bool g_autoClose = true;
static const size_t NPOS = std::string::npos;

// ---------------------------------------------------------------- UTF-8 ----
static U32 toU32(const std::string& s) {
    U32 o; o.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i]; char32_t cp; size_t n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c >> 5) == 6) { cp = c & 0x1F; n = 2; }
        else if ((c >> 4) == 14) { cp = c & 0x0F; n = 3; }
        else if ((c >> 3) == 30) { cp = c & 0x07; n = 4; }
        else { cp = 0xFFFD; n = 1; }
        if (n > 1) {
            if (i + n > s.size()) { cp = 0xFFFD; n = 1; }
            else for (size_t k = 1; k < n; k++) cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        }
        o.push_back(cp); i += n;
    }
    return o;
}
static std::string toU8(const U32& s) {
    std::string o;
    for (char32_t c : s) {
        if (c < 0x80) o += (char)c;
        else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
        else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
    }
    return o;
}
static bool isDig(char32_t c) { return c >= '0' && c <= '9'; }
static bool isIdS(char32_t c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 128; }
static bool isIdC(char32_t c) { return isIdS(c) || isDig(c); }
static std::string lowerS(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static std::string narrow(const U32& s, size_t a, size_t b, bool lc = false) {
    std::string r;
    for (size_t i = a; i < b && i < s.size(); i++) {
        char ch = s[i] < 128 ? (char)s[i] : '?';
        if (lc) ch = (char)tolower((unsigned char)ch);
        r += ch;
    }
    return r;
}

// ------------------------------------------------------- language data ----
typedef int Lang;
enum { L_TEXT, L_C, L_CPP, L_NASM, L_LUA, L_BUILTIN };
enum { LS_TEXT, LS_C, LS_CPP, LS_NASM, LS_LUA, LS_GENERIC };
enum Kind { K_KW, K_TYPE, K_FUNC, K_WORD, K_SNIP, K_CONST };
struct Item { std::string name, sig; Kind kind; std::string body; };
struct StrRule { std::string start, end, esc; bool multi = false; };
struct LineRule { std::string prefix; int tok = 1; };
struct LangDef {
    std::string name; std::vector<std::string> exts; int special = LS_GENERIC;
    bool ci = false, multi = false, callParens = true, keyColon = false, hashNum = false, markup = false;
    std::vector<Item> items;
    std::unordered_set<std::string> kw, type, func, reg, dir, cst;
    std::vector<std::string> lineC, indentAfter;
    std::vector<std::pair<std::string, std::string>> blockC;
    std::vector<StrRule> strs; std::vector<LineRule> lineStart;
    std::string preproc, annotation, variable, idExtra, wordChars, run, pairs = "()[]{}\"\"''", dedent;
};
static std::vector<LangDef> LANGS;
struct Snippet { std::string lang, prefix, desc, body; };
static std::vector<Snippet> g_snippets;

static void addWords(LangDef& d, Kind k, const char* list, std::unordered_set<std::string>* extra = nullptr) {
    std::istringstream ss(list); std::string w;
    while (ss >> w) { d.items.push_back({w, "", k}); if (extra) extra->insert(w); }
}
static void addFuncs(LangDef& d, const char* const* t) {
    for (; *t; ++t) {
        std::string s = *t; size_t p = s.find('|');
        d.items.push_back({s.substr(0, p), p == NPOS ? "" : s.substr(p + 1), K_FUNC});
    }
}
static void finalize(LangDef& d, bool lc) {
    d.kw.clear(); d.type.clear(); d.func.clear(); d.cst.clear();
    for (auto& it : d.items) {
        std::string n = lc ? lowerS(it.name) : it.name;
        if (it.kind == K_KW) d.kw.insert(n); else if (it.kind == K_TYPE) d.type.insert(n);
        else if (it.kind == K_FUNC) d.func.insert(n); else if (it.kind == K_CONST) d.cst.insert(n);
    }
}

static const char* C_FUNCS[] = {
 "printf|int printf(const char *fmt, ...)", "fprintf|int fprintf(FILE *f, const char *fmt, ...)",
 "sprintf|int sprintf(char *buf, const char *fmt, ...)", "snprintf|int snprintf(char *buf, size_t n, const char *fmt, ...)",
 "scanf|int scanf(const char *fmt, ...)", "sscanf|int sscanf(const char *s, const char *fmt, ...)",
 "fscanf|int fscanf(FILE *f, const char *fmt, ...)", "puts|int puts(const char *s)", "fputs|int fputs(const char *s, FILE *f)",
 "putchar|int putchar(int c)", "getchar|int getchar(void)", "fgets|char *fgets(char *buf, int n, FILE *f)",
 "fgetc|int fgetc(FILE *f)", "fputc|int fputc(int c, FILE *f)", "getc|int getc(FILE *f)", "putc|int putc(int c, FILE *f)",
 "ungetc|int ungetc(int c, FILE *f)", "fopen|FILE *fopen(const char *path, const char *mode)", "fclose|int fclose(FILE *f)",
 "fread|size_t fread(void *p, size_t sz, size_t n, FILE *f)", "fwrite|size_t fwrite(const void *p, size_t sz, size_t n, FILE *f)",
 "fseek|int fseek(FILE *f, long off, int whence)", "ftell|long ftell(FILE *f)", "rewind|void rewind(FILE *f)",
 "fflush|int fflush(FILE *f)", "feof|int feof(FILE *f)", "ferror|int ferror(FILE *f)", "remove|int remove(const char *path)",
 "rename|int rename(const char *a, const char *b)", "perror|void perror(const char *s)", "setvbuf|int setvbuf(FILE *f, char *buf, int mode, size_t n)",
 "malloc|void *malloc(size_t n)", "calloc|void *calloc(size_t n, size_t sz)", "realloc|void *realloc(void *p, size_t n)",
 "free|void free(void *p)", "exit|void exit(int code)", "abort|void abort(void)", "atexit|int atexit(void (*fn)(void))",
 "atoi|int atoi(const char *s)", "atol|long atol(const char *s)", "atof|double atof(const char *s)",
 "strtol|long strtol(const char *s, char **end, int base)", "strtoul|unsigned long strtoul(const char *s, char **end, int base)",
 "strtod|double strtod(const char *s, char **end)", "rand|int rand(void)", "srand|void srand(unsigned seed)",
 "qsort|void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void*, const void*))",
 "bsearch|void *bsearch(const void *key, const void *base, size_t n, size_t sz, cmp)", "abs|int abs(int x)", "labs|long labs(long x)",
 "getenv|char *getenv(const char *name)", "system|int system(const char *cmd)",
 "memcpy|void *memcpy(void *dst, const void *src, size_t n)", "memmove|void *memmove(void *dst, const void *src, size_t n)",
 "memset|void *memset(void *p, int c, size_t n)", "memcmp|int memcmp(const void *a, const void *b, size_t n)",
 "memchr|void *memchr(const void *p, int c, size_t n)", "strlen|size_t strlen(const char *s)",
 "strcpy|char *strcpy(char *dst, const char *src)", "strncpy|char *strncpy(char *dst, const char *src, size_t n)",
 "strcat|char *strcat(char *dst, const char *src)", "strncat|char *strncat(char *dst, const char *src, size_t n)",
 "strcmp|int strcmp(const char *a, const char *b)", "strncmp|int strncmp(const char *a, const char *b, size_t n)",
 "strchr|char *strchr(const char *s, int c)", "strrchr|char *strrchr(const char *s, int c)",
 "strstr|char *strstr(const char *h, const char *n)", "strtok|char *strtok(char *s, const char *delim)",
 "strdup|char *strdup(const char *s)", "strerror|char *strerror(int errnum)", "strspn|size_t strspn(const char *s, const char *a)",
 "strcspn|size_t strcspn(const char *s, const char *r)", "strpbrk|char *strpbrk(const char *s, const char *a)",
 "isalpha|int isalpha(int c)", "isdigit|int isdigit(int c)", "isalnum|int isalnum(int c)", "isspace|int isspace(int c)",
 "isupper|int isupper(int c)", "islower|int islower(int c)", "ispunct|int ispunct(int c)", "isxdigit|int isxdigit(int c)",
 "toupper|int toupper(int c)", "tolower|int tolower(int c)",
 "sin|double sin(double x)", "cos|double cos(double x)", "tan|double tan(double x)", "asin|double asin(double x)",
 "acos|double acos(double x)", "atan|double atan(double x)", "atan2|double atan2(double y, double x)", "sinh|double sinh(double x)",
 "cosh|double cosh(double x)", "tanh|double tanh(double x)", "exp|double exp(double x)", "log|double log(double x)",
 "log10|double log10(double x)", "log2|double log2(double x)", "pow|double pow(double x, double y)", "sqrt|double sqrt(double x)",
 "cbrt|double cbrt(double x)", "fabs|double fabs(double x)", "floor|double floor(double x)", "ceil|double ceil(double x)",
 "round|double round(double x)", "trunc|double trunc(double x)", "fmod|double fmod(double x, double y)", "hypot|double hypot(double x, double y)",
 "time|time_t time(time_t *t)", "clock|clock_t clock(void)", "difftime|double difftime(time_t a, time_t b)",
 "strftime|size_t strftime(char *s, size_t n, const char *fmt, const struct tm *tm)",
 "localtime|struct tm *localtime(const time_t *t)", "gmtime|struct tm *gmtime(const time_t *t)", "mktime|time_t mktime(struct tm *tm)",
 "assert|void assert(expr)", "setjmp|int setjmp(jmp_buf env)", "longjmp|void longjmp(jmp_buf env, int val)",
 "signal|void (*signal(int sig, void (*h)(int)))(int)", "va_start|void va_start(va_list ap, last)", "va_arg|type va_arg(va_list ap, type)",
 "va_end|void va_end(va_list ap)", "vprintf|int vprintf(const char *fmt, va_list ap)", "vsnprintf|int vsnprintf(char *b, size_t n, const char *fmt, va_list ap)",
 nullptr};

static const char* CPP_FUNCS[] = {
 "std::sort|void sort(It first, It last [, cmp])", "std::find|It find(It first, It last, const T& v)",
 "std::find_if|It find_if(It first, It last, pred)", "std::min|const T& min(const T& a, const T& b)",
 "std::max|const T& max(const T& a, const T& b)", "std::swap|void swap(T& a, T& b)", "std::move|T&& move(T& x)",
 "std::forward|T&& forward(T& x)", "std::make_unique|unique_ptr<T> make_unique<T>(args...)",
 "std::make_shared|shared_ptr<T> make_shared<T>(args...)", "std::make_pair|pair<A,B> make_pair(A a, B b)",
 "std::to_string|string to_string(number)", "std::stoi|int stoi(const string& s)", "std::stod|double stod(const string& s)",
 "std::getline|istream& getline(istream& in, string& s)", "std::accumulate|T accumulate(It first, It last, T init)",
 "std::reverse|void reverse(It first, It last)", "std::count|size_t count(It first, It last, const T& v)",
 "std::transform|It transform(It first, It last, It out, op)", "std::for_each|void for_each(It first, It last, fn)",
 "std::lower_bound|It lower_bound(It first, It last, const T& v)", "std::unique|It unique(It first, It last)",
 "std::this_thread::sleep_for|void sleep_for(duration)", "std::printf|int printf(const char *fmt, ...)", nullptr};

static const char* LUA_FUNCS[] = {
 "assert|assert(v [, message])", "collectgarbage|collectgarbage([opt [, arg]])", "dofile|dofile([filename])", "error|error(message [, level])",
 "getmetatable|getmetatable(object)", "ipairs|ipairs(t)", "load|load(chunk [, chunkname [, mode [, env]]])", "loadfile|loadfile([filename])",
 "next|next(table [, index])", "pairs|pairs(t)", "pcall|pcall(f [, arg1, ...])", "print|print(...)", "rawequal|rawequal(v1, v2)",
 "rawget|rawget(table, index)", "rawlen|rawlen(v)", "rawset|rawset(table, index, value)", "require|require(modname)",
 "select|select(n, ...)", "setmetatable|setmetatable(table, metatable)", "tonumber|tonumber(e [, base])", "tostring|tostring(v)",
 "type|type(v)", "unpack|unpack(list [, i [, j]])", "xpcall|xpcall(f, msgh [, arg1, ...])",
 "string.byte|string.byte(s [, i [, j]])", "string.char|string.char(...)", "string.find|string.find(s, pattern [, init [, plain]])",
 "string.format|string.format(fmt, ...)", "string.gmatch|string.gmatch(s, pattern)", "string.gsub|string.gsub(s, pattern, repl [, n])",
 "string.len|string.len(s)", "string.lower|string.lower(s)", "string.match|string.match(s, pattern [, init])",
 "string.rep|string.rep(s, n [, sep])", "string.reverse|string.reverse(s)", "string.sub|string.sub(s, i [, j])", "string.upper|string.upper(s)",
 "table.concat|table.concat(list [, sep [, i [, j]]])", "table.insert|table.insert(list, [pos,] value)", "table.move|table.move(a1, f, e, t [, a2])",
 "table.pack|table.pack(...)", "table.remove|table.remove(list [, pos])", "table.sort|table.sort(list [, comp])", "table.unpack|table.unpack(list [, i [, j]])",
 "math.abs|math.abs(x)", "math.ceil|math.ceil(x)", "math.floor|math.floor(x)", "math.sqrt|math.sqrt(x)", "math.sin|math.sin(x)",
 "math.cos|math.cos(x)", "math.tan|math.tan(x)", "math.exp|math.exp(x)", "math.log|math.log(x [, base])", "math.max|math.max(x, ...)",
 "math.min|math.min(x, ...)", "math.fmod|math.fmod(x, y)", "math.random|math.random([m [, n]])", "math.randomseed|math.randomseed(x)",
 "math.tointeger|math.tointeger(x)", "math.type|math.type(x)", "os.time|os.time([table])", "os.clock|os.clock()", "os.date|os.date([format [, time]])",
 "os.exit|os.exit([code [, close]])", "os.getenv|os.getenv(varname)", "os.remove|os.remove(filename)", "os.rename|os.rename(old, new)",
 "os.tmpname|os.tmpname()", "os.difftime|os.difftime(t2, t1)", "io.open|io.open(filename [, mode])", "io.close|io.close([file])",
 "io.read|io.read(...)", "io.write|io.write(...)", "io.lines|io.lines([filename, ...])", "io.input|io.input([file])", "io.output|io.output([file])",
 "coroutine.create|coroutine.create(f)", "coroutine.resume|coroutine.resume(co [, val1, ...])", "coroutine.yield|coroutine.yield(...)",
 "coroutine.status|coroutine.status(co)", "coroutine.wrap|coroutine.wrap(f)", "coroutine.isyieldable|coroutine.isyieldable()",
 "coroutine.running|coroutine.running()", nullptr};

static void initLangs() {
    LANGS.assign(L_BUILTIN, LangDef()); g_snippets.clear();
    const char* nm[] = {"Text", "C", "C++", "NASM", "Lua"};
    for (int i = 0; i < L_BUILTIN; i++) { LANGS[i].name = nm[i]; LANGS[i].special = i; }
    LANGS[L_TEXT].pairs = "";
    // ---- C
    LangDef& c = LANGS[L_C];
    addWords(c, K_KW, "auto break case const continue default do else enum extern for goto if inline register restrict return sizeof static struct switch typedef union volatile while NULL true false #include #define #undef #if #ifdef #ifndef #else #elif #endif #pragma #error");
    addWords(c, K_TYPE, "int char short long float double void signed unsigned size_t ssize_t FILE bool _Bool int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t intptr_t uintptr_t ptrdiff_t wchar_t time_t clock_t va_list jmp_buf");
    addFuncs(c, C_FUNCS);
    finalize(c, false);
    // ---- C++ (superset)
    LangDef& p = LANGS[L_CPP];
    p.items = c.items;
    addWords(p, K_KW, "alignas alignof and asm catch class concept consteval constexpr constinit const_cast co_await co_return co_yield decltype delete dynamic_cast explicit export final friend mutable namespace new noexcept nullptr operator or override private protected public reinterpret_cast requires static_assert static_cast template this thread_local throw try typeid typename using virtual not xor");
    addWords(p, K_TYPE, "char8_t char16_t char32_t std::string std::wstring std::vector std::map std::unordered_map std::set std::unordered_set std::list std::deque std::array std::pair std::tuple std::optional std::variant std::function std::unique_ptr std::shared_ptr std::weak_ptr std::thread std::mutex std::atomic std::cout std::cin std::cerr std::endl std::ostream std::istream std::ifstream std::ofstream std::stringstream std::string_view std::size_t std::nullptr_t auto");
    addFuncs(p, CPP_FUNCS);
    p.kw.clear(); p.type.clear(); p.func.clear();
    finalize(p, false);
    // ---- Lua
    LangDef& l = LANGS[L_LUA];
    addWords(l, K_KW, "and break do else elseif end false for function goto if in local nil not or repeat return then true until while");
    addWords(l, K_TYPE, "string table math os io coroutine debug utf8 package self _G _VERSION _ENV");
    addFuncs(l, LUA_FUNCS);
    finalize(l, false);
    // ---- NASM
    LangDef& a = LANGS[L_NASM];
    const char* instr = "mov push pop lea add sub mul imul div idiv inc dec neg not and or xor shl shr sal sar rol ror rcl rcr shld shrd cmp test jmp call ret retn retf leave enter nop int int3 into iret syscall sysenter sysexit hlt cli sti cld std clc stc cmc movzx movsx movsxd cbw cwd cdq cqo cwde cdqe xchg loop loope loopne rep repe repne repz repnz movsb movsw movsd movsq stosb stosw stosd stosq lodsb lodsw lodsd lodsq scasb scasw scasd scasq cmpsb cmpsw cmpsd cmpsq bt bts btr btc bsf bsr popcnt lzcnt tzcnt bswap adc sbb cmpxchg cmpxchg8b cmpxchg16b xadd lock pushf popf pushfq popfq pusha popa lahf sahf cpuid rdtsc rdtscp rdrand ud2 mfence lfence sfence pause xlat andn bextr blsi blsr bzhi pdep pext movaps movups movapd movupd movdqa movdqu movss movd movq movlps movhps addps addpd addss addsd subps subpd subss subsd mulps mulpd mulss mulsd divps divpd divss divsd sqrtps sqrtss sqrtsd minps maxps xorps xorpd andps andnps orps pxor por pand pandn paddb paddw paddd paddq psubb psubw psubd psubq pmullw pmulld pshufd pshufb punpcklbw punpckhbw cvtsi2sd cvtsi2ss cvttsd2si cvttss2si cvtss2sd cvtsd2ss comiss comisd ucomiss ucomisd vmovaps vmovups vmovdqa vmovdqu vaddps vsubps vmulps vdivps vxorps vpxor fld fst fstp fild fist fistp fadd fsub fmul fdiv fxch fchs fabs fsqrt fsin fcos fld1 fldz fldpi fcom fcomp fcomi fcomip finit fwait fnstcw fldcw";
    addWords(a, K_KW, instr);
    {   // conditional families
        const char* conds[] = {"o","no","b","c","nae","ae","nb","nc","e","z","ne","nz","be","na","a","nbe","s","ns","p","pe","np","po","l","nge","ge","nl","le","ng","g","nle"};
        for (auto cc : conds) {
            a.items.push_back({std::string("j") + cc, "", K_KW});
            a.items.push_back({std::string("cmov") + cc, "", K_KW});
            a.items.push_back({std::string("set") + cc, "", K_KW});
        }
        a.items.push_back({"jcxz", "", K_KW}); a.items.push_back({"jecxz", "", K_KW}); a.items.push_back({"jrcxz", "", K_KW});
    }
    addWords(a, K_KW, "", nullptr);
    addWords(a, K_KW, "section segment global extern default bits use16 use32 use64 org align alignb equ times db dw dd dq dt do dy dz resb resw resd resq rest reso resy resz incbin struc endstruc istruc iend absolute common cpu float import export static", &a.dir);
    addWords(a, K_TYPE, "byte word dword qword tword oword yword zword ptr short near far rel abs seg wrt strict nosplit to", nullptr);
    addWords(a, K_KW, "%define %xdefine %undef %assign %macro %endmacro %if %ifdef %ifndef %elif %else %endif %include %rep %endrep %rotate %strlen %substr %pathsearch %error %warning %push %pop", nullptr);
    {   // registers
        std::vector<std::string> regs;
        const char* base[] = {"rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp","eax","ebx","ecx","edx","esi","edi","ebp","esp","ax","bx","cx","dx","si","di","bp","sp","al","ah","bl","bh","cl","ch","dl","dh","sil","dil","bpl","spl","cs","ds","es","fs","gs","ss","rip","eip","rflags","eflags","mxcsr"};
        for (auto r : base) regs.push_back(r);
        for (int i = 8; i < 16; i++) for (const char* s : {"", "d", "w", "b"}) regs.push_back("r" + std::to_string(i) + s);
        for (int i = 0; i < 32; i++) { regs.push_back("xmm" + std::to_string(i)); regs.push_back("ymm" + std::to_string(i)); regs.push_back("zmm" + std::to_string(i)); }
        for (int i = 0; i < 8; i++) { regs.push_back("st" + std::to_string(i)); regs.push_back("mm" + std::to_string(i)); regs.push_back("cr" + std::to_string(i)); regs.push_back("dr" + std::to_string(i)); }
        regs.push_back("st");
        for (auto& r : regs) { a.reg.insert(r); if (r.size() >= 2 && r.rfind("zmm", 0) != 0 && r.rfind("ymm", 0) != 0 && r.rfind("dr", 0) != 0 && r.rfind("mm", 0) != 0) a.items.push_back({r, "", K_TYPE}); }
    }
    finalize(a, true);
#if defined(_WIN32)
    const char* asmRun = "nasm -f win64 \"{file}\" -o \"{dir}/{stem}.obj\" && gcc \"{dir}/{stem}.obj\" -o \"{out}\" && \"{out}\" {args}";
#else
    const char* asmRun = "nasm -f elf64 \"{file}\" -o \"{dir}/{stem}.o\" && ld \"{dir}/{stem}.o\" -o \"{out}\" && \"{out}\" {args}";
#endif
    LANGS[L_C].exts = {"c", "h"}; LANGS[L_CPP].exts = {"cpp", "cc", "cxx", "hpp", "hh", "hxx", "ino"};
    LANGS[L_NASM].exts = {"asm", "nasm", "inc", "s"}; LANGS[L_LUA].exts = {"lua"};
    for (int i : {L_C, L_CPP}) { LANGS[i].lineC = {"//"}; LANGS[i].blockC = {{"/*", "*/"}}; LANGS[i].indentAfter = {"{"}; LANGS[i].dedent = "}"; LANGS[i].wordChars = ":"; LANGS[i].multi = true; }
    LANGS[L_C].run = "gcc -O2 -Wall \"{file}\" -o \"{out}\" && \"{out}\" {args}";
    LANGS[L_CPP].run = "g++ -std=c++17 -O2 -Wall \"{file}\" -o \"{out}\" && \"{out}\" {args}";
    LANGS[L_NASM].lineC = {";"}; LANGS[L_NASM].ci = true; LANGS[L_NASM].pairs = "()[]\"\"''"; LANGS[L_NASM].callParens = false; LANGS[L_NASM].run = asmRun;
    LANGS[L_LUA].lineC = {"--"}; LANGS[L_LUA].wordChars = "."; LANGS[L_LUA].multi = true; LANGS[L_LUA].run = "lua \"{file}\" {args}";
    LANGS[L_LUA].indentAfter = {"then", "do", "repeat", "else", "{"};
    g_snippets.push_back({"c", "main", "int main(void)", "int main(void) {\n\t$0\n\treturn 0;\n}"});
    g_snippets.push_back({"c", "for", "for loop", "for (int ${1:i} = 0; $1 < ${2:n}; $1++) {\n\t$0\n}"});
    g_snippets.push_back({"c", "if", "if", "if (${1:cond}) {\n\t$0\n}"});
    g_snippets.push_back({"c", "while", "while", "while (${1:cond}) {\n\t$0\n}"});
    g_snippets.push_back({"c", "struct", "struct", "struct ${1:Name} {\n\t$0\n};"});
    g_snippets.push_back({"c", "inc", "#include <>", "#include <${1:stdio.h}>"});
    g_snippets.push_back({"c++", "main", "int main()", "int main() {\n\t$0\n\treturn 0;\n}"});
    g_snippets.push_back({"c++", "for", "for loop", "for (int ${1:i} = 0; $1 < ${2:n}; $1++) {\n\t$0\n}"});
    g_snippets.push_back({"c++", "forr", "range for", "for (auto& ${1:x} : ${2:v}) {\n\t$0\n}"});
    g_snippets.push_back({"c++", "class", "class", "class ${1:Name} {\npublic:\n\t$0\n};"});
    g_snippets.push_back({"c++", "cout", "std::cout", "std::cout << ${1:x} << std::endl;"});
    g_snippets.push_back({"c++", "inc", "#include <>", "#include <${1:iostream}>"});
    g_snippets.push_back({"lua", "fn", "function", "function ${1:name}(${2:args})\n\t$0\nend"});
    g_snippets.push_back({"lua", "forp", "for pairs", "for ${1:k}, ${2:v} in pairs(${3:t}) do\n\t$0\nend"});
    g_snippets.push_back({"lua", "fori", "for i", "for ${1:i} = 1, ${2:n} do\n\t$0\nend"});
    g_snippets.push_back({"nasm", "start", "_start (Linux x64)", "section .text\nglobal _start\n_start:\n\t$0\n\tmov rax, 60\n\txor rdi, rdi\n\tsyscall"});
    g_snippets.push_back({"nasm", "hello", "hello world (Linux x64)", "section .data\nmsg db \"Hello\", 10\nlen equ $ - msg\nsection .text\nglobal _start\n_start:\n\tmov rax, 1\n\tmov rdi, 1\n\tmov rsi, msg\n\tmov rdx, len\n\tsyscall\n\tmov rax, 60\n\txor rdi, rdi\n\tsyscall"});
}

// ------------------------------------------------------------ highlight ----
enum Tok : uint8_t { T_TEXT, T_KW, T_TYPE, T_FUNC, T_CALL, T_NUM, T_STR, T_CMT, T_PRE, T_REG, T_LABEL, T_OP, T_DIR, T_COUNT };

static void fillTok(std::vector<uint8_t>& o, size_t a, size_t b, uint8_t t) { for (size_t i = a; i < b && i < o.size(); i++) o[i] = t; }

static void hlC(bool cpp, const U32& s, int& st, std::vector<uint8_t>& o) {
    const LangDef& D = LANGS[cpp ? L_CPP : L_C];
    size_t n = s.size(), i = 0; o.assign(n, T_TEXT); bool first = true;
    while (i < n) {
        if (st == 1) {
            size_t j = i; bool f = false;
            while (j + 1 < n) { if (s[j] == '*' && s[j + 1] == '/') { f = true; break; } j++; }
            size_t e = f ? j + 2 : n; fillTok(o, i, e, T_CMT); i = e; if (f) st = 0; continue;
        }
        char32_t c = s[i];
        if (c == ' ' || c == '\t') { i++; continue; }
        bool isFirst = first; first = false;
        char32_t nx = i + 1 < n ? s[i + 1] : 0;
        if (c == '/' && nx == '/') { fillTok(o, i, n, T_CMT); break; }
        if (c == '/' && nx == '*') { st = 1; fillTok(o, i, i + 2, T_CMT); i += 2; continue; }
        if (c == '"' || c == '\'') {
            size_t j = i + 1; while (j < n && s[j] != c) { if (s[j] == '\\') j++; j++; }
            size_t e = std::min(n, j + 1); fillTok(o, i, e, T_STR); i = e; continue;
        }
        if (c == '#' && isFirst) {
            size_t j = i + 1; while (j < n && (s[j] == ' ' || s[j] == '\t')) j++;
            size_t k = j; while (k < n && isIdC(s[k])) k++;
            fillTok(o, i, k, T_PRE);
            if (narrow(s, j, k) == "include") {
                size_t m = k; while (m < n && s[m] == ' ') m++;
                if (m < n && s[m] == '<') { size_t q = m; while (q < n && s[q] != '>') q++; q = std::min(n, q + 1); fillTok(o, m, q, T_STR); k = q; }
            }
            i = k; continue;
        }
        if (isDig(c) || (c == '.' && isDig(nx))) {
            size_t j = i + 1;
            while (j < n) {
                char32_t d = s[j];
                if (isIdC(d) || d == '.' || d == '\'') j++;
                else if ((d == '+' || d == '-') && (s[j - 1] == 'e' || s[j - 1] == 'E' || s[j - 1] == 'p' || s[j - 1] == 'P')) j++;
                else break;
            }
            fillTok(o, i, j, T_NUM); i = j; continue;
        }
        if (isIdS(c)) {
            size_t j = i + 1; while (j < n && isIdC(s[j])) j++;
            if (cpp) while (j + 2 < n && s[j] == ':' && s[j + 1] == ':' && isIdS(s[j + 2])) { j += 2; while (j < n && isIdC(s[j])) j++; }
            std::string w = narrow(s, i, j);
            size_t k = j; while (k < n && s[k] == ' ') k++;
            bool call = k < n && s[k] == '(';
            uint8_t t = T_TEXT;
            if (D.kw.count(w)) t = T_KW; else if (D.type.count(w)) t = T_TYPE; else if (D.func.count(w)) t = T_FUNC;
            else if (cpp && w.compare(0, 5, "std::") == 0) t = call ? T_FUNC : T_TYPE;
            else if (call) t = T_CALL;
            fillTok(o, i, j, t); i = j; continue;
        }
        if (c > 0 && c < 128 && strchr("+-*/%=<>!&|^~?:", (int)c)) o[i] = T_OP;
        i++;
    }
}

static void hlLua(const U32& s, int& st, std::vector<uint8_t>& o) {
    const LangDef& D = LANGS[L_LUA];
    size_t n = s.size(), i = 0; o.assign(n, T_TEXT);
    auto longOpen = [&](size_t p, int& lvl) -> size_t {
        if (p >= n || s[p] != '[') return 0;
        size_t q = p + 1; int l = 0; while (q < n && s[q] == '=') { l++; q++; }
        if (q < n && s[q] == '[') { lvl = l; return q + 1 - p; }
        return 0;
    };
    while (i < n) {
        if (st >= 100) {
            int lvl = (st - 100) / 2; bool isStr = ((st - 100) % 2) != 0;
            size_t j = i; bool f = false;
            while (j < n) {
                if (s[j] == ']') {
                    size_t q = j + 1; int l = 0; while (q < n && s[q] == '=') { l++; q++; }
                    if (l == lvl && q < n && s[q] == ']') { f = true; j = q + 1; break; }
                }
                j++;
            }
            size_t e = f ? j : n; fillTok(o, i, e, isStr ? T_STR : T_CMT); i = e; if (f) st = 0; continue;
        }
        char32_t c = s[i];
        if (c == ' ' || c == '\t') { i++; continue; }
        char32_t nx = i + 1 < n ? s[i + 1] : 0;
        if (c == '-' && nx == '-') {
            int lvl = 0; size_t L = longOpen(i + 2, lvl);
            if (L) { st = 100 + lvl * 2; fillTok(o, i, i + 2 + L, T_CMT); i += 2 + L; continue; }
            fillTok(o, i, n, T_CMT); break;
        }
        if (c == '[') {
            int lvl = 0; size_t L = longOpen(i, lvl);
            if (L) { st = 100 + lvl * 2 + 1; fillTok(o, i, i + L, T_STR); i += L; continue; }
        }
        if (c == '"' || c == '\'') {
            size_t j = i + 1; while (j < n && s[j] != c) { if (s[j] == '\\') j++; j++; }
            size_t e = std::min(n, j + 1); fillTok(o, i, e, T_STR); i = e; continue;
        }
        if (isDig(c) || (c == '.' && isDig(nx))) {
            size_t j = i + 1;
            while (j < n && (isIdC(s[j]) || s[j] == '.' || ((s[j] == '+' || s[j] == '-') && (s[j - 1] == 'e' || s[j - 1] == 'E' || s[j - 1] == 'p' || s[j - 1] == 'P')))) j++;
            fillTok(o, i, j, T_NUM); i = j; continue;
        }
        if (isIdS(c)) {
            size_t j = i + 1; while (j < n && isIdC(s[j])) j++;
            while (j + 1 < n && s[j] == '.' && isIdS(s[j + 1])) { j++; while (j < n && isIdC(s[j])) j++; }
            std::string w = narrow(s, i, j);
            size_t k = j; while (k < n && s[k] == ' ') k++;
            bool call = k < n && s[k] == '(';
            if (D.kw.count(w)) fillTok(o, i, j, T_KW);
            else if (D.func.count(w)) fillTok(o, i, j, T_FUNC);
            else if (D.type.count(w)) fillTok(o, i, j, T_TYPE);
            else if (call) { size_t p = w.rfind('.'); fillTok(o, i + (p == NPOS ? 0 : p + 1), j, T_CALL); }
            i = j; continue;
        }
        if (c < 128 && strchr("+-*/%^#=<>~", (int)c)) o[i] = T_OP;
        i++;
    }
}

static void hlAsm(const U32& s, std::vector<uint8_t>& o) {
    const LangDef& D = LANGS[L_NASM];
    size_t n = s.size(), i = 0; o.assign(n, T_TEXT); bool first = true;
    while (i < n) {
        char32_t c = s[i];
        if (c == ' ' || c == '\t') { i++; continue; }
        bool isFirst = first; first = false;
        char32_t nx = i + 1 < n ? s[i + 1] : 0;
        if (c == ';') { fillTok(o, i, n, T_CMT); break; }
        if (c == '"' || c == '\'' || c == '`') {
            size_t j = i + 1; while (j < n && s[j] != c) { if (c == '`' && s[j] == '\\') j++; j++; }
            size_t e = std::min(n, j + 1); fillTok(o, i, e, T_STR); i = e; continue;
        }
        if (c == '%') {
            size_t j = i + 1; if (j < n && s[j] == '%') j++;
            while (j < n && (isIdC(s[j]) || s[j] == '$')) j++;
            if (j == i + 1) { o[i] = T_OP; i++; continue; }
            fillTok(o, i, j, T_PRE); i = j; continue;
        }
        if (isDig(c) || (c == '$' && isDig(nx))) {
            size_t j = i + 1; while (j < n && (isIdC(s[j]) || s[j] == '.')) j++;
            fillTok(o, i, j, T_NUM); i = j; continue;
        }
        if (isIdS(c) || c == '.' || c == '?' || c == '@' || c == '$' || c == '#' || c == '~') {
            size_t j = i + 1;
            while (j < n && (isIdC(s[j]) || s[j] == '.' || s[j] == '$' || s[j] == '#' || s[j] == '@' || s[j] == '~' || s[j] == '?')) j++;
            std::string w = narrow(s, i, j, true);
            bool colon = j < n && s[j] == ':';
            uint8_t t = T_TEXT;
            if (w == "$" || w == "$$") t = T_NUM;
            else if (D.reg.count(w)) t = T_REG;
            else if (D.dir.count(w)) t = T_DIR;
            else if (D.type.count(w)) t = T_TYPE;
            else if (D.kw.count(w) && !(isFirst && colon)) t = T_KW;
            else if ((isFirst && (colon || i == 0)) || w[0] == '.') { t = T_LABEL; if (colon) j++; }
            fillTok(o, i, j, t); i = j; continue;
        }
        if (c < 128 && strchr("+-*/<>=!&|^~", (int)c)) o[i] = T_OP;
        i++;
    }
}

