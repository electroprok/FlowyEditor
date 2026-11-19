#pragma once
// ------------------------------------------------------------- document ----
struct Pos {
    int l = 0, c = 0;
    bool operator==(const Pos& o) const { return l == o.l && c == o.c; }
    bool operator!=(const Pos& o) const { return !(*this == o); }
    bool operator<(const Pos& o) const { return l < o.l || (l == o.l && c < o.c); }
};
struct Snap { std::vector<U32> lines; Pos cur, anc; };

static int charCls(char32_t c) { if (c == ' ' || c == '\t') return 0; return isIdC(c) ? 1 : 2; }
static bool ceq(char32_t a, char32_t b, bool mc) {
    if (a == b) return true; if (mc) return false;
    if (a < 128 && b < 128) return tolower((int)a) == tolower((int)b);
    return false;
}
static size_t findIn(const U32& s, const U32& q, size_t from, bool mc) {
    if (q.empty() || s.size() < q.size()) return NPOS;
    for (size_t p = from; p + q.size() <= s.size(); p++) {
        size_t k = 0; while (k < q.size() && ceq(s[p + k], q[k], mc)) k++;
        if (k == q.size()) return p;
    }
    return NPOS;
}
static size_t rfindIn(const U32& s, const U32& q, long long lim, bool mc) { // last match starting at < lim
    if (q.empty() || s.size() < q.size()) return NPOS;
    long long p = std::min<long long>((long long)s.size() - (long long)q.size(), lim - 1);
    for (; p >= 0; p--) {
        size_t k = 0; while (k < q.size() && ceq(s[p + k], q[k], mc)) k++;
        if (k == q.size()) return (size_t)p;
    }
    return NPOS;
}

struct Doc {
    std::vector<U32> lines{U32()};
    Pos cur, anc; int wantCol = -1;
    std::string path; bool modified = false, crlf = false; Lang lang = L_TEXT;
    float sx = 0, sy = 0, vsx = 0, vsy = 0; std::string cwd, args; unsigned edits = 0;
    std::vector<Snap> undo, redo; Uint32 lastTick = 0; int lastKind = 0, lastLine = -1;
    std::vector<int> stStart{0}; size_t validTo = 1;

    bool multi() const { return lang >= 0 && lang < (int)LANGS.size() && LANGS[lang].multi; }
    void touch(int l) { edits++; if (l < 0) l = 0; if ((size_t)l + 1 < validTo) validTo = (size_t)l + 1; if (validTo < 1) validTo = 1; }
    int stateAt(size_t l) {
        if (!multi()) return 0;
        if (stStart.size() < lines.size() + 1) stStart.resize(lines.size() + 1, 0);
        std::vector<uint8_t> tmp;
        while (validTo <= l && validTo <= lines.size()) {
            int s = stStart[validTo - 1]; highlight(lang, lines[validTo - 1], s, tmp); stStart[validTo] = s; validTo++;
        }
        return stStart[l];
    }
    void hl(int l, std::vector<uint8_t>& tok) { int s = stateAt((size_t)l); highlight(lang, lines[l], s, tok); }
    void setLang(Lang l) { lang = l; validTo = 1; }

    // ---- undo
    void pushUndo(int kind) {
        Uint32 now = SDL_GetTicks();
        bool co = kind == 1 && lastKind == 1 && cur.l == lastLine && now - lastTick < 1500;
        if (!co) { undo.push_back({lines, cur, anc}); if (undo.size() > (lines.size() > 20000 ? 20u : 200u)) undo.erase(undo.begin()); }
        redo.clear(); lastKind = kind; lastTick = now; lastLine = cur.l; modified = true;
    }
    void doUndo() {
        if (undo.empty()) return;
        redo.push_back({lines, cur, anc}); Snap s = std::move(undo.back()); undo.pop_back();
        lines = std::move(s.lines); cur = s.cur; anc = s.anc; validTo = 1; lastKind = 0; modified = true; edits++;
    }
    void doRedo() {
        if (redo.empty()) return;
        undo.push_back({lines, cur, anc}); Snap s = std::move(redo.back()); redo.pop_back();
        lines = std::move(s.lines); cur = s.cur; anc = s.anc; validTo = 1; lastKind = 0; modified = true; edits++;
    }

