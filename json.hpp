#pragma once
// Minimal JSON (with // and /* */ comments and trailing commas tolerated) - used for settings, themes and plugins.
struct JV {
    enum T { NUL, BOOL, NUM, STR, ARR, OBJ } t = NUL;
    bool b = false; double n = 0; std::string s;
    std::vector<JV> a; std::vector<std::pair<std::string, JV>> o;

    static JV object() { JV j; j.t = OBJ; return j; }
    static JV array() { JV j; j.t = ARR; return j; }
    static JV string(const std::string& v) { JV j; j.t = STR; j.s = v; return j; }
    static JV number(double v) { JV j; j.t = NUM; j.n = v; return j; }
    static JV boolean_(bool v) { JV j; j.t = BOOL; j.b = v; return j; }

    const JV* get(const std::string& k) const {
        if (t == OBJ) for (auto& kv : o) if (kv.first == k) return &kv.second;
        return nullptr;
    }
    std::string str(const std::string& k, const std::string& d = "") const { auto* v = get(k); return v && v->t == STR ? v->s : d; }
    double num(const std::string& k, double d = 0) const { auto* v = get(k); return v && v->t == NUM ? v->n : d; }
    bool boolean(const std::string& k, bool d = false) const { auto* v = get(k); return v && v->t == BOOL ? v->b : d; }
    std::vector<std::string> strs(const std::string& k) const {
        std::vector<std::string> r; auto* v = get(k);
        if (v && v->t == ARR) for (auto& e : v->a) if (e.t == STR) r.push_back(e.s);
        return r;
    }
    JV& set(const std::string& k, JV v) {
        for (auto& kv : o) if (kv.first == k) { kv.second = std::move(v); return kv.second; }
        o.emplace_back(k, std::move(v)); return o.back().second;
    }
};

