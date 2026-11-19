#pragma once
namespace fs = std::filesystem;

enum Ov { OV_NONE, OV_MENU, OV_SETTINGS, OV_PICKER };
enum PMode { P_NONE, P_FIND, P_REPLACE, P_GOTO, P_INPUT, P_STDIN };
enum { R_HEAD, R_TOGGLE, R_SLIDER, R_CHOICE, R_ACTION, R_COLOR, R_INFO };
enum { PK_OPEN, PK_SAVE, PK_IMAGE, PK_FOLDER, PK_FILE };
// key codes: >0 = character, <0 = special
enum { KC_SHIFT = -1, KC_BKSP = -2, KC_ENTER = -3, KC_SPACE = -4, KC_L123 = -5, KC_LABC = -6, KC_LSYM = -7, KC_TAB = -8, KC_LEFT = -9, KC_RIGHT = -10,
       KC_UP = -11, KC_DOWN = -12, KC_HIDE = -13, KC_CMP = -14, KC_GAP = -15, KC_SUG = -100,
       IC_MENU = -30, IC_RUN = -31, IC_STOP = -32, IC_CLOSE = -33, IC_PLUS = -34, IC_BACK = -35, IC_FOLDER = -36, IC_FILE = -37, IC_IMG = -38, IC_GEAR = -39 };

struct Anim {
    float v = 0, t = 0;
    void to(float x) { t = x; } void snap(float x) { v = t = x; }
    bool step(float k) { v += (t - v) * k; if (std::fabs(t - v) < 0.003f) v = t; return v != t; }
};
struct Glyph { SDL_Texture* t = nullptr; int w = 0, h = 0; };
struct Btn { SDL_Rect r; std::string label; std::function<void()> fn; int style = 0; };   // style: 0 draw as button, -1 invisible hit area, -2 invisible no ripple
struct Prompt { int mode = P_NONE; U32 f[2]; int active = 0; bool mc = false; std::string label; std::function<void(const std::string&)> ok; };
struct Comp { bool open = false; std::vector<Item> list; int sel = 0, top = 0; Pos start; SDL_Rect rect{0, 0, 0, 0}; };
struct Cmd { std::string name, hint; std::function<void()> fn; };
struct Row { int type = R_INFO; std::string label, val; std::function<void()> tap, chg; std::function<float()> get; std::function<void(float)> set; float mn = 0, mx = 1; SDL_Color* col = nullptr; bool on = false; };
struct Entry { std::string name; bool dir = false, img = false; uintmax_t size = 0; };
struct Picker { int mode = PK_OPEN; std::string title, dir, sel, exts; U32 name; std::vector<Entry> ents; std::function<void(const std::string&)> done; bool hidden = false; };
struct Thumb { SDL_Texture* t = nullptr; int w = 0, h = 0; bool pending = false, failed = false; };
struct VKey { std::string lab, alt; int code; float w; };
struct KeyRect { SDL_Rect r; int code; int ch; std::string lab; };
struct Slider { SDL_Rect r; float mn, mx; std::function<void(float)> set; };
struct Ripple { int x, y; Uint32 t0; SDL_Rect clip; };
struct TBItem { std::string l; std::function<void()> f; int w = 0; };
struct Task { std::string name, cmd, lang; };
struct PluginInfo { std::string file, status; };
struct OutLine { std::string text; int kind = 0; };   // kind: 0 normal 1 error 2 warning 3 info 4 good 5 bad

