#pragma once
// Generic data-driven highlighter + language/snippet loaders (used by built-in JSON languages AND user plugins)

static bool atS(const U32& s, size_t i, const std::string& p) {
    if (p.empty() || i + p.size() > s.size()) return false;
    for (size_t k = 0; k < p.size(); k++) if (s[i + k] != (char32_t)(unsigned char)p[k]) return false;
    return true;
}

static void hlGeneric(const LangDef& D, const U32& s, int& st, std::vector<uint8_t>& o) {
    size_t n = s.size(), i = 0; o.assign(n, T_TEXT); bool first = true;
    auto idc = [&](char32_t c) { return isIdC(c) || (c > 0 && c < 128 && D.idExtra.find((char)c) != NPOS); };
    auto strEnd = [&](const StrRule& r, size_t j) -> size_t {
        while (j < n) {
            if (!r.esc.empty() && atS(s, j, r.esc)) { j += r.esc.size() + 1; continue; }
            if (atS(s, j, r.end)) return j + r.end.size();
            j++;
        }
        return NPOS;
    };
    while (i < n) {
        if (st >= 1 && st < 20 && (size_t)(st - 1) < D.blockC.size()) {
            const std::string& e = D.blockC[st - 1].second; size_t j = i; bool f = false;
            while (j < n) { if (atS(s, j, e)) { f = true; j += e.size(); break; } j++; }
            size_t en = f ? j : n; fillTok(o, i, en, T_CMT); i = en; if (f) st = 0; continue;
        }
        if (st >= 20 && st < 100 && (size_t)(st - 20) < D.strs.size()) {
            size_t e = strEnd(D.strs[st - 20], i);
            if (e == NPOS) { fillTok(o, i, n, T_STR); i = n; } else { fillTok(o, i, e, T_STR); i = e; st = 0; }
            continue;
        }
        char32_t c = s[i];
        if (st == 100) {   // inside <tag ...>
            if (c == ' ' || c == '\t') { i++; continue; }
            if (c == '>') { o[i] = T_KW; st = 0; i++; continue; }
            if (c == '/' && i + 1 < n && s[i + 1] == '>') { fillTok(o, i, i + 2, T_KW); st = 0; i += 2; continue; }
            if (c == '"' || c == '\'') { size_t j = i + 1; while (j < n && s[j] != c) j++; size_t e = std::min(n, j + 1); fillTok(o, i, e, T_STR); i = e; continue; }
            if (isIdS(c)) { size_t j = i + 1; while (j < n && (isIdC(s[j]) || s[j] == '-' || s[j] == ':')) j++; fillTok(o, i, j, T_TYPE); i = j; continue; }
            if (c == '=') o[i] = T_OP;
            i++; continue;
        }
        if (st != 0) st = 0;   // stale state (language changed)
        if (c == ' ' || c == '\t') { i++; continue; }
        bool isFirst = first; first = false; bool done = false;
        if (isFirst) { for (auto& r : D.lineStart) if (atS(s, i, r.prefix)) { fillTok(o, i, n, (uint8_t)r.tok); done = true; break; } if (done) break; }
        for (auto& lc : D.lineC) if (atS(s, i, lc)) { fillTok(o, i, n, T_CMT); done = true; break; }
        if (done) break;
        for (size_t b = 0; b < D.blockC.size() && !done && b < 19; b++) if (atS(s, i, D.blockC[b].first)) {
            const std::string& e = D.blockC[b].second; size_t j = i + D.blockC[b].first.size(); bool f = false;
            while (j < n) { if (atS(s, j, e)) { f = true; j += e.size(); break; } j++; }
            fillTok(o, i, f ? j : n, T_CMT); if (!f) st = (int)b + 1; i = f ? j : n; done = true;
        }
        if (done) continue;
        for (size_t k = 0; k < D.strs.size() && !done && k < 80; k++) if (atS(s, i, D.strs[k].start)) {
            const StrRule& r = D.strs[k]; size_t e = strEnd(r, i + r.start.size()); size_t en;
            if (e == NPOS) { en = n; if (r.multi) st = 20 + (int)k; } else en = e;
            uint8_t t = T_STR;
            if (D.keyColon && e != NPOS) { size_t q = en; while (q < n && s[q] == ' ') q++; if (q < n && s[q] == ':') t = T_TYPE; }
            fillTok(o, i, en, t); i = en; done = true;
        }
        if (done) continue;
        if (D.markup && c == '<' && i + 1 < n && (isIdS(s[i + 1]) || s[i + 1] == '/' || s[i + 1] == '!' || s[i + 1] == '?')) {
            size_t j = i + 1; if (s[j] == '/') j++;
            if (j < n && (s[j] == '!' || s[j] == '?')) { size_t e = j; while (e < n && s[e] != '>') e++; e = std::min(n, e + 1); fillTok(o, i, e, T_PRE); i = e; continue; }
            while (j < n && (isIdC(s[j]) || s[j] == '-' || s[j] == ':')) j++;
            fillTok(o, i, j, T_KW); i = j; st = 100; continue;
        }
        if (D.markup && c == '&') { size_t j = i + 1; while (j < n && isIdC(s[j])) j++; if (j < n && s[j] == ';') { fillTok(o, i, j + 1, T_NUM); i = j + 1; continue; } }
        if (!D.preproc.empty() && isFirst && atS(s, i, D.preproc)) { size_t j = i + D.preproc.size(); while (j < n && idc(s[j])) j++; fillTok(o, i, j, T_PRE); i = j; continue; }
        if (!D.annotation.empty() && atS(s, i, D.annotation) && i + D.annotation.size() < n && isIdS(s[i + D.annotation.size()])) {
            size_t j = i + D.annotation.size(); while (j < n && (isIdC(s[j]) || s[j] == '.')) j++; fillTok(o, i, j, T_PRE); i = j; continue; }
        if (!D.variable.empty() && atS(s, i, D.variable) && i + 1 < n && (isIdS(s[i + 1]) || s[i + 1] == '{' || isDig(s[i + 1]))) {
            size_t j = i + 1; if (s[j] == '{') { while (j < n && s[j] != '}') j++; j = std::min(n, j + 1); } else while (j < n && isIdC(s[j])) j++;
            fillTok(o, i, j, T_REG); i = j; continue; }
        if (D.hashNum && c == '#') {
            size_t j = i + 1; while (j < n && ((s[j] >= '0' && s[j] <= '9') || (s[j] >= 'a' && s[j] <= 'f') || (s[j] >= 'A' && s[j] <= 'F'))) j++;
            if (j - i >= 4) { fillTok(o, i, j, T_NUM); i = j; continue; } }
        if (isDig(c) || (c == '.' && i + 1 < n && isDig(s[i + 1]))) {
            size_t j = i + 1; while (j < n && (isIdC(s[j]) || s[j] == '.')) j++; fillTok(o, i, j, T_NUM); i = j; continue; }
        if (isIdS(c)) {
            size_t j = i + 1; while (j < n && idc(s[j])) j++;
            if (!D.wordChars.empty()) {
                std::string sep = D.wordChars == ":" ? "::" : D.wordChars;
                while (atS(s, j, sep) && j + sep.size() < n && isIdS(s[j + sep.size()])) { j += sep.size(); while (j < n && idc(s[j])) j++; }
            }
            std::string w = narrow(s, i, j, D.ci);
            size_t k = j; while (k < n && s[k] == ' ') k++;
            bool call = k < n && s[k] == '(';
            uint8_t t = T_TEXT;
            if (D.kw.count(w)) t = T_KW; else if (D.type.count(w)) t = T_TYPE; else if (D.func.count(w)) t = T_FUNC; else if (D.cst.count(w)) t = T_NUM;
            else if (D.keyColon && k < n && s[k] == ':' && !(k + 1 < n && s[k + 1] == ':')) t = T_TYPE;
            else if (call) t = T_CALL;
            fillTok(o, i, j, t); i = j; continue;
        }
        if (c > 0 && c < 128 && strchr("+-*/%=<>!&|^~?:", (int)c)) o[i] = T_OP;
        i++;
    }
}