struct JParser {
    const std::string& s; size_t i = 0; std::string err;
    explicit JParser(const std::string& src) : s(src) {}
    void ws() {
        for (;;) {
            while (i < s.size() && isspace((unsigned char)s[i])) i++;
            if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') { while (i < s.size() && s[i] != '\n') i++; }
            else if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') { i += 2; while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) i++; i += 2; }
            else break;
        }
    }
    bool fail(const char* m) { if (err.empty()) err = std::string(m) + " at offset " + std::to_string(i); return false; }
    bool str(std::string& out) {
        if (i >= s.size() || s[i] != '"') return fail("string expected");
        i++;
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c != '\\') { out += c; continue; }
            if (i >= s.size()) return fail("bad escape");
            char e = s[i++];
            switch (e) {
                case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                case 'u': {
                    auto hex4 = [&](unsigned& v) { if (i + 4 > s.size()) return false; v = (unsigned)strtoul(s.substr(i, 4).c_str(), nullptr, 16); i += 4; return true; };
                    unsigned cp; if (!hex4(cp)) return fail("bad \\u");
                    if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < s.size() && s[i] == '\\' && s[i + 1] == 'u') { i += 2; unsigned lo; if (hex4(lo)) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); }
                    out += toU8(U32(1, (char32_t)cp)); break; }
                default: out += e; break;
            }
        }
        if (i >= s.size()) return fail("unterminated string");
        i++; return true;
    }
    bool val(JV& v, int depth = 0) {
        if (depth > 64) return fail("too deep");
        ws(); if (i >= s.size()) return fail("unexpected end");
        char c = s[i];
        if (c == '{') {
            v = JV::object(); i++;
            for (;;) {
                ws(); if (i < s.size() && s[i] == '}') { i++; return true; }
                std::string k; if (!str(k)) return false; ws();
                if (i >= s.size() || s[i] != ':') return fail("':' expected"); i++;
                JV e; if (!val(e, depth + 1)) return false; v.o.emplace_back(k, std::move(e)); ws();
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == '}') { i++; return true; }
                return fail("',' or '}' expected");
            }
        }
        if (c == '[') {
            v = JV::array(); i++;
            for (;;) {
                ws(); if (i < s.size() && s[i] == ']') { i++; return true; }
                JV e; if (!val(e, depth + 1)) return false; v.a.push_back(std::move(e)); ws();
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == ']') { i++; return true; }
                return fail("',' or ']' expected");
            }
        }
        if (c == '"') { v = JV(); v.t = JV::STR; return str(v.s); }
        if (s.compare(i, 4, "true") == 0) { v = JV::boolean_(true); i += 4; return true; }
        if (s.compare(i, 5, "false") == 0) { v = JV::boolean_(false); i += 5; return true; }
        if (s.compare(i, 4, "null") == 0) { v = JV(); i += 4; return true; }
        if (c == '-' || isdigit((unsigned char)c)) { char* e = nullptr; double d = strtod(s.c_str() + i, &e); if (e == s.c_str() + i) return fail("bad number"); i = (size_t)(e - s.c_str()); v = JV::number(d); return true; }
        return fail("unexpected character");
    }
};
static bool jparse(const std::string& src, JV& out, std::string* err = nullptr) {
    JParser p(src); bool ok = p.val(out); if (ok) { p.ws(); }
    if (!ok && err) *err = p.err;
    return ok;
}
static void jescape(const std::string& s, std::string& o) {
    o += '"';
    for (unsigned char c : s) {
        switch (c) { case '"': o += "\\\""; break; case '\\': o += "\\\\"; break; case '\n': o += "\\n"; break; case '\t': o += "\\t"; break; case '\r': o += "\\r"; break;
            default: if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; } else o += (char)c; }
    }
    o += '"';
}
static void jdump(const JV& v, std::string& o, int ind = 0) {
    std::string pad((size_t)ind * 2, ' '), pad2((size_t)(ind + 1) * 2, ' ');
    switch (v.t) {
        case JV::NUL: o += "null"; break;
        case JV::BOOL: o += v.b ? "true" : "false"; break;
        case JV::NUM: { char b[48]; if (v.n == std::floor(v.n) && std::fabs(v.n) < 1e15) snprintf(b, sizeof b, "%lld", (long long)v.n); else snprintf(b, sizeof b, "%.4g", v.n); o += b; break; }
        case JV::STR: jescape(v.s, o); break;
        case JV::ARR:
            if (v.a.empty()) { o += "[]"; break; }
            o += "[\n"; for (size_t k = 0; k < v.a.size(); k++) { o += pad2; jdump(v.a[k], o, ind + 1); o += k + 1 < v.a.size() ? ",\n" : "\n"; } o += pad + "]"; break;
        case JV::OBJ:
            if (v.o.empty()) { o += "{}"; break; }
            o += "{\n"; for (size_t k = 0; k < v.o.size(); k++) { o += pad2; jescape(v.o[k].first, o); o += ": "; jdump(v.o[k].second, o, ind + 1); o += k + 1 < v.o.size() ? ",\n" : "\n"; } o += pad + "}"; break;
    }
}
// UTF-8 <-> std::filesystem::path, valid for C++17 (u8path) and C++20 (char8_t)
static std::filesystem::path fsPath(const std::string& s) {
#ifdef __cpp_char8_t
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
#else
    return std::filesystem::u8path(s);
#endif
}
static std::string fsStr(const std::filesystem::path& p) { auto u = p.u8string(); return std::string(u.begin(), u.end()); }
static bool readFile(const std::string& p, std::string& out) {
    std::ifstream f(fsPath(p), std::ios::binary); if (!f) return false;
    std::stringstream ss; ss << f.rdbuf(); out = ss.str(); return true;
}
static bool writeFile(const std::string& p, const std::string& data) {
    std::ofstream f(fsPath(p), std::ios::binary); if (!f) return false;
    f.write(data.data(), (std::streamsize)data.size()); return f.good();
}
