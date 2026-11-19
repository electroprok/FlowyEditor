#pragma once
// ===================================================================== init ====
void App::startTextInput() { if (mobile && st.kbMode == 1) SDL_StopTextInput(); else SDL_StartTextInput(); }

void App::initRegistries() {
    initLangs();
    for (const char* const* p = BUILTIN_LANGS; *p; ++p) { JV j; std::string e; if (jparse(*p, j, &e)) loadLangJson(j); else fprintf(stderr, "builtin language: %s\n", e.c_str()); }
    g_themes.clear();
    for (const char* const* p = BUILTIN_THEMES; *p; ++p) { JV j; if (jparse(*p, j)) loadThemeJson(j); }
}
void App::loadSettings() { std::string s; if (readFile(joinPath(prefDir, "settings.json"), s)) { JV j; if (jparse(s, j)) settingsFromJson(st, j); } }
void App::saveSettings() { std::string o; jdump(settingsToJson(st), o); o += "\n"; writeFile(joinPath(prefDir, "settings.json"), o); settingsDirty = false; }
void App::settingsChanged() { settingsDirty = true; settingsDirtyAt = SDL_GetTicks(); }
void App::applyTheme() {
    int i = findTheme(st.theme); if (i < 0) i = 0; T = g_themes[i];
    for (auto& kv : st.ov) for (int k = 0; k < NCOL; k++) if (kv.first == COLOR_KEYS[k]) { SDL_Color c; if (parseHex(kv.second, c)) T.c[k] = c; }
}
void App::applySettings() {
    TABW = st.tabW; g_autoClose = st.autoClose; applyTheme(); loadFont(); loadBackground(); startTextInput();
    for (auto& d : docs) d.validTo = 1;
    follow = true;
}
bool App::calcScale() {
    int ow, oh, ww, wh; SDL_GetRendererOutputSize(ren, &ow, &oh); SDL_GetWindowSize(win, &ww, &wh); float s = 1;
    if (mobile) { float dpi = 0; if (SDL_GetDisplayDPI(0, &dpi, nullptr, nullptr) != 0 || dpi < 50) dpi = 320; s = dpi / 160.f; }
    else s = ww > 0 ? (float)ow / ww : 1.f;
    bool ch = std::fabs(s - uiScale) > 0.01f; uiScale = s; return ch;
}
bool App::loadFont() {
    if (font) { TTF_CloseFont(font); font = nullptr; }
    for (auto& g : glyphs) if (g.second.t) SDL_DestroyTexture(g.second.t);
    glyphs.clear();
    float base = mobile ? 14.f : 16.f; fontPx = std::max(8, (int)std::lround(base * uiScale * st.fontScale));
    std::vector<std::string> cand; if (!st.fontFile.empty()) cand.push_back(st.fontFile);
    for (const char* p : {"font.ttf", "assets/font.ttf", "fonts/font.ttf"}) cand.push_back(p);
    cand.push_back(joinPath(baseDir, "font.ttf")); cand.push_back(joinPath(baseDir, "assets/font.ttf")); cand.push_back(joinPath(prefDir, "font.ttf"));
    for (const char* p : {"C:/Windows/Fonts/consola.ttf", "C:/Windows/Fonts/cour.ttf", "/System/Library/Fonts/Menlo.ttc", "/System/Library/Fonts/Monaco.ttf",
                          "/Library/Fonts/Courier New.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
                          "/usr/share/fonts/dejavu/DejaVuSansMono.ttf", "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
                          "/usr/share/fonts/liberation/LiberationMono-Regular.ttf", "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
                          "/usr/share/fonts/noto/NotoSansMono-Regular.ttf", "/system/fonts/DroidSansMono.ttf", "/system/fonts/RobotoMono-Regular.ttf", "/system/fonts/CutiveMono.ttf"})
        cand.push_back(p);
    for (auto& p : cand) { font = TTF_OpenFont(p.c_str(), fontPx); if (font) break; }
    if (!font) return false;
    int adv = 8; TTF_GlyphMetrics(font, 'M', nullptr, nullptr, nullptr, nullptr, &adv);
    cw = std::max(4, adv); fh = TTF_FontHeight(font); lh = fh + std::max(2, (int)(fontPx * st.lineSpace)); pad = std::max(4, lh / 4);
    return true;
}
void App::loadBackground() {
    if (bgTex) { SDL_DestroyTexture(bgTex); bgTex = nullptr; }
    bgLoaded.clear(); if (st.bgImage.empty()) return;
    SDL_Surface* s = loadScaled(st.bgImage, 2048, 1 + (int)(st.bgBlur * 2));
    if (!s) { say("Cannot load image: " + baseOf(st.bgImage)); return; }
    bgW = s->w; bgH = s->h; bgTex = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
    if (bgTex) SDL_SetTextureBlendMode(bgTex, SDL_BLENDMODE_BLEND);
    bgLoaded = st.bgImage; bgLoadedBlur = (int)(st.bgBlur * 10);
}
void App::buildPlaces() {
    places.clear(); auto add = [&](const std::string& n, const std::string& p) { if (!p.empty() && isDirPath(p)) { for (auto& e : places) if (e.second == p) return; places.push_back({n, p}); } };
#if defined(__ANDROID__)
    if (const char* p = SDL_AndroidGetInternalStoragePath()) add("App storage", p);
    if (const char* p = SDL_AndroidGetExternalStoragePath()) add("App files", p);
    add("Phone storage", "/sdcard"); add("Pictures", "/sdcard/Pictures"); add("Camera", "/sdcard/DCIM"); add("Download", "/sdcard/Download"); add("Documents", "/sdcard/Documents");
#else
    add("Home", homeDir); for (const char* n : {"Documents", "Downloads", "Desktop", "Pictures"}) add(n, joinPath(homeDir, n));
    add("App data", prefDir);
#endif
#if defined(_WIN32)
    for (char c = 'C'; c <= 'Z'; c++) { std::string r = std::string(1, c) + ":/"; add(std::string(1, c) + ":", r); }
#else
    add("Root", "/");
#endif
}
bool App::init(int argc, char** argv) {
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0"); SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0"); SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return false; }
    if (TTF_Init() != 0) { fprintf(stderr, "TTF: %s\n", TTF_GetError()); return false; }