static void highlight(Lang l, const U32& s, int& st, std::vector<uint8_t>& o) {
    if (l < 0 || l >= (int)LANGS.size()) { o.assign(s.size(), T_TEXT); return; }
    switch (LANGS[l].special) {
        case LS_C: hlC(false, s, st, o); break;
        case LS_CPP: hlC(true, s, st, o); break;
        case LS_LUA: hlLua(s, st, o); break;
        case LS_NASM: hlAsm(s, o); break;
        case LS_GENERIC: hlGeneric(LANGS[l], s, st, o); break;
        default: o.assign(s.size(), T_TEXT); break;
    }
}

static Lang langFromPath(const std::string& p) {
    size_t sl = p.find_last_of("/\\"), d = p.rfind('.');
    std::string base = lowerS(sl == NPOS ? p : p.substr(sl + 1)), e;
    if (d != NPOS && (sl == NPOS || d > sl)) e = lowerS(p.substr(d + 1));
    for (int i = (int)LANGS.size() - 1; i > 0; i--) for (auto& x : LANGS[i].exts) if (x == e || x == base) return i;
    return L_TEXT;
}
static int findLang(const std::string& name) {
    std::string n = lowerS(name);
    for (size_t i = 0; i < LANGS.size(); i++) if (lowerS(LANGS[i].name) == n) return (int)i;
    return -1;
}