static bool parseLoc(const std::string& s, std::string& file, int& line, int& col) {   // "file:12[:5]: ..." (gcc/clang/python style)
    size_t st = 0; if (s.size() > 2 && isalpha((unsigned char)s[0]) && s[1] == ':' && (s[2] == '\\' || s[2] == '/')) st = 2;
    size_t c1 = s.find(':', st); if (c1 == NPOS || c1 == 0) return false; file = s.substr(0, c1);
    size_t p = c1 + 1; if (p >= s.size() || !isdigit((unsigned char)s[p])) return false;
    line = atoi(s.c_str() + p); while (p < s.size() && isdigit((unsigned char)s[p])) p++; col = 1;
    if (p < s.size() && s[p] == ':') { p++; if (p < s.size() && isdigit((unsigned char)s[p])) col = atoi(s.c_str() + p); }
    return true;
}
static bool inRect(const SDL_Rect& r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }
static std::string joinPath(const std::string& a, const std::string& b) { if (a.empty()) return b; char l = a.back(); return (l == '/' || l == '\\') ? a + b : a + "/" + b; }
static std::string dirOf(const std::string& p) { size_t k = p.find_last_of("/\\"); if (k == NPOS) return ""; if (k == 0) return "/"; return p.substr(0, k); }
static std::string baseOf(const std::string& p) { size_t k = p.find_last_of("/\\"); return k == NPOS ? p : p.substr(k + 1); }
static bool pathExists(const std::string& p) { std::error_code ec; return fs::exists(fsPath(p), ec); }
static bool isDirPath(const std::string& p) { std::error_code ec; return fs::is_directory(fsPath(p), ec); }
static std::string replaceAll(std::string s, const std::string& a, const std::string& b) {
    size_t p = 0; while ((p = s.find(a, p)) != NPOS) { s.replace(p, a.size(), b); p += b.size(); } return s;
}

struct App {
    // ---- platform / rendering
    SDL_Window* win = nullptr; SDL_Renderer* ren = nullptr; TTF_Font* font = nullptr;
    Settings st; Theme T; bool mobile = FLOWY_MOBILE;
    float uiScale = 1, pxX = 1, pxY = 1; int fontPx = 16, cw = 9, lh = 20, fh = 18, pad = 5, rad = 8, W = 0, H = 0;
    std::string prefDir, baseDir, homeDir;
    std::unordered_map<Uint32, Glyph> glyphs;
    SDL_Texture* bgTex = nullptr; int bgW = 0, bgH = 0; std::string bgLoaded; int bgLoadedBlur = -1; Uint32 bgReloadAt = 0;
    // ---- documents
    std::vector<Doc> docs; int ci = 0, lastCi = -1;
    // ---- layout
    int tabH = 0, statH = 0, rowH = 0, barH = 0, tbH = 0, kbH = 0, kbFull = 0, kbKeyH = 0, kbGap = 0, outH = 0, outFull = 0, gutterW = 0;
    SDL_Rect edRect{0, 0, 100, 100}, outRect{0, 0, 0, 0}, kbRect{0, 0, 0, 0}, statRect{0, 0, 0, 0};
    std::vector<TBItem> tbItems; int tbBh = 0;
    // ---- state
    bool ovClosing = false, running = true, follow = false, quitWarn = false, closeWarn = false, dragging = false, kbWanted = false;
    std::string msg; Uint32 msgTick = 0, caretT = 0, lastScroll = 0; bool animating = false;
    Prompt pr; Comp comp; std::vector<Btn> btns; std::vector<Slider> sliders; std::vector<Ripple> ripples;
    Anim kbA, outA, barA, ovA, popA, toastA, sbA, carX, carY, clY, tabX, tabW2;
    // ---- overlays
    int ov = OV_NONE, setPage = 0, colorEdit = -1, palSel = 0; std::vector<Cmd> pal; U32 palQuery; std::string palTitle;
    Picker pk; std::unordered_map<std::string, Thumb> thumbs; ThumbLoader loader; std::vector<std::pair<std::string, std::string>> places;
    SDL_Rect ovPanel{0, 0, 0, 0}, ovList{0, 0, 0, 0}; float ovScroll = 0, ovTarget = 0, ovContentH = 0; std::string pkSel;
    Uint32 settingsDirtyAt = 0; bool settingsDirty = false;
    // ---- keyboard
    int kbLayer = 0, kbShift = 0; Uint32 kbLastShift = 0; std::vector<KeyRect> keys; std::map<int64_t, int> kbHeld; std::unordered_map<int, float> keyAnim;
    int rpCode = 0, rpCh = 0; int64_t rpPid = 0; Uint32 rpT0 = 0, rpLast = 0; KeyRect rpKey{};
    // ---- output / run
    Proc proc; std::vector<OutLine> outLines; std::string outPartial, runCwd; float outScroll = 0, outTarget = 0; bool outStick = true, outReported = true;
    SDL_Rect outBody{0, 0, 0, 0}; std::vector<Task> tasks; std::vector<PluginInfo> pluginInfo;
    // ---- touch / mouse
    struct Fin { float x, y; };
    std::map<SDL_FingerID, Fin> fingers; SDL_FingerID sliderFinger = -1; int activeSlider = -1;
    bool tDown = false, tMoved = false, tLong = false, tPinch = false; int scrollTgt = -1;
    float tx0 = 0, ty0 = 0, pinchD = 0, velX = 0, velY = 0, lastTapX = 0, lastTapY = 0; Uint32 tDownTick = 0, lastTapTick = 0, lastMoveTick = 0;
    std::string lastTitle; Pos mbCacheFor{-1, -1}, mbA, mbB; bool mbOk = false; unsigned mbEdits = ~0u; int mbDoc = -1;