#if FLOWY_IMG
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);
#endif
    Uint32 fl = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI; if (mobile) fl |= SDL_WINDOW_FULLSCREEN;
    win = SDL_CreateWindow("FlowyEditor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1180, 760, fl); if (!win) return false;
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) return false;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    if (char* pp = SDL_GetPrefPath("FlowyEditor", "FlowyEditor")) { prefDir = pp; SDL_free(pp); }
    if (char* bp = SDL_GetBasePath()) { baseDir = bp; SDL_free(bp); }
    if (const char* h = getenv("HOME")) homeDir = h; else if (const char* u = getenv("USERPROFILE")) homeDir = u;
    std::error_code ec; fs::create_directories(fsPath(joinPath(prefDir, "plugins")), ec);
    initRegistries(); loadSettings(); loadPlugins(); applyTheme(); TABW = st.tabW; g_autoClose = st.autoClose;
    calcScale();
    if (!loadFont()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FlowyEditor", "No monospace font found.\nPut a monospace font named font.ttf next to the executable (Android: assets/, iOS: app bundle) or choose one in Settings.", win);
        return false;
    }
    buildPlaces(); loadBackground();
    for (int i = 1; i < argc; i++) openFile(argv[i]);
    if (docs.empty()) {
        Doc d; d.lang = L_C;
        d.lines = {U"#include <stdio.h>", U"", U"/* FlowyEditor: F5 - run, Ctrl+Space - autocomplete, F1 - menu */", U"int main(void) {", U"    printf(\"Hello, FlowyEditor!\\n\");", U"    return 0;", U"}"};
        docs.push_back(d);
    }
    kbA.snap(0); outA.snap(0); barA.snap(0); caretT = SDL_GetTicks(); startTextInput();
    return true;
}
void App::shutdown() {
    if (settingsDirty) saveSettings();
    proc.stop(); proc.join(); loader.stop();
    for (auto& g : glyphs) if (g.second.t) SDL_DestroyTexture(g.second.t);
    for (auto& t : thumbs) if (t.second.t) SDL_DestroyTexture(t.second.t);
    if (bgTex) SDL_DestroyTexture(bgTex);
    if (font) TTF_CloseFont(font); if (ren) SDL_DestroyRenderer(ren); if (win) SDL_DestroyWindow(win);
#if FLOWY_IMG
    IMG_Quit();
#endif
    TTF_Quit(); SDL_Quit();
}