// ---------------------------------------------------------------- loaders ----
static const char* TOKEN_KEYS[T_COUNT] = {"text", "keyword", "type", "function", "call", "number", "string", "comment", "preproc", "register", "label", "operator", "directive"};
static int tokByName(const std::string& n) { for (int i = 0; i < T_COUNT; i++) if (n == TOKEN_KEYS[i]) return i; return T_KW; }

static void loadSnippets(const JV* arr, const std::string& defLang) {
    if (!arr || arr->t != JV::ARR) return;
    for (auto& e : arr->a) {
        if (e.t != JV::OBJ) continue;
        Snippet s{lowerS(e.str("lang", defLang)), e.str("prefix"), e.str("description", e.str("desc")), e.str("body")};
        if (s.prefix.empty() || s.body.empty()) continue;
        if (s.lang.empty()) s.lang = "*";
        g_snippets.push_back(s);
    }
}

// Accepts the language spec format documented in README (same format for built-ins and plugins).
static void loadLangJson(const JV& j) {
    std::string name = j.str("name"); if (name.empty()) return;
    int id = findLang(name);
    if (id < 0) { LANGS.push_back(LangDef()); id = (int)LANGS.size() - 1; LANGS[id].name = name; LANGS[id].special = LS_GENERIC; LANGS[id].multi = false; }
    LangDef& d = LANGS[id];
    for (auto& e : j.strs("extensions")) { std::string x = lowerS(e); if (!x.empty() && x[0] == '.') x.erase(0, 1); d.exts.push_back(x); }
    d.ci = j.boolean("caseInsensitive", d.ci); d.callParens = j.boolean("callParens", d.callParens);
    d.keyColon = j.boolean("keyColon", d.keyColon); d.hashNum = j.boolean("hashNumbers", d.hashNum); d.markup = j.boolean("markup", d.markup);
    d.preproc = j.str("preprocessor", d.preproc); d.annotation = j.str("annotation", d.annotation); d.variable = j.str("variable", d.variable);
    d.idExtra = j.str("identifierChars", d.idExtra); d.wordChars = j.str("wordChars", d.wordChars); d.run = j.str("run", d.run);
    if (j.get("pairs")) d.pairs = j.str("pairs"); d.dedent = j.str("dedent", d.dedent);
    auto add = [&](const char* key, Kind k) { for (auto& w : j.strs(key)) d.items.push_back({w, "", k}); };
    add("keywords", K_KW); add("types", K_TYPE); add("constants", K_CONST);
    for (auto& f : j.strs("functions")) { size_t p = f.find('|'); d.items.push_back({f.substr(0, p), p == NPOS ? "" : f.substr(p + 1), K_FUNC}); }
    if (auto* c = j.get("comments")) {
        for (auto& l : c->strs("line")) d.lineC.push_back(l);
        if (auto* b = c->get("block")) if (b->t == JV::ARR) for (auto& e : b->a) if (e.t == JV::ARR && e.a.size() == 2) d.blockC.push_back({e.a[0].s, e.a[1].s});
    }
    if (auto* st = j.get("strings")) if (st->t == JV::ARR) for (auto& e : st->a) {
        StrRule r; r.esc = "\\";
        if (e.t == JV::STR) { r.start = r.end = e.s; }
        else if (e.t == JV::OBJ) { r.start = e.str("start"); r.end = e.str("end", r.start); r.esc = e.str("escape", "\\"); r.multi = e.boolean("multiline"); }
        if (!r.start.empty()) d.strs.push_back(r);
    }
    std::stable_sort(d.strs.begin(), d.strs.end(), [](const StrRule& a, const StrRule& b) { return a.start.size() > b.start.size(); });
    if (auto* ls = j.get("lineStart")) if (ls->t == JV::ARR) for (auto& e : ls->a) d.lineStart.push_back({e.str("prefix"), tokByName(e.str("token", "keyword"))});
    for (auto& s : j.strs("indentAfter")) d.indentAfter.push_back(s);
    if (d.dedent.empty()) for (auto& s : d.indentAfter) if (s == "{") d.dedent = "}";
    finalize(d, d.ci);
    d.multi = d.special != LS_GENERIC ? d.multi : (!d.blockC.empty() || d.markup || std::any_of(d.strs.begin(), d.strs.end(), [](const StrRule& r) { return r.multi; }));
    if (d.special == LS_GENERIC && d.markup) d.multi = true;
    loadSnippets(j.get("snippets"), lowerS(name));
}