    // ---- columns
    int colToVis(int l, int c) const {
        const U32& s = lines[l]; int v = 0;
        for (int i = 0; i < c && i < (int)s.size(); i++) v += s[i] == '\t' ? TABW - v % TABW : 1;
        return v;
    }
    int visToCol(int l, double v) const {
        const U32& s = lines[l]; int c = 0; int vis = 0;
        while (c < (int)s.size()) {
            int w = s[c] == '\t' ? TABW - vis % TABW : 1;
            if (v < vis + w / 2.0) break;
            vis += w; c++;
        }
        return c;
    }

    // ---- selection
    bool hasSel() const { return cur != anc; }
    void selRange(Pos& a, Pos& b) const { if (cur < anc) { a = cur; b = anc; } else { a = anc; b = cur; } }
    U32 textBetween(Pos a, Pos b) const {
        if (a.l == b.l) return lines[a.l].substr(a.c, b.c - a.c);
        U32 r = lines[a.l].substr(a.c);
        for (int i = a.l + 1; i < b.l; i++) { r += U'\n'; r += lines[i]; }
        r += U'\n'; r += lines[b.l].substr(0, b.c);
        return r;
    }
    U32 selText() const { Pos a, b; selRange(a, b); return textBetween(a, b); }
    void eraseRange(Pos a, Pos b) {
        if (a.l == b.l) lines[a.l].erase(a.c, b.c - a.c);
        else { lines[a.l] = lines[a.l].substr(0, a.c) + lines[b.l].substr(b.c); lines.erase(lines.begin() + a.l + 1, lines.begin() + b.l + 1); }
        cur = anc = a; touch(a.l);
    }
    bool deleteSel() { if (!hasSel()) return false; Pos a, b; selRange(a, b); eraseRange(a, b); return true; }
    void insertRaw(const U32& t) {
        int start = cur.l; U32 tail = lines[cur.l].substr(cur.c); lines[cur.l].erase(cur.c);
        size_t s = 0; int ln = cur.l;
        for (;;) {
            size_t nl = t.find(U'\n', s);
            if (nl == NPOS) { lines[ln] += t.substr(s); cur.c = (int)lines[ln].size(); lines[ln] += tail; break; }
            lines[ln] += t.substr(s, nl - s); lines.insert(lines.begin() + ln + 1, U32()); ln++; s = nl + 1;
        }
        cur.l = ln; anc = cur; touch(start);
    }
    void insertText(const U32& t) { pushUndo(2); deleteSel(); insertRaw(t); wantCol = -1; }
    void selectAll() { anc = {0, 0}; cur = {(int)lines.size() - 1, (int)lines.back().size()}; }
    void selectWord(Pos p) {
        const U32& s = lines[p.l]; int n = (int)s.size(); if (n == 0) { cur = anc = p; return; }
        int c = std::min(p.c, n - 1); if (p.c >= n) c = n - 1;
        int k = charCls(s[c]); int a = c, b = c;
        while (a > 0 && charCls(s[a - 1]) == k) a--;
        while (b < n && charCls(s[b]) == k) b++;
        anc = {p.l, a}; cur = {p.l, b};
    }
    void selectLine(int l) { anc = {l, 0}; cur = (l + 1 < (int)lines.size()) ? Pos{l + 1, 0} : Pos{l, (int)lines[l].size()}; }