// =================================================================== plugins ====
void App::loadPlugins() {
    pluginInfo.clear(); tasks.clear();
    std::vector<std::string> dirs = {joinPath(prefDir, "plugins"), joinPath(baseDir, "plugins"), "plugins"}, seen;
    for (auto& dir : dirs) {
        if (!isDirPath(dir)) continue;
        std::error_code ec; std::string canon = fsStr(fs::weakly_canonical(fsPath(dir), ec));
        if (std::find(seen.begin(), seen.end(), canon) != seen.end()) continue; seen.push_back(canon);
        std::vector<std::string> files;
        for (fs::directory_iterator it(fsPath(dir), ec), end; !ec && it != end; it.increment(ec)) { std::string p = fsStr(it->path()); if (lowerS(p).size() > 5 && lowerS(p).substr(lowerS(p).size() - 5) == ".json") files.push_back(p); }
        std::sort(files.begin(), files.end());
        for (auto& f : files) {
            std::string data, err; JV j; PluginInfo pi{baseOf(f), ""};
            if (!readFile(f, data)) pi.status = "cannot read";
            else if (!jparse(data, j, &err)) pi.status = "JSON error: " + err;
            else {
                int nl = 0, nt = 0, ns = 0, nk = 0;
                auto lang = [&](const JV& x) { loadLangJson(x); nl++; }; auto thm = [&](const JV& x) { loadThemeJson(x); nt++; };
                if (auto* a = j.get("languages")) if (a->t == JV::ARR) for (auto& x : a->a) lang(x);
                if (auto* a = j.get("language")) if (a->t == JV::OBJ) lang(*a);
                if (auto* a = j.get("themes")) if (a->t == JV::ARR) for (auto& x : a->a) thm(x);
                if (auto* a = j.get("theme")) if (a->t == JV::OBJ) thm(*a);
                size_t before = g_snippets.size(); loadSnippets(j.get("snippets"), "*"); ns = (int)(g_snippets.size() - before);
                if (auto* a = j.get("tasks")) if (a->t == JV::ARR) for (auto& x : a->a) if (x.t == JV::OBJ && !x.str("cmd").empty()) { tasks.push_back({x.str("name", x.str("cmd")), x.str("cmd"), lowerS(x.str("lang"))}); nk++; }
                pi.status = "OK  (" + j.str("name", "plugin") + ": " + std::to_string(nl) + " lang, " + std::to_string(nt) + " theme, " + std::to_string(ns) + " snippets, " + std::to_string(nk) + " tasks)";
            }
            pluginInfo.push_back(pi);
        }
    }
}
void App::createSamplePlugin() {
    static const char* sample = R"J({
  "name": "Example plugin",
  "version": "1.0",
  "languages": [{
    "name": "Zig", "extensions": ["zig"], "run": "zig run \"{file}\" -- {args}", "indentAfter": ["{"],
    "comments": {"line": ["//"]}, "strings": ["\"", "'"],
    "keywords": ["const", "var", "fn", "pub", "if", "else", "while", "for", "switch", "return", "try", "catch", "defer", "struct", "enum", "union", "comptime"],
    "types": ["i32", "u8", "u32", "u64", "f32", "f64", "bool", "void", "usize", "type"],
    "constants": ["true", "false", "null", "undefined"],
    "functions": ["@import|@import(\"std\")", "std.debug.print|std.debug.print(fmt, args)"],
    "wordChars": ".",
    "snippets": [{"prefix": "main", "description": "main fn", "body": "pub fn main() void {\n\t$0\n}"}]
  }],
  "themes": [{"name": "Sunset", "base": "Dracula", "colors": {"bg": "#2b1d2e", "accent": "#ff7a59", "keyword": "#ff7a59", "string": "#ffd166"}}],
  "snippets": [{"lang": "*", "prefix": "todo", "description": "TODO comment", "body": "// TODO: $0"}],
  "tasks": [{"name": "Git status", "cmd": "git status"}, {"name": "Format with clang-format", "lang": "c++", "cmd": "clang-format -i \"{file}\""}]
}
)J";
    std::error_code ec; fs::create_directories(fsPath(joinPath(prefDir, "plugins")), ec);
    std::string p = joinPath(joinPath(prefDir, "plugins"), "example.json");
    say(writeFile(p, sample) ? "Sample plugin written: " + p : "Cannot write " + p);
}

