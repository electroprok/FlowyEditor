#pragma once
#if defined(__has_include)
#if __has_include(<SDL_image.h>)
#include <SDL_image.h>
#define FLOWY_IMG 1
#endif
#endif
#ifndef FLOWY_IMG
#define FLOWY_IMG 0
#endif

// -------------------------------------------------------------- colors ----
static SDL_Color C(int r, int g, int b, int a = 255) { return SDL_Color{(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a}; }
static SDL_Color withA(SDL_Color c, float k) { c.a = (Uint8)std::max(0.f, std::min(255.f, c.a * k)); return c; }
static SDL_Color mix(SDL_Color a, SDL_Color b, float t) {
    return C((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t), (int)(a.a + (b.a - a.a) * t));
}
static bool parseHex(const std::string& h, SDL_Color& c) {
    std::string s = h; if (!s.empty() && s[0] == '#') s.erase(0, 1);
    if (s.size() != 6 && s.size() != 8) return false;
    for (char ch : s) if (!isxdigit((unsigned char)ch)) return false;
    auto b = [&](size_t i) { return (Uint8)strtol(s.substr(i, 2).c_str(), nullptr, 16); };
    c.r = b(0); c.g = b(2); c.b = b(4); c.a = s.size() == 8 ? b(6) : 255; return true;
}
static std::string toHex(SDL_Color c) {
    char buf[16]; if (c.a == 255) snprintf(buf, sizeof buf, "#%02x%02x%02x", c.r, c.g, c.b); else snprintf(buf, sizeof buf, "#%02x%02x%02x%02x", c.r, c.g, c.b, c.a);
    return buf;
}

// -------------------------------------------------------------- themes ----
enum CI { C_BG, C_FG, C_GUTTER, C_GUTTERFG, C_CURLINE, C_SEL, C_CURSOR, C_BAR, C_BARFG, C_ACCENT, C_POPUP, C_MATCH, C_BTN, C_GOOD, C_WARN, C_BAD, C_TOK0, NCOL = (int)C_TOK0 + (int)T_COUNT };
static const char* COLOR_KEYS[NCOL] = {"bg", "fg", "gutter", "gutterFg", "curLine", "sel", "cursor", "bar", "barFg", "accent", "popup", "match", "btn", "good", "warn", "bad",
    "text", "keyword", "type", "function", "call", "number", "string", "comment", "preproc", "register", "label", "operator", "directive"};
static const char* COLOR_LABELS[NCOL] = {"Background", "Text", "Gutter", "Line numbers", "Current line", "Selection", "Cursor", "Bars", "Bar text", "Accent", "Popups", "Search match", "Buttons", "Success", "Warning", "Error",
    "Code: plain", "Code: keyword", "Code: type", "Code: function", "Code: call", "Code: number", "Code: string", "Code: comment", "Code: preprocessor", "Code: register", "Code: label", "Code: operator", "Code: directive"};
struct Theme { std::string name; SDL_Color c[NCOL]; };
static std::vector<Theme> g_themes;
static int findTheme(const std::string& n) { for (size_t i = 0; i < g_themes.size(); i++) if (lowerS(g_themes[i].name) == lowerS(n)) return (int)i; return -1; }
static void loadThemeJson(const JV& j) {
    std::string name = j.str("name"); if (name.empty()) return;
    Theme t; int b = findTheme(j.str("base", "One Dark"));
    if (b >= 0) t = g_themes[b]; else for (auto& c : t.c) c = C(128, 128, 128);
    t.name = name;
    if (auto* cs = j.get("colors")) if (cs->t == JV::OBJ) for (auto& kv : cs->o) for (int i = 0; i < NCOL; i++)
        if (kv.first == COLOR_KEYS[i] && kv.second.t == JV::STR) { SDL_Color c; if (parseHex(kv.second.s, c)) t.c[i] = c; }
    int e = findTheme(name); if (e >= 0) g_themes[e] = t; else g_themes.push_back(t);
}

// ------------------------------------------------------------ settings ----
struct Settings {
    std::string theme = "One Dark", fontFile, bgImage, lastDir;
    std::map<std::string, std::string> ov, runCmd;
    float fontScale = 1, lineSpace = 0.25f, radius = 8, animSpeed = 1, kbScale = 1;
    float bgOpacity = 1, bgDim = 0.35f, bgBlur = 0, tintStr = 0, paneAlpha = 0.82f;
    int tabW = 4, caret = 0, bgMode = 0, kbMode = FLOWY_MOBILE ? 1 : 0;
    bool lineNums = true, guides = true, whitespace = false, curLine = true, bracket = true, autoClose = true, smooth = true, anim = true,
         scrollbar = true, blink = true, bgDrift = false, toolbar = FLOWY_MOBILE ? true : false, autoSave = true;
    SDL_Color tint{80, 120, 200, 255};
    std::vector<std::string> recent;
};
static JV settingsToJson(const Settings& s) {
    JV j = JV::object();
    auto S = [&](const char* k, const std::string& v) { j.set(k, JV::string(v)); };
    auto N = [&](const char* k, double v) { j.set(k, JV::number(v)); };
    auto B = [&](const char* k, bool v) { j.set(k, JV::boolean_(v)); };
    S("theme", s.theme); S("fontFile", s.fontFile); S("bgImage", s.bgImage); S("lastDir", s.lastDir); S("tint", toHex(s.tint));
    N("fontScale", s.fontScale); N("lineSpace", s.lineSpace); N("radius", s.radius); N("animSpeed", s.animSpeed); N("kbScale", s.kbScale);
    N("bgOpacity", s.bgOpacity); N("bgDim", s.bgDim); N("bgBlur", s.bgBlur); N("tintStr", s.tintStr); N("paneAlpha", s.paneAlpha);
    N("tabWidth", s.tabW); N("caret", s.caret); N("bgMode", s.bgMode); N("kbMode", s.kbMode);
    B("lineNumbers", s.lineNums); B("indentGuides", s.guides); B("whitespace", s.whitespace); B("currentLine", s.curLine); B("bracketMatch", s.bracket);
    B("autoClose", s.autoClose); B("smoothScroll", s.smooth); B("animations", s.anim); B("scrollbar", s.scrollbar); B("caretBlink", s.blink);
    B("bgDrift", s.bgDrift); B("toolbar", s.toolbar); B("autoSaveOnRun", s.autoSave);
    JV ov = JV::object(); for (auto& kv : s.ov) ov.set(kv.first, JV::string(kv.second)); j.set("colorOverrides", ov);
    JV rc = JV::object(); for (auto& kv : s.runCmd) rc.set(kv.first, JV::string(kv.second)); j.set("runCommands", rc);
    JV rec = JV::array(); for (auto& r : s.recent) rec.a.push_back(JV::string(r)); j.set("recent", rec);
    return j;
}
static void settingsFromJson(Settings& s, const JV& j) {
    s.theme = j.str("theme", s.theme); s.fontFile = j.str("fontFile", s.fontFile); s.bgImage = j.str("bgImage", s.bgImage); s.lastDir = j.str("lastDir", s.lastDir);
    { SDL_Color c; if (parseHex(j.str("tint"), c)) s.tint = c; }
    auto F = [&](const char* k, float& v) { v = (float)j.num(k, v); };
    auto I = [&](const char* k, int& v) { v = (int)j.num(k, v); };
    auto B = [&](const char* k, bool& v) { v = j.boolean(k, v); };
    F("fontScale", s.fontScale); F("lineSpace", s.lineSpace); F("radius", s.radius); F("animSpeed", s.animSpeed); F("kbScale", s.kbScale);
    F("bgOpacity", s.bgOpacity); F("bgDim", s.bgDim); F("bgBlur", s.bgBlur); F("tintStr", s.tintStr); F("paneAlpha", s.paneAlpha);
    I("tabWidth", s.tabW); I("caret", s.caret); I("bgMode", s.bgMode); I("kbMode", s.kbMode);
    B("lineNumbers", s.lineNums); B("indentGuides", s.guides); B("whitespace", s.whitespace); B("currentLine", s.curLine); B("bracketMatch", s.bracket);
    B("autoClose", s.autoClose); B("smoothScroll", s.smooth); B("animations", s.anim); B("scrollbar", s.scrollbar); B("caretBlink", s.blink);
    B("bgDrift", s.bgDrift); B("toolbar", s.toolbar); B("autoSaveOnRun", s.autoSave);
    s.fontScale = std::min(3.f, std::max(0.5f, s.fontScale)); s.tabW = std::min(8, std::max(1, s.tabW));
    if (auto* o = j.get("colorOverrides")) if (o->t == JV::OBJ) for (auto& kv : o->o) if (kv.second.t == JV::STR) s.ov[kv.first] = kv.second.s;
    if (auto* o = j.get("runCommands")) if (o->t == JV::OBJ) for (auto& kv : o->o) if (kv.second.t == JV::STR) s.runCmd[kv.first] = kv.second.s;
    s.recent = j.strs("recent");
}

// -------------------------------------------------------------- images ----
static SDL_Surface* loadImg(const std::string& p) {
#if FLOWY_IMG
    return IMG_Load(p.c_str());
#else
    return SDL_LoadBMP(p.c_str());
#endif
}
static SDL_Surface* toRGBA(SDL_Surface* s) {
    if (!s) return nullptr;
    SDL_Surface* c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0); SDL_FreeSurface(s); return c;
}
static SDL_Surface* boxDown(SDL_Surface* src, int f) {   // average f x f blocks (cheap, decent blur when f is large)
    if (!src || f <= 1) return src;
    int nw = std::max(1, src->w / f), nh = std::max(1, src->h / f);
    SDL_Surface* d = SDL_CreateRGBSurfaceWithFormat(0, nw, nh, 32, SDL_PIXELFORMAT_RGBA32);
    if (!d) return src;
    for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) {
        unsigned r = 0, g = 0, b = 0, a = 0, cnt = 0;
        for (int yy = y * f; yy < std::min(src->h, (y + 1) * f); yy++) {
            const Uint8* p = (const Uint8*)src->pixels + (size_t)yy * src->pitch + (size_t)x * f * 4;
            for (int xx = x * f; xx < std::min(src->w, (x + 1) * f); xx++, p += 4) { r += p[0]; g += p[1]; b += p[2]; a += p[3]; cnt++; }
        }
        if (!cnt) cnt = 1;
        Uint8* q = (Uint8*)d->pixels + (size_t)y * d->pitch + (size_t)x * 4; q[0] = (Uint8)(r / cnt); q[1] = (Uint8)(g / cnt); q[2] = (Uint8)(b / cnt); q[3] = (Uint8)(a / cnt);
    }
    SDL_FreeSurface(src); return d;
}
// load + limit to maxDim on the long side, then extra blur factor
static SDL_Surface* loadScaled(const std::string& path, int maxDim, int extra) {
    SDL_Surface* s = toRGBA(loadImg(path)); if (!s) return nullptr;
    int m = std::max(s->w, s->h); int f = std::max(1, (m + maxDim - 1) / maxDim) * std::max(1, extra);
    f = std::min(f, std::max(1, std::min(s->w, s->h)));
    return boxDown(s, f);
}