// ------------------------------------------------------- built-in languages ----
static const char* BUILTIN_LANGS[] = {
R"J({"name":"Python","extensions":["py","pyw","pyi"],"run":"python3 \"{file}\" {args}","indentAfter":[":"],"wordChars":".","annotation":"@",
"comments":{"line":["#"]},"strings":[{"start":"\"\"\"","multiline":true},{"start":"'''","multiline":true},"\"","'"],
"keywords":["and","as","assert","async","await","break","class","continue","def","del","elif","else","except","finally","for","from","global","if","import","in","is","lambda","nonlocal","not","or","pass","raise","return","try","while","with","yield","match","case"],
"constants":["True","False","None","self","cls","__name__","__file__"],"types":["int","str","float","bool","list","dict","set","tuple","bytes","object","type"],
"functions":["print|print(*values, sep=' ', end='\\n')","len|len(obj)","range|range(start, stop[, step])","input|input([prompt])","open|open(file, mode='r')","enumerate|enumerate(iterable, start=0)","zip|zip(*iterables)","map|map(func, iterable)","filter|filter(func, iterable)","sorted|sorted(iterable, key=None, reverse=False)","sum|sum(iterable)","min|min(iterable)","max|max(iterable)","abs|abs(x)","isinstance|isinstance(obj, cls)","super|super()","str|str(obj)","repr|repr(obj)","os.path.join|os.path.join(a, *p)","os.listdir|os.listdir(path)","sys.exit|sys.exit(code)","json.loads|json.loads(s)","json.dumps|json.dumps(obj)","time.sleep|time.sleep(secs)","random.randint|random.randint(a, b)","math.sqrt|math.sqrt(x)"],
"snippets":[{"prefix":"def","description":"function","body":"def ${1:name}(${2:args}):\n\t$0"},{"prefix":"class","description":"class","body":"class ${1:Name}:\n\tdef __init__(self${2}):\n\t\t$0"},{"prefix":"main","description":"main guard","body":"if __name__ == \"__main__\":\n\t$0"},{"prefix":"for","description":"for loop","body":"for ${1:i} in range(${2:n}):\n\t$0"}]})J",
R"J({"name":"JavaScript","extensions":["js","mjs","cjs","jsx"],"run":"node \"{file}\" {args}","indentAfter":["{","(","["],"wordChars":".","pairs":"()[]{}\"\"''``",
"comments":{"line":["//"],"block":[["/*","*/"]]},"strings":[{"start":"`","multiline":true},"\"","'"],
"keywords":["async","await","break","case","catch","class","const","continue","debugger","default","delete","do","else","export","extends","finally","for","function","if","import","in","instanceof","let","new","of","return","static","super","switch","this","throw","try","typeof","var","void","while","with","yield","from","as","get","set"],
"constants":["true","false","null","undefined","NaN","Infinity"],"types":["Array","Object","String","Number","Boolean","Promise","Map","Set","Date","RegExp","Error","JSON","Math"],
"functions":["console.log|console.log(...data)","console.error|console.error(...data)","setTimeout|setTimeout(fn, ms)","setInterval|setInterval(fn, ms)","fetch|fetch(url[, options])","parseInt|parseInt(str, radix)","parseFloat|parseFloat(str)","JSON.parse|JSON.parse(text)","JSON.stringify|JSON.stringify(value)","Math.floor|Math.floor(x)","Math.random|Math.random()","Array.isArray|Array.isArray(x)","Object.keys|Object.keys(obj)","require|require(module)","document.getElementById|document.getElementById(id)","addEventListener|addEventListener(type, fn)"],
"snippets":[{"prefix":"fn","description":"function","body":"function ${1:name}(${2:args}) {\n\t$0\n}"},{"prefix":"arrow","description":"arrow fn","body":"const ${1:name} = (${2:args}) => {\n\t$0\n};"},{"prefix":"for","description":"for loop","body":"for (let ${1:i} = 0; $1 < ${2:n}; $1++) {\n\t$0\n}"},{"prefix":"log","description":"console.log","body":"console.log($0);"}]})J",
R"J({"name":"TypeScript","extensions":["ts","tsx"],"run":"npx ts-node \"{file}\" {args}","indentAfter":["{","(","["],"wordChars":".","pairs":"()[]{}\"\"''``",
"comments":{"line":["//"],"block":[["/*","*/"]]},"strings":[{"start":"`","multiline":true},"\"","'"],
"keywords":["abstract","as","async","await","break","case","catch","class","const","continue","declare","default","delete","do","else","enum","export","extends","finally","for","function","if","implements","import","in","instanceof","interface","let","namespace","new","of","private","protected","public","readonly","return","static","super","switch","this","throw","try","type","typeof","var","while","yield","from"],
"constants":["true","false","null","undefined","NaN"],"types":["string","number","boolean","any","void","never","unknown","object","Array","Promise","Record","Partial","Map","Set"],
"functions":["console.log|console.log(...data)","setTimeout|setTimeout(fn, ms)","fetch|fetch(url[, options])","JSON.parse|JSON.parse(text)","JSON.stringify|JSON.stringify(value)"]})J",
R"J({"name":"Java","extensions":["java"],"run":"javac \"{file}\" && java -cp \"{dir}\" {stem} {args}","indentAfter":["{"],"annotation":"@","wordChars":".",
"comments":{"line":["//"],"block":[["/*","*/"]]},"strings":["\"","'"],
"keywords":["abstract","assert","break","case","catch","class","continue","default","do","else","enum","extends","final","finally","for","if","implements","import","instanceof","interface","native","new","package","private","protected","public","return","static","super","switch","synchronized","this","throw","throws","try","volatile","while","var"],
"constants":["true","false","null"],"types":["int","long","short","byte","char","float","double","boolean","void","String","Integer","Long","Double","Object","List","ArrayList","Map","HashMap","Set","HashSet","Scanner","Math"],
"functions":["System.out.println|System.out.println(x)","System.out.print|System.out.print(x)","System.out.printf|System.out.printf(fmt, args...)","String.format|String.format(fmt, args...)","Math.max|Math.max(a, b)","Math.min|Math.min(a, b)","Integer.parseInt|Integer.parseInt(s)"],
"snippets":[{"prefix":"main","description":"main method","body":"public static void main(String[] args) {\n\t$0\n}"},{"prefix":"sout","description":"println","body":"System.out.println($0);"},{"prefix":"class","description":"class","body":"public class ${1:Name} {\n\t$0\n}"}]})J",
R"J({"name":"C#","extensions":["cs"],"run":"dotnet run","indentAfter":["{"],"preprocessor":"#","wordChars":".",
"comments":{"line":["//"],"block":[["/*","*/"]]},"strings":[{"start":"@\"","escape":""},"\"","'"],
"keywords":["abstract","as","async","await","base","break","case","catch","class","const","continue","default","delegate","do","else","enum","event","explicit","extern","finally","for","foreach","get","if","implicit","in","interface","internal","is","lock","namespace","new","operator","out","override","params","partial","private","protected","public","readonly","ref","return","sealed","set","static","struct","switch","this","throw","try","typeof","using","var","virtual","volatile","while","yield"],
"constants":["true","false","null"],"types":["int","long","short","byte","char","float","double","decimal","bool","string","object","void","List","Dictionary","Task","Console","Math"],
"functions":["Console.WriteLine|Console.WriteLine(value)","Console.Write|Console.Write(value)","Console.ReadLine|Console.ReadLine()","Math.Sqrt|Math.Sqrt(x)","string.Format|string.Format(fmt, args)"]})J",
R"J({"name":"Go","extensions":["go"],"run":"go run \"{file}\" {args}","indentAfter":["{"],"wordChars":".",
"comments":{"line":["//"],"block":[["/*","*/"]]},"strings":[{"start":"`","escape":"","multiline":true},"\"","'"],
"keywords":["break","case","chan","const","continue","default","defer","else","fallthrough","for","func","go","goto","if","import","interface","map","package","range","return","select","struct","switch","type","var"],
"constants":["true","false","nil","iota"],"types":["int","int8","int16","int32","int64","uint","uint8","uint16","uint32","uint64","float32","float64","string","bool","byte","rune","error","any"],
"functions":["fmt.Println|fmt.Println(a ...any)","fmt.Printf|fmt.Printf(format string, a ...any)","fmt.Sprintf|fmt.Sprintf(format string, a ...any)","len|len(v)","cap|cap(v)","make|make(T, size)","append|append(slice, elems...)","panic|panic(v)","strings.Split|strings.Split(s, sep)","os.Exit|os.Exit(code)"],
"snippets":[{"prefix":"main","description":"main func","body":"package main\n\nimport \"fmt\"\n\nfunc main() {\n\t$0\n}"},{"prefix":"for","description":"for loop","body":"for ${1:i} := 0; $1 < ${2:n}; $1++ {\n\t$0\n}"}]})J",
R"J({"name":"Rust","extensions":["rs"],"run":"rustc \"{file}\" -o \"{out}\" && \"{out}\" {args}","indentAfter":["{"],"wordChars":":","annotation":"#",
"comments":{"line":["//"],"block":[["/*","*/"]]},"strings":["\"","'"],
"keywords":["as","async","await","break","const","continue","crate","dyn","else","enum","extern","fn","for","if","impl","in","let","loop","match","mod","move","mut","pub","ref","return","self","Self","static","struct","super","trait","type","unsafe","use","where","while"],
"constants":["true","false","None","Some","Ok","Err"],"types":["i8","i16","i32","i64","i128","isize","u8","u16","u32","u64","u128","usize","f32","f64","bool","char","str","String","Vec","Option","Result","Box","Rc","Arc","HashMap"],
"functions":["println!|println!(\"{}\", x)","print!|print!(\"{}\", x)","format!|format!(\"{}\", x)","vec!|vec![a, b, c]","panic!|panic!(\"msg\")","assert!|assert!(cond)","assert_eq!|assert_eq!(a, b)","String::from|String::from(s)","Vec::new|Vec::new()"],
"snippets":[{"prefix":"main","description":"main fn","body":"fn main() {\n\t$0\n}"},{"prefix":"fn","description":"function","body":"fn ${1:name}(${2:args}) -> ${3:()} {\n\t$0\n}"},{"prefix":"struct","description":"struct","body":"struct ${1:Name} {\n\t$0\n}"}]})J",
R"J({"name":"Kotlin","extensions":["kt","kts"],"run":"kotlinc \"{file}\" -include-runtime -d \"{dir}/{stem}.jar\" && java -jar \"{dir}/{stem}.jar\" {args}","indentAfter":["{"],"annotation":"@",
"comments":{"line":["//"],"block":[["/*","*/"]]},"strings":[{"start":"\"\"\"","multiline":true},"\"","'"],
"keywords":["as","break","class","continue","do","else","for","fun","if","in","interface","is","object","override","package","private","protected","public","return","super","this","throw","try","typealias","val","var","when","while","import","data","sealed","open","abstract","companion","suspend","lateinit"],
"constants":["true","false","null"],"types":["Int","Long","Short","Byte","Char","Float","Double","Boolean","String","Unit","Any","List","Map","Set","Array"],
"functions":["println|println(message)","print|print(message)","listOf|listOf(vararg elements)","mutableListOf|mutableListOf()","mapOf|mapOf(vararg pairs)","readLine|readLine()"]})J",
R"J({"name":"PHP","extensions":["php"],"run":"php \"{file}\" {args}","indentAfter":["{"],"variable":"$","callParens":true,
"comments":{"line":["//","#"],"block":[["/*","*/"]]},"strings":["\"","'"],
"keywords":["abstract","and","array","as","break","case","catch","class","const","continue","default","do","echo","else","elseif","extends","final","finally","for","foreach","function","global","if","implements","include","interface","namespace","new","or","private","protected","public","require","return","static","switch","throw","trait","try","use","while","yield"],
"constants":["true","false","null","TRUE","FALSE","NULL"],"functions":["echo|echo $x","print_r|print_r($v)","var_dump|var_dump($v)","strlen|strlen($s)","count|count($a)","array_map|array_map($fn, $a)","implode|implode($sep, $a)","explode|explode($sep, $s)","json_encode|json_encode($v)","isset|isset($v)","empty|empty($v)"]})J",
R"J({"name":"Bash","extensions":["sh","bash","zsh","ksh","bashrc","profile"],"run":"bash \"{file}\" {args}","variable":"$","indentAfter":["then","do","{"],"callParens":false,
"comments":{"line":["#"]},"strings":["\"","'"],
"keywords":["if","then","else","elif","fi","for","while","until","do","done","case","esac","in","function","select","return","exit","local","export","readonly","declare","source","alias","unset","break","continue","shift","trap"],
"functions":["echo|echo [-n] text","printf|printf format args","cd|cd dir","ls|ls [opts] [path]","grep|grep [opts] pattern [file]","sed|sed [opts] script [file]","awk|awk 'prog' [file]","cat|cat [file]","mkdir|mkdir [-p] dir","rm|rm [-rf] path","cp|cp src dst","mv|mv src dst","chmod|chmod mode file","find|find path [expr]","read|read [-r] var","test|test expr","curl|curl [opts] url","tar|tar [opts] file"]})J",
R"J({"name":"SQL","extensions":["sql"],"caseInsensitive":true,"callParens":true,"comments":{"line":["--"],"block":[["/*","*/"]]},"strings":["'","\""],
"keywords":["select","from","where","insert","into","values","update","set","delete","create","table","drop","alter","add","index","view","join","inner","left","right","outer","full","cross","on","as","and","or","not","null","is","in","like","between","exists","group","by","order","having","limit","offset","union","all","distinct","primary","key","foreign","references","default","unique","check","constraint","begin","commit","rollback","transaction","case","when","then","else","end","asc","desc","if"],
"types":["int","integer","bigint","smallint","varchar","char","text","date","datetime","timestamp","boolean","bool","float","double","decimal","numeric","blob","serial"],
"functions":["count|count(expr)","sum|sum(expr)","avg|avg(expr)","min|min(expr)","max|max(expr)","coalesce|coalesce(a, b, ...)","upper|upper(s)","lower|lower(s)","length|length(s)","substr|substr(s, start, len)","now|now()","round|round(x, n)"]})J",
R"J({"name":"JSON","extensions":["json","jsonc","geojson","webmanifest"],"keyColon":true,"callParens":false,"indentAfter":["{","["],"comments":{"line":["//"],"block":[["/*","*/"]]},"strings":["\""],"constants":["true","false","null"],"pairs":"()[]{}\"\""})J",
R"J({"name":"HTML","extensions":["html","htm","xml","svg","xhtml","vue","xaml","plist"],"markup":true,"callParens":false,"comments":{"block":[["<!--","-->"]]},"identifierChars":"-","strings":[]})J",
R"J({"name":"CSS","extensions":["css","scss","less"],"hashNumbers":true,"callParens":false,"indentAfter":["{"],"identifierChars":"-","annotation":"@","comments":{"line":["//"],"block":[["/*","*/"]]},"strings":["\"","'"],
"types":["color","background","background-color","margin","padding","border","border-radius","font","font-size","font-family","font-weight","display","position","width","height","min-width","max-width","min-height","max-height","top","left","right","bottom","flex","grid","gap","align-items","justify-content","overflow","opacity","transform","transition","animation","z-index","cursor","box-shadow","text-align","line-height","content"],
"constants":["none","auto","block","inline","flex","grid","relative","absolute","fixed","sticky","center","solid","bold","inherit","transparent","hidden","visible"],"functions":["rgb|rgb(r, g, b)","rgba|rgba(r, g, b, a)","calc|calc(expr)","var|var(--name)","url|url(path)","linear-gradient|linear-gradient(dir, c1, c2)","translate|translate(x, y)","rotate|rotate(angle)"]})J",
R"J({"name":"Markdown","extensions":["md","markdown","mdx","txt.md"],"callParens":false,"pairs":"()[]\"\"``","lineStart":[{"prefix":"#","token":"keyword"},{"prefix":">","token":"comment"},{"prefix":"- ","token":"type"},{"prefix":"* ","token":"type"},{"prefix":"+ ","token":"type"}],
"strings":[{"start":"```","multiline":true,"escape":""},{"start":"`","escape":""}]})J",
R"J({"name":"YAML","extensions":["yml","yaml"],"keyColon":true,"callParens":false,"indentAfter":[":"],"comments":{"line":["#"]},"strings":["\"","'"],"constants":["true","false","null","yes","no","on","off"],"identifierChars":"-.","lineStart":[{"prefix":"---","token":"preproc"}]})J",
R"J({"name":"TOML","extensions":["toml","ini","cfg","conf","properties","desktop"],"callParens":false,"comments":{"line":["#",";"]},"strings":[{"start":"\"\"\"","multiline":true},"\"","'"],"constants":["true","false"],"lineStart":[{"prefix":"[","token":"keyword"}],"identifierChars":"-.","hashNumbers":false})J",
R"J({"name":"Makefile","extensions":["mk","makefile","gnumakefile"],"callParens":false,"variable":"$","comments":{"line":["#"]},"strings":["\"","'"],"keywords":["ifeq","ifneq","ifdef","ifndef","else","endif","include","define","endef","export","override",".PHONY"],"run":"make","indentAfter":[":"]})J",
nullptr};