// ==================================================================== files ====
std::string App::startDir() {
    Doc& d = doc(); if (!d.path.empty()) return dirOf(d.path);
    if (!st.lastDir.empty() && isDirPath(st.lastDir)) return st.lastDir;
    return mobile || homeDir.empty() ? prefDir : homeDir;
}
void App::newDoc() { Doc d; docs.push_back(d); ci = (int)docs.size() - 1; follow = true; comp.open = false; }
void App::addRecent(const std::string& p) {
    st.recent.erase(std::remove(st.recent.begin(), st.recent.end(), p), st.recent.end()); st.recent.insert(st.recent.begin(), p);
    if (st.recent.size() > 12) st.recent.resize(12); st.lastDir = dirOf(p); settingsChanged();
}
void App::openFile(const std::string& p) {
    for (size_t i = 0; i < docs.size(); i++) if (docs[i].path == p) { ci = (int)i; follow = true; return; }
    Doc d; if (!d.load(p)) { d.path = p; d.lang = langFromPath(p); say("New file: " + baseOf(p)); }
    if (docs.size() == 1 && docs[0].path.empty() && !docs[0].modified && docs[0].lines.size() <= 8 && docs[0].lines[0] == U"#include <stdio.h>") { docs[0] = d; ci = 0; }
    else { docs.push_back(d); ci = (int)docs.size() - 1; }
    addRecent(p); follow = true; comp.open = false;
}
void App::openDialog() { openPicker(PK_OPEN, "Open file", startDir(), "", [this](const std::string& p) { openFile(p); }); }
void App::saveDoc() {
    Doc& d = doc(); if (d.path.empty()) { saveAsDialog(); return; }
    bool ok = d.save(); say(ok ? "Saved " + d.name() : "Save FAILED: " + d.path); if (ok) addRecent(d.path);
}
void App::saveAsDialog(std::function<void()> after) {
    Doc& d = doc(); std::string ext = d.lang > 0 && !LANGS[d.lang].exts.empty() ? LANGS[d.lang].exts[0] : "txt";
    std::string nm = d.path.empty() ? "untitled." + ext : baseOf(d.path);
    openPicker(PK_SAVE, "Save as", startDir(), "", [this, after](const std::string& p) {
        Doc& dd = doc(); dd.path = p; if (dd.lang == L_TEXT) dd.setLang(langFromPath(p));
        bool ok = dd.save(); say(ok ? "Saved " + dd.name() : "Save FAILED: " + p); if (ok) { addRecent(p); if (after) after(); } }, nm);
}
void App::closeDoc() {
    Doc& d = doc();
    if (d.modified && !closeWarn) { closeWarn = true; say("Unsaved changes! Close again to discard."); return; }
    closeWarn = false; docs.erase(docs.begin() + ci); if (docs.empty()) docs.push_back(Doc());
    ci = std::min(ci, (int)docs.size() - 1); comp.open = false; follow = true;
}
void App::autosaveAll() {
    for (auto& d : docs) {
        if (!d.modified) continue;
        if (d.path.empty()) { if (!mobile) continue; d.path = joinPath(prefDir, "untitled.txt"); }
        d.save();
    }
    if (settingsDirty) saveSettings();
}
void App::languageMenu() {
    std::vector<Cmd> c;
    for (int i = 0; i < (int)LANGS.size(); i++) c.push_back({LANGS[i].name, i == doc().lang ? "current" : "", [this, i] { doc().setLang(i); say("Language: " + LANGS[i].name); }});
    openMenu("Language", c);
}