    Doc& doc() { return docs[ci]; }
    void say(const std::string& m) { msg = m; msgTick = SDL_GetTicks(); }
    bool kbVisible() const { return kbA.t > 0.5f; }

    // ---- primitives (defined in app_draw.hpp)
    void setc(SDL_Color c) { SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a); }
    void fill(int x, int y, int w, int h, SDL_Color c) { if (c.a == 0 || w <= 0 || h <= 0) return; setc(c); SDL_Rect r{x, y, w, h}; SDL_RenderFillRect(ren, &r); }
    void fill(const SDL_Rect& r, SDL_Color c) { fill(r.x, r.y, r.w, r.h, c); }
    void outline(const SDL_Rect& r, SDL_Color c) { setc(c); SDL_RenderDrawRect(ren, &r); }
    void clip(const SDL_Rect& r) { SDL_RenderSetClipRect(ren, &r); }
    void unclip() { SDL_RenderSetClipRect(ren, nullptr); }
    void rr(SDL_Rect r, SDL_Color c, int radius = -1);
    void circle(int cx, int cy, int r, SDL_Color c);
    void shadow(const SDL_Rect& r, int size, int alpha);
    void tri(int cx, int cy, int s, int dir, SDL_Color c);
    void icon(int code, const SDL_Rect& r, SDL_Color c);
    void glyph(char32_t ch, int x, int y, SDL_Color c);
    int text(const std::string& s, int x, int y, SDL_Color c);
    int textU(const U32& u, int x, int y, SDL_Color c);
    int textW(const std::string& s) { return (int)toU32(s).size() * cw; }
    SDL_Color pane(SDL_Color c) { return bgTex ? withA(c, st.paneAlpha) : c; }
    void addBtn(SDL_Rect r, const std::string& l, std::function<void()> f, int style = 0) { btns.push_back({r, l, std::move(f), style}); }

    // ---- init / settings / plugins (app_ops.hpp)
    bool init(int argc, char** argv);
    void shutdown();
    bool calcScale(); bool loadFont(); void applyTheme(); void applySettings(); void settingsChanged(); void loadSettings(); void saveSettings();
    void initRegistries(); void loadPlugins(); void createSamplePlugin(); void buildPlaces(); void loadBackground(); void startTextInput();
    // files / docs
    void newDoc(); void openFile(const std::string& p); void openDialog(); void saveDoc(); void saveAsDialog(std::function<void()> after = nullptr);
    void closeDoc(); void autosaveAll(); void addRecent(const std::string& p); void languageMenu(); std::string startDir();
    // prompt
    void openPrompt(int mode, const U32& init = U32()); void promptInput(const std::string& label, const U32& init, std::function<void(const std::string&)> ok);
    void closePrompt(); void findStep(bool fwd); void promptEnter(bool shift, bool ctrl); void promptText(const U32& t); bool promptKey(SDL_Keysym ks, bool ctrl, bool shift);
    // completion / snippets / clipboard
    void refreshComp(bool force); void acceptComp(); void validateComp(); void afterEdit(bool tryComp); void insertSnippet(const std::string& body, int from);
    void copySel(bool cut); void paste();
    // run
    std::string expand(const std::string& t, const Doc& d); std::string runTemplate(Lang l); void runDoc(bool askArgs); void startCmd(const std::string& cmd, const std::string& cwd);
    void stopRun(); void drainProc(); void pushOut(const std::string& line, int kind = -1); void jumpToLoc(const std::string& line);
    // overlays (app_overlay.hpp)
    void openMenu(const std::string& title, std::vector<Cmd> cmds); void mainMenu(); void openSettings(int page); std::vector<Row> settingsRows();
    void openPicker(int mode, const std::string& title, const std::string& dir, const std::string& exts, std::function<void(const std::string&)> done, const std::string& name = "");
    void pickerGo(const std::string& dir); void closeOverlay(); void overlayKey(SDL_Keycode k, bool ctrl, bool shift);
    void drawOverlay(); void drawMenu(); void drawSettings(); void drawPicker(); void drawPanelFrame(const std::string& title, bool back);
    // drawing (app_draw.hpp)
    void layout(); void draw(); void drawBackground(); void drawTabs(); void drawPromptBar(); void drawEditor(); void drawPopup(Doc& d); void drawOutput();
    void drawToolbar(); void button(SDL_Rect r, const std::string& l, std::function<void()> f, bool accent = false); void drawStatus(); void drawKeyboard(); void drawToast(); void drawRipples(); std::vector<TBItem> toolbarItems();
    float caretAlpha(); bool matchBracketCached(Pos& a, Pos& b);
    // input (app_input.hpp)
    void event(const SDL_Event& e); void onKey(SDL_Keysym ks); void onText(const char* t); void inputText(const U32& u); bool tap(int x, int y, int clicks, bool shift);
    void onFinger(const SDL_Event& e); void tick(float dt); void run(); Pos posAt(int x, int y); void ensureVisible(); void clampScroll();
    void scrollBy(int target, float dx, float dy); int scrollTargetAt(int x, int y);
    int beginSlider(int x, int y); void moveSlider(int idx, int x); void kbDown(int x, int y, int64_t pid); void kbUp(int64_t pid); void kbPress(const KeyRect& k); void sendKey(SDL_Keycode k);
    void kbRepeat(); bool hitKeyboard(int x, int y) { return kbA.v > 0.05f && inRect(kbRect, x, y); }
};