    // ---- movement
    void setCur(Pos p, bool ext) { cur = p; if (!ext) anc = p; }
    int wordLeft(int l, int c) const {
        const U32& s = lines[l]; while (c > 0 && charCls(s[c - 1]) == 0) c--;
        if (c > 0) { int k = charCls(s[c - 1]); while (c > 0 && charCls(s[c - 1]) == k) c--; }
        return c;
    }
    int wordRight(int l, int c) const {
        const U32& s = lines[l]; int n = (int)s.size(); while (c < n && charCls(s[c]) == 0) c++;
        if (c < n) { int k = charCls(s[c]); while (c < n && charCls(s[c]) == k) c++; }
        return c;
    }
    void left(bool ext, bool word) {
        wantCol = -1;
        if (!ext && hasSel()) { Pos a, b; selRange(a, b); cur = anc = a; return; }
        Pos p = cur;
        if (p.c == 0) { if (p.l > 0) { p.l--; p.c = (int)lines[p.l].size(); } }
        else p.c = word ? wordLeft(p.l, p.c) : p.c - 1;
        setCur(p, ext);
    }
    void right(bool ext, bool word) {
        wantCol = -1;
        if (!ext && hasSel()) { Pos a, b; selRange(a, b); cur = anc = b; return; }
        Pos p = cur;
        if (p.c >= (int)lines[p.l].size()) { if (p.l + 1 < (int)lines.size()) { p.l++; p.c = 0; } }
        else p.c = word ? wordRight(p.l, p.c) : p.c + 1;
        setCur(p, ext);
    }
    void vert(int dl, bool ext) {
        int vc = wantCol >= 0 ? wantCol : colToVis(cur.l, cur.c); int nl = cur.l + dl;
        if (nl < 0) { setCur({0, 0}, ext); wantCol = -1; return; }
        if (nl >= (int)lines.size()) { setCur({(int)lines.size() - 1, (int)lines.back().size()}, ext); wantCol = -1; return; }
        setCur({nl, visToCol(nl, vc)}, ext); wantCol = vc;
    }
    void home(bool ext) {
        wantCol = -1; const U32& s = lines[cur.l]; int fn = 0; while (fn < (int)s.size() && (s[fn] == ' ' || s[fn] == '\t')) fn++;
        setCur({cur.l, cur.c == fn ? 0 : fn}, ext);
    }
    void end(bool ext) { wantCol = -1; setCur({cur.l, (int)lines[cur.l].size()}, ext); }