static const char* BUILTIN_THEMES[] = {
R"J({"name":"One Dark","colors":{"bg":"#1e2026","fg":"#d4d8e0","gutter":"#242730","gutterFg":"#646a78","curLine":"#2c303a","sel":"#466ebe6e","cursor":"#ffcc66","bar":"#181a1f","barFg":"#aab0be","accent":"#61afef","popup":"#282c36","match":"#ffc83c5a","btn":"#343946","good":"#98c379","warn":"#e5c07b","bad":"#e06c75","text":"#d4d8e0","keyword":"#c678dd","type":"#e5c07b","function":"#61afef","call":"#7fbfff","number":"#d19a66","string":"#98c379","comment":"#6c7587","preproc":"#e06c75","register":"#56b6c2","label":"#ffcc66","operator":"#78bec8","directive":"#e06c75"}})J",
R"J({"name":"Light","colors":{"bg":"#fcfcfa","fg":"#282c34","gutter":"#f0f0ec","gutterFg":"#969696","curLine":"#eef2fa","sel":"#6496e65a","cursor":"#141414","bar":"#e6e7eb","barFg":"#3c4048","accent":"#1e64d2","popup":"#f6f6f8","match":"#ffbe0064","btn":"#d6d9e0","good":"#50a14f","warn":"#c18401","bad":"#ca1243","text":"#282c34","keyword":"#a626a4","type":"#986801","function":"#4078f2","call":"#1e5ac8","number":"#986801","string":"#50a14f","comment":"#9696a0","preproc":"#c82832","register":"#0184bc","label":"#b46400","operator":"#0184bc","directive":"#c82832"}})J",
R"J({"name":"Dracula","base":"One Dark","colors":{"bg":"#282a36","fg":"#f8f8f2","gutter":"#21222c","gutterFg":"#6272a4","curLine":"#343746","sel":"#bd93f955","cursor":"#f8f8f0","bar":"#191a21","barFg":"#bdc0d4","accent":"#bd93f9","popup":"#2d2f3d","btn":"#44475a","good":"#50fa7b","warn":"#f1fa8c","bad":"#ff5555","text":"#f8f8f2","keyword":"#ff79c6","type":"#8be9fd","function":"#50fa7b","call":"#69f0a0","number":"#bd93f9","string":"#f1fa8c","comment":"#6272a4","preproc":"#ffb86c","register":"#8be9fd","label":"#ffb86c","operator":"#ff79c6","directive":"#ff79c6"}})J",
R"J({"name":"Monokai","base":"One Dark","colors":{"bg":"#272822","fg":"#f8f8f2","gutter":"#2d2e27","gutterFg":"#75715e","curLine":"#3e3d32","sel":"#49483e","cursor":"#f8f8f0","bar":"#1e1f1a","accent":"#a6e22e","popup":"#33342c","btn":"#49483e","text":"#f8f8f2","keyword":"#f92672","type":"#66d9ef","function":"#a6e22e","call":"#b8f060","number":"#ae81ff","string":"#e6db74","comment":"#75715e","preproc":"#f92672","register":"#66d9ef","label":"#fd971f","operator":"#f92672","directive":"#fd971f"}})J",
R"J({"name":"Solarized Dark","base":"One Dark","colors":{"bg":"#002b36","fg":"#93a1a1","gutter":"#073642","gutterFg":"#586e75","curLine":"#073642","sel":"#268bd255","cursor":"#b58900","bar":"#00212b","barFg":"#839496","accent":"#268bd2","popup":"#073642","btn":"#0d4a5a","text":"#93a1a1","keyword":"#859900","type":"#b58900","function":"#268bd2","call":"#2aa198","number":"#d33682","string":"#2aa198","comment":"#586e75","preproc":"#cb4b16","register":"#6c71c4","label":"#b58900","operator":"#859900","directive":"#cb4b16"}})J",
R"J({"name":"Nord","base":"One Dark","colors":{"bg":"#2e3440","fg":"#d8dee9","gutter":"#3b4252","gutterFg":"#616e88","curLine":"#3b4252","sel":"#5e81ac66","cursor":"#88c0d0","bar":"#272c36","barFg":"#c0c8d8","accent":"#88c0d0","popup":"#343b49","btn":"#434c5e","text":"#d8dee9","keyword":"#81a1c1","type":"#8fbcbb","function":"#88c0d0","call":"#9ad1e0","number":"#b48ead","string":"#a3be8c","comment":"#616e88","preproc":"#d08770","register":"#8fbcbb","label":"#ebcb8b","operator":"#81a1c1","directive":"#d08770"}})J",
R"J({"name":"AMOLED","base":"One Dark","colors":{"bg":"#000000","gutter":"#050505","curLine":"#111318","bar":"#000000","popup":"#0c0d10","btn":"#1a1c22","accent":"#00e5ff"}})J",
nullptr};