// =================================================================== prompts ====
void App::openPrompt(int mode, const U32& init) {
    pr.mode = mode; pr.active = 0; pr.f[0] = init; pr.f[1].clear(); comp.open = false; Doc& d = doc();
    if ((mode == P_FIND || mode == P_REPLACE) && d.hasSel()) { U32 t = d.selText(); if (t.find(U'\n') == NPOS) pr.f[0] = t; }
    barA.to(mode == P_REPLACE ? 2.f : 1.f); if (mobile && st.kbMode == 1) { kbWanted = true; kbA.to(1); }
    startTextInput(); caretT = SDL_GetTicks();
}
void App::promptInput(const std::string& label, const U32& init, std::function<void(const std::string&)> ok) { pr.label = label; pr.ok = std::move(ok); openPrompt(P_INPUT, init); }
void App::closePrompt() { pr.mode = P_NONE; barA.to(0); }
void App::findStep(bool fwd) { if (pr.f[0].empty()) return; if (!doc().find(pr.f[0], fwd, pr.mc)) say("Not found"); follow = true; }
void App::promptEnter(bool shift, bool ctrl) {
    Doc& d = doc();
    switch (pr.mode) {
        case P_FIND: findStep(!shift); break;
        case P_REPLACE:
            if (pr.active == 0) findStep(!shift);
            else if (ctrl) { int n = d.replaceAll(pr.f[0], pr.f[1], pr.mc); say("Replaced " + std::to_string(n)); follow = true; }
            else { if (!d.replaceOne(pr.f[0], pr.f[1], pr.mc)) say("Not found"); follow = true; }
            break;
        case P_GOTO: {
            std::string s = toU8(pr.f[0]); int ln = atoi(s.c_str()); size_t cp = s.find(':'); int col = cp == NPOS ? 1 : atoi(s.c_str() + cp + 1);
            ln = std::max(1, std::min(ln, (int)d.lines.size())); d.cur = d.anc = {ln - 1, std::max(0, std::min(col - 1, (int)d.lines[ln - 1].size()))};
            d.sy = std::max(0.f, (float)((ln - 5) * lh)); follow = true; closePrompt(); break; }
        case P_INPUT: { auto cb = pr.ok; std::string s = toU8(pr.f[0]); closePrompt(); if (cb) cb(s); break; }
        case P_STDIN: { std::string s = toU8(pr.f[0]); proc.sendLine(s); pushOut(s, 3); pr.f[0].clear(); break; }
        default: break;
    }
}
void App::promptText(const U32& t) {
    for (char32_t c : t) if (c >= 32) pr.f[pr.active] += c;
    if ((pr.mode == P_FIND || pr.mode == P_REPLACE) && pr.active == 0) { Doc& d = doc(); Pos a, b; d.selRange(a, b); d.cur = d.anc = a; findStep(true); }
}
bool App::promptKey(SDL_Keysym ks, bool ctrl, bool shift) {
    SDL_Keycode k = ks.sym; if (ctrl && k != SDLK_v && k != SDLK_BACKSPACE) return false;
    switch (k) {
        case SDLK_ESCAPE: case SDLK_AC_BACK: closePrompt(); return true;
        case SDLK_RETURN: case SDLK_KP_ENTER: promptEnter(shift, ctrl); return true;
        case SDLK_TAB: if (pr.mode == P_REPLACE) pr.active ^= 1; return true;
        case SDLK_BACKSPACE: if (ctrl) pr.f[pr.active].clear(); else if (!pr.f[pr.active].empty()) pr.f[pr.active].pop_back(); return true;
        case SDLK_F3: findStep(!shift); return true;
        case SDLK_v: if (char* t = SDL_GetClipboardText()) { U32 u = toU32(t); SDL_free(t); size_t nl = u.find(U'\n'); if (nl != NPOS) u = u.substr(0, nl); promptText(u); } return true;
        default: return true;
    }
}