    // ---- editing
    void typeChar(char32_t c) {
        if (hasSel()) { pushUndo(2); deleteSel(); } else pushUndo(1);
        wantCol = -1;
        const LangDef& LD = LANGS[lang]; U32& L = lines[cur.l]; int n = (int)L.size();
        char32_t nx = cur.c < n ? L[cur.c] : 0, pv = cur.c > 0 ? L[cur.c - 1] : 0;
        const std::string& pr = LD.pairs; bool ac = g_autoClose && c < 128;
        if (ac && nx == c) for (size_t k = 1; k < pr.size(); k += 2) if (pr[k] == (char)c) { cur.c++; anc = cur; return; }
        char32_t close = 0;
        if (ac) for (size_t k = 0; k + 1 < pr.size(); k += 2) if (pr[k] == (char)c) { close = (char32_t)(unsigned char)pr[k + 1]; if (pr[k] == pr[k + 1] && isIdC(pv)) close = 0; break; }
        if (close && nx != 0 && isIdC(nx)) close = 0;
        if (ac && !LD.dedent.empty() && LD.dedent.find((char)c) != NPOS && cur.l + 1 < (int)lines.size()) {   // typed "}" on an empty line right above an auto-inserted "}": hop onto it
            bool ws = true; for (char32_t ch : L) if (ch != ' ' && ch != '\t') { ws = false; break; }
            if (ws) { size_t f = lines[cur.l + 1].find_first_not_of(U" \t"); if (f != NPOS && lines[cur.l + 1][f] == c) { lines.erase(lines.begin() + cur.l); cur.c = (int)f + 1; anc = cur; touch(cur.l); return; } }
        }
        L.insert(cur.c, 1, c); if (close) L.insert(cur.c + 1, 1, close);
        cur.c++; anc = cur;
        if (c < 128 && !LD.dedent.empty() && LD.dedent.find((char)c) != NPOS) {   // auto-dedent closing brace
            bool ws = true; for (int i = 0; i < cur.c - 1; i++) if (L[i] != ' ' && L[i] != '\t') ws = false;
            if (ws && cur.c > 1) {
                int rm = 0;
                if (L[0] == '\t') rm = 1; else { while (rm < TABW && rm < cur.c - 1 && L[rm] == ' ') rm++; }
                L.erase(0, rm); cur.c -= rm; anc = cur;
            }
        }
        touch(cur.l);
    }
    void typeText(const U32& t) { for (char32_t c : t) { if (c == '\n') newline(); else if (c >= 32 || c == '\t') typeChar(c); } }
    void newline() {
        pushUndo(2); deleteSel(); wantCol = -1;
        const LangDef& LD = LANGS[lang];
        U32 L = lines[cur.l]; U32 before = L.substr(0, cur.c), after = L.substr(cur.c);
        U32 ind; for (char32_t ch : before) { if (ch == ' ' || ch == '\t') ind += ch; else break; }
        size_t e = before.find_last_not_of(U" \t"); U32 tb = e == NPOS ? U32() : before.substr(0, e + 1);
        size_t fs = after.find_first_not_of(U" \t"); after = fs == NPOS ? U32() : after.substr(fs);
        bool extra = false, split = false;
        std::string t = toU8(tb);
        if (LD.special == LS_LUA) {
            size_t b = t.find_first_not_of(" \t"); if (b != NPOS) t = t.substr(b);
            auto ew = [&](const char* w) { size_t k = strlen(w); return t.size() >= k && t.compare(t.size() - k, k, w) == 0 && (t.size() == k || !isalnum((unsigned char)t[t.size() - k - 1])); };
            extra = ew("then") || ew("do") || ew("repeat") || t == "else" || (t.find("function") != NPOS && !t.empty() && t.back() == ')') || ew("{");
        } else {
            for (auto& sfx : LD.indentAfter) {
                size_t k = sfx.size();
                if (t.size() >= k && t.compare(t.size() - k, k, sfx) == 0 && (!isalnum((unsigned char)sfx.back()) || t.size() == k || !isalnum((unsigned char)t[t.size() - k - 1]))) { extra = true; break; }
            }
        }
        if (!tb.empty() && !after.empty()) { char32_t o = tb.back(), cl = o == '{' ? '}' : o == '[' ? ']' : o == '(' ? ')' : 0; split = cl && after[0] == cl; }
        U32 unit(TABW, U' ');
        lines[cur.l] = before;
        if (split) { lines.insert(lines.begin() + cur.l + 1, ind + unit); lines.insert(lines.begin() + cur.l + 2, ind + after); cur = {cur.l + 1, (int)(ind.size() + unit.size())}; }
        else { U32 nl = ind + (extra ? unit : U32()); cur = {cur.l + 1, (int)nl.size()}; lines.insert(lines.begin() + cur.l, nl + after); }
        anc = cur; touch(cur.l - 1);
    }
    void backspace(bool word) {
        if (hasSel()) { pushUndo(2); deleteSel(); return; }
        wantCol = -1;
        if (cur.c == 0) {
            if (cur.l == 0) return; pushUndo(2);
            int pl = cur.l - 1, len = (int)lines[pl].size(); lines[pl] += lines[cur.l]; lines.erase(lines.begin() + cur.l);
            cur = {pl, len}; anc = cur; touch(pl); return;
        }
        pushUndo(1); U32& L = lines[cur.l]; int st = cur.c - 1, cnt = 1;
        if (word) { st = wordLeft(cur.l, cur.c); cnt = cur.c - st; }
        else if (cur.c < (int)L.size()) {
            char32_t a = L[cur.c - 1], b = L[cur.c];
            if ((a == '(' && b == ')') || (a == '[' && b == ']') || (a == '{' && b == '}') || (a == '"' && b == '"') || (a == '\'' && b == '\'')) cnt = 2;
        }
        L.erase(st, cnt); cur.c = st; anc = cur; touch(cur.l);
    }
    void del(bool word) {
        if (hasSel()) { pushUndo(2); deleteSel(); return; }
        wantCol = -1; U32& L = lines[cur.l];
        if (cur.c >= (int)L.size()) {
            if (cur.l + 1 >= (int)lines.size()) return; pushUndo(2);
            L += lines[cur.l + 1]; lines.erase(lines.begin() + cur.l + 1); touch(cur.l); return;
        }
        pushUndo(1); int e = word ? wordRight(cur.l, cur.c) : cur.c + 1; L.erase(cur.c, e - cur.c); touch(cur.l);
    }
    void lineRange(int& a, int& b) const { Pos p, q; selRange(p, q); a = p.l; b = q.l; if (b > a && q.c == 0) b--; }
    void indentSel(bool out) {
        pushUndo(2); int a, b; lineRange(a, b);
        for (int l = a; l <= b; l++) {
            U32& s = lines[l]; int delta = 0;
            if (!out) { if (!s.empty() || a == b) { s.insert(0, U32(TABW, U' ')); delta = TABW; } }
            else { int r = 0; if (!s.empty() && s[0] == '\t') r = 1; else while (r < TABW && r < (int)s.size() && s[r] == ' ') r++; s.erase(0, r); delta = -r; }
            if (cur.l == l) cur.c = std::max(0, cur.c + delta);
            if (anc.l == l) anc.c = std::max(0, anc.c + delta);
        }
        touch(a);
    }
    void toggleComment() {
        const LangDef& LD = LANGS[lang]; if (LD.lineC.empty()) return;
        U32 pre = toU32(LD.lineC[0]);
        pushUndo(2); int a, b; lineRange(a, b); bool all = true; int mi = INT_MAX; bool any = false;
        for (int l = a; l <= b; l++) {
            const U32& s = lines[l]; size_t f = s.find_first_not_of(U" \t"); if (f == NPOS) continue;
            any = true; mi = std::min(mi, (int)f); if (s.compare(f, pre.size(), pre) != 0) all = false;
        }
        if (!any) return;
        for (int l = a; l <= b; l++) {
            U32& s = lines[l]; size_t f = s.find_first_not_of(U" \t"); if (f == NPOS) continue;
            int d = 0;
            if (all) { size_t r = pre.size(); if (f + r < s.size() && s[f + r] == ' ') r++; s.erase(f, r); d = -(int)r; }
            else { s.insert(mi, pre + U" "); d = (int)pre.size() + 1; }
            int pos = all ? (int)f : mi;
            if (cur.l == l && cur.c > pos) cur.c = std::max(pos, cur.c + d);
            if (anc.l == l && anc.c > pos) anc.c = std::max(pos, anc.c + d);
        }
        touch(a);
    }
    void duplicate() {
        pushUndo(2);
        if (hasSel()) { Pos a, b; selRange(a, b); U32 t = textBetween(a, b); cur = anc = b; insertRaw(t); anc = b; }
        else { U32 t = lines[cur.l]; lines.insert(lines.begin() + cur.l + 1, t); cur.l++; anc = cur; touch(cur.l - 1); }
    }
    void deleteLines() {
        pushUndo(2); int a, b; lineRange(a, b); lines.erase(lines.begin() + a, lines.begin() + b + 1);
        if (lines.empty()) lines.push_back(U32());
        cur = anc = {std::min(a, (int)lines.size() - 1), 0}; touch(a);
    }
    void moveLines(int dir) {
        int a, b; lineRange(a, b);
        if (dir < 0 && a == 0) return; if (dir > 0 && b == (int)lines.size() - 1) return;
        pushUndo(2);
        if (dir < 0) { U32 t = lines[a - 1]; lines.erase(lines.begin() + a - 1); lines.insert(lines.begin() + b, t); }
        else { U32 t = lines[b + 1]; lines.erase(lines.begin() + b + 1); lines.insert(lines.begin() + a, t); }
        cur.l += dir; anc.l += dir; touch(std::min(a, a + dir));
    }