struct ThumbLoader {   // decodes images on a worker thread so the UI never stalls
    std::mutex mu; std::condition_variable cv; std::deque<std::string> q;
    std::vector<std::pair<std::string, SDL_Surface*>> done; std::thread th; bool quit = false, started = false;
    void start() {
        if (started) return; started = true;
        th = std::thread([this] {
            for (;;) {
                std::string p;
                { std::unique_lock<std::mutex> l(mu); cv.wait(l, [&] { return quit || !q.empty(); }); if (quit) return; p = q.front(); q.pop_front(); }
                SDL_Surface* s = loadScaled(p, 192, 1);
                std::lock_guard<std::mutex> l(mu); done.emplace_back(p, s);
            }
        });
    }
    void request(const std::string& p) { { std::lock_guard<std::mutex> l(mu); q.push_back(p); } cv.notify_one(); }
    void clearQueue() { std::lock_guard<std::mutex> l(mu); q.clear(); }
    bool pop(std::string& p, SDL_Surface*& s) { std::lock_guard<std::mutex> l(mu); if (done.empty()) return false; p = done.back().first; s = done.back().second; done.pop_back(); return true; }
    void stop() {
        if (!started) return;
        { std::lock_guard<std::mutex> l(mu); quit = true; } cv.notify_all(); th.join();
        for (auto& d : done) if (d.second) SDL_FreeSurface(d.second);
        started = false;
    }
};