// ============================================================ completion etc ====
void App::refreshComp(bool force) {
    comp.open = false; Doc& d = doc(); const LangDef& LD = LANGS[d.lang]; const U32& L = d.lines[d.cur.l]; int c = d.cur.c;
    bool hash = LD.special == LS_C || LD.special == LS_CPP || LD.preproc == "#";
    auto isW = [&](char32_t ch) { return isIdC(ch) || (ch > 0 && ch < 128 && (LD.wordChars.find((char)ch) != NPOS || LD.idExtra.find((char)ch) != NPOS)); };
    int st_ = c; while (st_ > 0 && isW(L[st_ - 1])) st_--;
    if (st_ > 0 && hash && L[st_ - 1] == '#') st_--;
    while (st_ < c && !isIdC(L[st_]) && L[st_] != '#') st_++;
    std::string pre = toU8(L.substr(st_, c - st_));
    if (!force && pre.size() < 2) return;
    if (!pre.empty() && isdigit((unsigned char)pre[0])) return;
    if (!force && c > 0) { std::vector<uint8_t> tk; d.hl(d.cur.l, tk); if (c - 1 < (int)tk.size() && (tk[c - 1] == T_STR || tk[c - 1] == T_CMT)) return; }
    std::string lp = lowerS(pre); std::vector<Item> res; std::unordered_set<std::string> seen;
    auto consider = [&](const Item& it) {
        if (it.name == pre || it.name.size() < lp.size()) return;
        if (lowerS(it.name).compare(0, lp.size(), lp) != 0) return;
        if (!seen.insert(it.name + (it.kind == K_SNIP ? "\1" : "")).second) return; res.push_back(it);
    };
    for (auto& it : LD.items) consider(it);
    std::string ln = lowerS(LD.name);
    for (auto& sn : g_snippets) if (sn.lang == ln || sn.lang == "*") consider({sn.prefix, sn.desc, K_SNIP, sn.body});
    size_t lim = std::min<size_t>(d.lines.size(), 20000);
    for (size_t li = 0; li < lim; li++) {
        const U32& s = d.lines[li]; size_t i = 0, n = s.size();
        while (i < n) {
            if (isIdS(s[i])) { size_t j = i + 1; while (j < n && isIdC(s[j])) j++; if (j - i >= 3 && !(li == (size_t)d.cur.l && (int)j == c)) consider({toU8(s.substr(i, j - i)), "", K_WORD, ""}); i = j; }
            else i++;
        }
    }
    if (res.empty()) return;
    std::sort(res.begin(), res.end(), [&](const Item& a, const Item& b) {
        bool ea = a.name.compare(0, pre.size(), pre) == 0, eb = b.name.compare(0, pre.size(), pre) == 0;
        if (ea != eb) return ea; int ra = a.kind == K_WORD, rb = b.kind == K_WORD; if (ra != rb) return ra < rb; return a.name < b.name; });
    if (res.size() > 60) res.resize(60);
    comp.list = res; comp.sel = 0; comp.top = 0; comp.start = {d.cur.l, st_}; comp.open = true;
}
void App::insertSnippet(const std::string& body, int from) {
    Doc& d = doc(); d.pushUndo(2); U32& L = d.lines[d.cur.l]; U32 ind; for (char32_t ch : L) { if (ch == ' ' || ch == '\t') ind += ch; else break; }
    U32 b = toU32(body), out; std::map<int, U32> defs; long first = -1, firstLen = 0, zero = -1; int firstN = INT_MAX;
    for (size_t i = 0; i < b.size();) {
        char32_t c = b[i];
        if (c == '\n') { out += U'\n'; out += ind; i++; }
        else if (c == '\t') { out += U32(TABW, U' '); i++; }
        else if (c == '$' && i + 1 < b.size()) {
            if (b[i + 1] == '$') { out += U'$'; i += 2; }
            else if (isDig(b[i + 1])) {
                int num = 0; size_t j = i + 1; while (j < b.size() && isDig(b[j])) { num = num * 10 + (int)(b[j] - '0'); j++; }
                if (num == 0) zero = (long)out.size(); else if (defs.count(num)) out += defs[num]; else if (num < firstN) { firstN = num; first = (long)out.size(); firstLen = 0; }
                i = j;
            } else if (b[i + 1] == '{') {
                size_t j = i + 2; int num = 0; while (j < b.size() && isDig(b[j])) { num = num * 10 + (int)(b[j] - '0'); j++; }
                U32 def; if (j < b.size() && b[j] == ':') { j++; while (j < b.size() && b[j] != '}') def += b[j++]; }
                if (j < b.size()) j++;
                size_t pos = out.size(); out += def; defs[num] = def;
                if (num == 0) zero = (long)pos; else if (num < firstN) { firstN = num; first = (long)pos; firstLen = (long)def.size(); }
                i = j;
            } else { out += c; i++; }
        } else { out += c; i++; }
    }
    int startL = d.cur.l, col = from; L.erase((size_t)col, (size_t)(d.cur.c - col)); d.cur.c = col; d.anc = d.cur; d.insertRaw(out);
    auto toPos = [&](long off) { Pos p{startL, col}; for (long k = 0; k < off && k < (long)out.size(); k++) { if (out[(size_t)k] == '\n') { p.l++; p.c = 0; } else p.c++; } return p; };
    if (first >= 0) { d.anc = toPos(first); d.cur = toPos(first + firstLen); } else if (zero >= 0) { d.cur = d.anc = toPos(zero); }
    follow = true;
}
void App::acceptComp() {
    if (!comp.open || comp.list.empty()) return;
    Doc& d = doc(); Item it = comp.list[comp.sel]; comp.open = false;
    if (it.kind == K_SNIP) { insertSnippet(it.body, comp.start.c); return; }
    d.pushUndo(2); U32& L = d.lines[d.cur.l]; int s = comp.start.c; U32 nm = toU32(it.name);
    L.replace((size_t)s, (size_t)(d.cur.c - s), nm); d.cur.c = s + (int)nm.size(); d.anc = d.cur;
    const LangDef& LD = LANGS[d.lang];
    if (it.kind == K_FUNC && LD.callParens) {
        char32_t nx = d.cur.c < (int)L.size() ? L[d.cur.c] : 0;
        if (nx != '(') { L.insert((size_t)d.cur.c, U"()"); bool noargs = it.sig.find("()") != NPOS || it.sig.find("(void)") != NPOS; d.cur.c += noargs ? 2 : 1; d.anc = d.cur; }
    }
    d.touch(d.cur.l); follow = true;
}
void App::validateComp() { if (comp.open) { Doc& d = doc(); if (d.cur.l != comp.start.l || d.cur.c < comp.start.c) comp.open = false; } }
void App::afterEdit(bool tryComp) { follow = true; caretT = SDL_GetTicks(); if (tryComp || comp.open) refreshComp(false); }
void App::copySel(bool cut) {
    Doc& d = doc(); U32 t;
    if (d.hasSel()) t = d.selText(); else { t = d.lines[d.cur.l] + U"\n"; if (cut) d.selectLine(d.cur.l); }
    SDL_SetClipboardText(toU8(t).c_str());
    if (cut && d.hasSel()) { d.pushUndo(2); d.deleteSel(); }
}
void App::paste() {
    char* t = SDL_GetClipboardText(); if (!t) return; std::string s = t; SDL_free(t); if (s.empty()) return;
    U32 u; for (char32_t c : toU32(s)) if (c != '\r') u += c;
    doc().insertText(u); follow = true; comp.open = false;
}