    // ---- search
    bool find(const U32& q, bool fwd, bool mc) {
        if (q.empty()) return false;
        int n = (int)lines.size(); Pos a, b; selRange(a, b);
        if (fwd) {
            for (int k = 0; k <= n; k++) {
                int ll = (b.l + k) % n; size_t p = findIn(lines[ll], q, k == 0 ? (size_t)b.c : 0, mc);
                if (p != NPOS) { anc = {ll, (int)p}; cur = {ll, (int)(p + q.size())}; return true; }
            }
        } else {
            for (int k = 0; k <= n; k++) {
                int ll = ((a.l - k) % n + n) % n; long long lim = k == 0 ? a.c : (long long)lines[ll].size() + 1;
                size_t p = rfindIn(lines[ll], q, lim, mc);
                if (p != NPOS) { anc = {ll, (int)p}; cur = {ll, (int)(p + q.size())}; return true; }
            }
        }
        return false;
    }
    bool replaceOne(const U32& q, const U32& r, bool mc) {
        if (hasSel()) {
            U32 t = selText();
            if (t.size() == q.size() && findIn(t, q, 0, mc) == 0) { pushUndo(2); Pos a, b; selRange(a, b); eraseRange(a, b); insertRaw(r); }
        }
        return find(q, true, mc);
    }
    int replaceAll(const U32& q, const U32& r, bool mc) {
        if (q.empty()) return 0; int cnt = 0; pushUndo(2);
        for (auto& s : lines) {
            size_t p = 0; U32 out; size_t last = 0; bool ch = false;
            while ((p = findIn(s, q, p, mc)) != NPOS) { out += s.substr(last, p - last); out += r; p += q.size(); last = p; cnt++; ch = true; }
            if (ch) { out += s.substr(last); s = out; }
        }
        cur = anc = {0, 0}; validTo = 1; return cnt;
    }