static std::vector<std::vector<VKey>> kbLayout(int layer) {
    std::vector<std::vector<VKey>> L;
    auto chars = [](const char* s, float w = 1.f) { std::vector<VKey> r; for (const char* p = s; *p; p++) { std::string a(1, *p), b = a; if (*p >= 'a' && *p <= 'z') b = std::string(1, (char)(*p - 32)); r.push_back({a, b, (int)(unsigned char)*p, w}); } return r; };
    auto sp = [](int code, const char* lab, float w) { return VKey{lab, lab, code, w}; };
    auto cat = [](std::vector<VKey> a, const std::vector<VKey>& b) { a.insert(a.end(), b.begin(), b.end()); return a; };
    std::vector<VKey> bottom = {sp(layer == 0 ? KC_L123 : KC_LABC, layer == 0 ? "?123" : "abc", 1.5f), VKey{",", ",", ',', 1.f}, sp(KC_SPACE, "", 5.f), VKey{".", ".", '.', 1.f}, sp(KC_ENTER, "", 1.5f)};
    if (layer == 0) {
        L.push_back(chars("qwertyuiop"));
        L.push_back(cat(cat({sp(KC_GAP, "", .5f)}, chars("asdfghjkl")), {sp(KC_GAP, "", .5f)}));
        L.push_back(cat(cat({sp(KC_SHIFT, "", 1.5f)}, chars("zxcvbnm")), {sp(KC_BKSP, "", 1.5f)}));
    } else if (layer == 1) {
        L.push_back(chars("1234567890")); L.push_back(chars("-/:;()$&@\""));
        L.push_back(cat(cat({sp(KC_LSYM, "#+=", 1.5f)}, chars(".,?!'", 1.4f)), {sp(KC_BKSP, "", 1.5f)}));
    } else {
        L.push_back(chars("[]{}#%^*+=")); L.push_back(chars("_\\|~<>`'\"!"));
        L.push_back(cat(cat({sp(KC_L123, "123", 1.5f)}, chars(".,?:;", 1.4f)), {sp(KC_BKSP, "", 1.5f)}));
    }
    L.push_back(bottom);
    return L;
}