// ====================================================================== run ====
std::string App::runTemplate(Lang l) {
    if (l < 0 || l >= (int)LANGS.size()) return "";
    auto it = st.runCmd.find(LANGS[l].name); if (it != st.runCmd.end() && !it->second.empty()) return it->second;
    return LANGS[l].run;
}
std::string App::expand(const std::string& t, const Doc& d) {
    std::string file = d.path, dir = dirOf(file), nm = d.name(), stem = nm, ext; size_t dot = nm.rfind('.');
    if (dot != NPOS && dot > 0) { stem = nm.substr(0, dot); ext = nm.substr(dot + 1); }
    if (dir.empty()) dir = ".";
    std::string out = joinPath(dir, stem);
#if defined(_WIN32)
    out += ".exe";
#endif
    std::string r = t; r = replaceAll(r, "{file}", file); r = replaceAll(r, "{dir}", dir); r = replaceAll(r, "{name}", nm); r = replaceAll(r, "{stem}", stem);
    r = replaceAll(r, "{ext}", ext); r = replaceAll(r, "{out}", out); r = replaceAll(r, "{args}", d.args);
    return r;
}
void App::pushOut(const std::string& line, int kind) {
    if (kind < 0) { std::string l = lowerS(line); kind = (l.find("error") != NPOS || l.find("fatal") != NPOS || l.find("undefined reference") != NPOS) ? 1 : (l.find("warning") != NPOS ? 2 : 0); }
    outLines.push_back({line, kind}); if (outLines.size() > 5000) outLines.erase(outLines.begin(), outLines.begin() + 500);
}
void App::startCmd(const std::string& cmd, const std::string& cwd) {
    if (proc.running) { say("Already running - Shift+F5 to stop"); return; }
    outLines.clear(); outPartial.clear(); outStick = true; outScroll = outTarget = 0; pushOut("$ " + cmd, 3);
    std::string err; runCwd = cwd;
    if (!proc.start(cmd, cwd, err)) { pushOut(err, 5); say(err); outReported = true; } else outReported = false;
    outA.to(1);
}
void App::stopRun() { if (proc.running) { proc.stop(); say("Stopping process..."); } }
void App::runDoc(bool askArgs) {
    Doc& d = doc();
    if (proc.running) { say("Already running - Shift+F5 to stop"); return; }
    if (d.path.empty()) { saveAsDialog([this] { runDoc(false); }); return; }
    if (d.modified && st.autoSave) { if (!d.save()) { say("Save failed"); return; } }
    if (askArgs) { promptInput("Arguments", toU32(d.args), [this](const std::string& s) { doc().args = s; runDoc(false); }); return; }
    std::string tpl = runTemplate(d.lang);
    if (tpl.empty()) { say("No run command for " + LANGS[d.lang].name + ". Set one in Settings > Run."); return; }
    startCmd(expand(tpl, d), d.cwd.empty() ? dirOf(d.path) : d.cwd);
}
static std::string stripAnsi(const std::string& s) {
    std::string o; for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == 27 && i + 1 < s.size() && s[i + 1] == '[') { i += 2; while (i < s.size() && !isalpha((unsigned char)s[i])) i++; continue; }
        if (s[i] == '\r') continue; o += s[i];
    }
    return o;
}
void App::drainProc() {
    proc.poll(); bool fin = proc.finished; std::string chunk = proc.drain();
    if (!chunk.empty()) {
        outPartial += stripAnsi(chunk); size_t p;
        while ((p = outPartial.find('\n')) != NPOS) { std::string line = outPartial.substr(0, p); outPartial.erase(0, p + 1); pushOut(line); }
    }
    if (fin && !outReported && chunk.empty()) {
        if (!outPartial.empty()) { pushOut(outPartial); outPartial.clear(); }
        int code = proc.exitCode; pushOut("[process exited with code " + std::to_string(code) + "]", code == 0 ? 4 : 5); outReported = true; proc.join();
    }
}
void App::jumpToLoc(const std::string& line) {
    std::string f; int ln = 1, cl = 1; if (!parseLoc(line, f, ln, cl)) return;
    std::string p = f; if (!pathExists(p) && !runCwd.empty()) p = joinPath(runCwd, f);
    if (!pathExists(p)) { say("File not found: " + f); return; }
    openFile(p); Doc& d = doc(); ln = std::max(1, std::min(ln, (int)d.lines.size()));
    d.cur = d.anc = {ln - 1, std::max(0, std::min(cl - 1, (int)d.lines[ln - 1].size()))}; d.sy = std::max(0.f, (float)((ln - 5) * lh)); follow = true;
}