    // ---- bracket matching
    bool matchBracket(Pos& a, Pos& b) const {
        const U32& L = lines[cur.l];
        for (int side = 0; side < 2; side++) {
            int c = side == 0 ? cur.c : cur.c - 1; if (c < 0 || c >= (int)L.size()) continue;
            char32_t ch = L[c]; const char* open = "([{"; const char* close = ")]}"; int idx = -1; bool fwd = true;
            for (int i = 0; i < 3; i++) { if (ch == (char32_t)open[i]) { idx = i; fwd = true; } if (ch == (char32_t)close[i]) { idx = i; fwd = false; } }
            if (idx < 0) continue;
            char32_t o = open[idx], cl = close[idx]; int depth = 0, budget = 30000; int l = cur.l, i = c;
            while (budget-- > 0) {
                const U32& s = lines[l];
                if (i >= 0 && i < (int)s.size()) {
                    if (s[i] == (fwd ? o : cl)) depth++; else if (s[i] == (fwd ? cl : o)) { depth--; if (depth == 0) { a = {cur.l, c}; b = {l, i}; return true; } }
                }
                if (fwd) { i++; if (i >= (int)s.size() + 1) { l++; i = 0; if (l >= (int)lines.size()) break; } }
                else { i--; if (i < 0) { l--; if (l < 0) break; i = (int)lines[l].size() - 1; } }
            }
        }
        return false;
    }

    // ---- file io
    bool load(const std::string& p) {
        std::ifstream f(p, std::ios::binary); if (!f) return false;
        std::stringstream ss; ss << f.rdbuf(); std::string data = ss.str();
        if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF) data.erase(0, 3);
        crlf = data.find("\r\n") != NPOS; U32 all = toU32(data); lines.clear(); U32 cl;
        for (char32_t c : all) { if (c == '\n') { lines.push_back(cl); cl.clear(); } else if (c != '\r') cl += c; }
        lines.push_back(cl); path = p; modified = false; cur = anc = {0, 0}; validTo = 1; undo.clear(); redo.clear(); lang = langFromPath(p);
        return true;
    }
    bool save() {
        std::ofstream f(path, std::ios::binary); if (!f) return false;
        std::string out;
        for (size_t i = 0; i < lines.size(); i++) { out += toU8(lines[i]); if (i + 1 < lines.size()) out += crlf ? "\r\n" : "\n"; }
        f.write(out.data(), (std::streamsize)out.size()); modified = !f.good(); return f.good();
    }
    std::string name() const { if (path.empty()) return "untitled"; size_t p = path.find_last_of("/\\"); return p == NPOS ? path : path.substr(p + 1); }
};

