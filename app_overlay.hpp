#pragma once
// ================================================================== helpers ====
void App::closeOverlay() { ovClosing = true; ovA.to(0); if (settingsDirty) saveSettings(); }
static void startOverlay(App& a, int which) { a.ov = which; a.ovClosing = false; a.ovA.to(1); a.ovScroll = a.ovTarget = 0; a.ovContentH = 0; a.comp.open = false; }

void App::drawPanelFrame(const std::string& title, bool back) {
    int hh = rowH + pad; SDL_Rect p = ovPanel;
    rr(p, T.c[C_POPUP], rad + 2); fill(p.x, p.y + hh, p.w, 1, withA(T.c[C_FG], 0.1f));
    SDL_Rect b{p.x + pad / 2, p.y, hh, hh}; icon(back ? IC_BACK : IC_CLOSE, b, T.c[C_FG]);
    addBtn(b, "", [this, back] { if (back) { setPage = 0; ovTarget = ovScroll = 0; colorEdit = -1; } else closeOverlay(); });
    text(title, p.x + hh + pad, p.y + (hh - lh) / 2, T.c[C_FG]);
    ovList = {p.x, p.y + hh + 1, p.w, p.h - hh - 1};
}
void App::drawOverlay() {
    btns.clear(); sliders.clear();
    fill(0, 0, W, H, C(0, 0, 0, (int)(150 * ovA.v)));
    bool wide = W > 900; int availH = std::max(lh * 8, H - kbH);
    int mw = wide ? std::min((int)(W * 0.72f), (int)(900 * std::max(1.f, uiScale * 0.8f))) : W, mh = wide ? (int)(availH * 0.86f) : availH;
    ovPanel = {(W - mw) / 2, (availH - mh) / 2 + (int)((1 - ovA.v) * 24), mw, mh};
    addBtn(SDL_Rect{0, 0, W, H}, "", [this] { if (ov == OV_MENU) closeOverlay(); }, -2);
    addBtn(ovPanel, "", nullptr, -2);
    if (wide) shadow(ovPanel, 14, (int)(140 * ovA.v));
    if (ov == OV_MENU) drawMenu(); else if (ov == OV_SETTINGS) drawSettings(); else if (ov == OV_PICKER) drawPicker();
}

// ====================================================================== menu ====
void App::openMenu(const std::string& title, std::vector<Cmd> cmds) { pal = std::move(cmds); palTitle = title; palQuery.clear(); palSel = 0; startOverlay(*this, OV_MENU); if (mobile && st.kbMode == 1) { kbWanted = true; kbA.to(1); } }
void App::drawMenu() {
    drawPanelFrame(palTitle, false);
    int rh = (int)((lh + pad * 2) * (mobile ? 1.25f : 1.f)), hh = rowH;
    SDL_Rect q{ovList.x + pad, ovList.y + pad / 2, ovList.w - pad * 2, hh - pad}; rr(q, T.c[C_BG], rad);
    std::string qs = toU8(palQuery); clip(q); text(qs.empty() ? "type to filter..." : qs, q.x + pad, q.y + (q.h - lh) / 2, qs.empty() ? T.c[C_GUTTERFG] : T.c[C_FG]);
    if (!qs.empty() || true) fill(q.x + pad + (int)palQuery.size() * cw, q.y + 3, 2, q.h - 6, withA(T.c[C_CURSOR], caretAlpha())); unclip();
    ovList.y += hh + pad / 2; ovList.h -= hh + pad / 2;
    std::vector<int> idx; std::string lq = lowerS(qs);
    for (int i = 0; i < (int)pal.size(); i++) if (lq.empty() || lowerS(pal[i].name).find(lq) != NPOS) idx.push_back(i);
    if (palSel >= (int)idx.size()) palSel = std::max(0, (int)idx.size() - 1);
    ovContentH = (float)(idx.size() * rh); ovTarget = std::min(std::max(0.f, ovTarget), std::max(0.f, ovContentH - ovList.h));
    clip(ovList);
    for (int k = 0; k < (int)idx.size(); k++) {
        SDL_Rect r{ovList.x, ovList.y + k * rh - (int)ovScroll, ovList.w, rh}; if (r.y + r.h < ovList.y || r.y > ovList.y + ovList.h) continue;
        if (k == palSel) fill(r, withA(T.c[C_SEL], 0.7f));
        Cmd& c = pal[idx[k]]; text(c.name, r.x + pad * 2, r.y + (rh - lh) / 2, T.c[C_FG]);
        if (!c.hint.empty()) text(c.hint, r.x + r.w - textW(c.hint) - pad * 2, r.y + (rh - lh) / 2, T.c[C_GUTTERFG]);
        SDL_Rect hit = r; hit.y = std::max(hit.y, ovList.y); hit.h = std::min(r.y + r.h, ovList.y + ovList.h) - hit.y;
        int id = idx[k]; addBtn(hit, "", [this, id] { auto f = pal[id].fn; closeOverlay(); if (f) f(); });
    }
    unclip();
}
void App::mainMenu() {
    std::vector<Cmd> c; Doc& d = doc();
    auto add = [&](const std::string& n, const std::string& h, std::function<void()> f) { c.push_back({n, h, std::move(f)}); };
    add("New file", "Ctrl+N", [this] { newDoc(); }); add("Open file...", "Ctrl+O", [this] { openDialog(); });
    add("Save", "Ctrl+S", [this] { saveDoc(); }); add("Save as...", "Ctrl+Shift+S", [this] { saveAsDialog(); });
    add("Save all", "", [this] { autosaveAll(); say("Saved all"); }); add("Close tab", "Ctrl+W", [this] { closeDoc(); });
    for (size_t i = 0; i < st.recent.size() && i < 6; i++) { std::string r = st.recent[i]; add("Recent: " + baseOf(r), dirOf(r), [this, r] { openFile(r); }); }
    add("Run", "F5", [this] { runDoc(false); }); add("Stop", "Shift+F5", [this] { stopRun(); }); add("Run with arguments...", "Ctrl+F5", [this] { runDoc(true); });
    add("Edit run command...", "", [this] { Lang l = doc().lang; promptInput("Run command", toU32(runTemplate(l)), [this, l](const std::string& s) { st.runCmd[LANGS[l].name] = s; settingsChanged(); say("Run command saved for " + LANGS[l].name); }); });
    add("Set working directory...", "", [this] { openPicker(PK_FOLDER, "Working directory", startDir(), "", [this](const std::string& p) { doc().cwd = p; say("Working dir: " + p); }); });
    std::string ln = lowerS(LANGS[d.lang].name);
    for (auto& t : tasks) if (t.lang.empty() || t.lang == ln) { Task tk = t; add("Task: " + t.name, "plugin", [this, tk] { Doc& dd = doc(); startCmd(expand(tk.cmd, dd), dd.cwd.empty() ? (dd.path.empty() ? "." : dirOf(dd.path)) : dd.cwd); }); }
    add("Find", "Ctrl+F", [this] { openPrompt(P_FIND); }); add("Replace", "Ctrl+H", [this] { openPrompt(P_REPLACE); }); add("Go to line", "Ctrl+G", [this] { openPrompt(P_GOTO); });
    add("Undo", "Ctrl+Z", [this] { doc().doUndo(); follow = true; }); add("Redo", "Ctrl+Y", [this] { doc().doRedo(); follow = true; });
    add("Toggle comment", "Ctrl+/", [this] { doc().toggleComment(); follow = true; }); add("Duplicate line", "Ctrl+D", [this] { doc().duplicate(); follow = true; });
    add("Delete line", "Ctrl+Shift+K", [this] { doc().deleteLines(); follow = true; }); add("Select all", "Ctrl+A", [this] { doc().selectAll(); });
    add("Autocomplete", "Ctrl+Space", [this] { refreshComp(true); }); add("Language...", "F6", [this] { languageMenu(); });
    add("Settings", "Ctrl+,", [this] { openSettings(0); }); add("Colors...", "", [this] { openSettings(2); }); add("Background image...", "", [this] { openSettings(3); });
    add("Toggle output panel", "", [this] { outA.to(outA.t > 0.5f ? 0.f : 1.f); });
    add("Toggle on-screen keyboard", "F4", [this] { kbWanted = !kbWanted; kbA.to(kbWanted ? 1.f : 0.f); follow = true; });
    add("Toggle button toolbar", "F2", [this] { st.toolbar = !st.toolbar; settingsChanged(); });
    add("Next theme", "Ctrl+T", [this] { int i = (findTheme(st.theme) + 1) % (int)g_themes.size(); st.theme = g_themes[i].name; st.ov.clear(); applyTheme(); settingsChanged(); say("Theme: " + st.theme); });
    add("Zoom in", "Ctrl+=", [this] { st.fontScale = std::min(3.f, st.fontScale * 1.1f); loadFont(); settingsChanged(); follow = true; });
    add("Zoom out", "Ctrl+-", [this] { st.fontScale = std::max(0.5f, st.fontScale / 1.1f); loadFont(); settingsChanged(); follow = true; });
    add("Reset zoom", "Ctrl+0", [this] { st.fontScale = 1; loadFont(); settingsChanged(); follow = true; });
    add("Fullscreen", "F11", [this] { Uint32 f = SDL_GetWindowFlags(win); SDL_SetWindowFullscreen(win, (f & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP); });
    add("Reload plugins", "", [this] { initRegistries(); loadPlugins(); applyTheme(); for (auto& dd : docs) dd.validTo = 1; say("Plugins reloaded: " + std::to_string(pluginInfo.size()) + " file(s)"); });
    openMenu("Menu", c);
}

// =================================================================== settings ====
void App::openSettings(int page) { setPage = page; colorEdit = -1; startOverlay(*this, OV_SETTINGS); }
std::vector<Row> App::settingsRows() {
    std::vector<Row> r;
    auto head = [&](const std::string& t) { Row x; x.type = R_HEAD; x.label = t; r.push_back(x); };
    auto info = [&](const std::string& t) { Row x; x.type = R_INFO; x.label = t; r.push_back(x); };
    auto act = [&](const std::string& l, const std::string& v, std::function<void()> f) { Row x; x.type = R_ACTION; x.label = l; x.val = v; x.tap = std::move(f); r.push_back(x); };
    auto tog = [&](const std::string& l, bool* b, std::function<void()> after = nullptr) { Row x; x.type = R_TOGGLE; x.label = l; x.on = *b; x.tap = [this, b, after] { *b = !*b; if (after) after(); settingsChanged(); }; r.push_back(x); };
    auto sl = [&](const std::string& l, float* p, float mn, float mx, std::function<void()> after = nullptr) {
        Row x; x.type = R_SLIDER; x.label = l; x.mn = mn; x.mx = mx; x.get = [p] { return *p; }; x.set = [this, p, after](float v) { *p = v; if (after) after(); settingsChanged(); }; r.push_back(x); };
    auto isl = [&](const std::string& l, int* p, int mn, int mx, std::function<void()> after = nullptr) {
        Row x; x.type = R_SLIDER; x.label = l; x.mn = (float)mn; x.mx = (float)mx; x.get = [p] { return (float)*p; }; x.set = [this, p, after](float v) { *p = (int)std::lround(v); if (after) after(); settingsChanged(); }; r.push_back(x); };
    auto ch = [&](const std::string& l, int* p, std::vector<std::string> opts, std::function<void()> after = nullptr) {
        Row x; x.type = R_CHOICE; x.label = l; x.val = opts[(size_t)*p % opts.size()]; x.tap = [this, p, opts, after] { *p = (*p + 1) % (int)opts.size(); if (after) after(); settingsChanged(); }; r.push_back(x); };
    auto colorRow = [&](const std::string& l, SDL_Color* c, int id, std::function<void()> chg, std::function<void()> reset) {
        Row x; x.type = R_COLOR; x.label = l; x.col = c; x.tap = [this, id] { colorEdit = colorEdit == id ? -1 : id; }; r.push_back(x);
        if (colorEdit == id) {
            const char* nm[4] = {"   Red", "   Green", "   Blue", "   Alpha"}; Uint8* cp[4] = {&c->r, &c->g, &c->b, &c->a};
            for (int k = 0; k < 4; k++) { Row s; s.type = R_SLIDER; s.label = nm[k]; s.mn = 0; s.mx = 255; Uint8* pp = cp[k]; s.get = [pp] { return (float)*pp; }; s.set = [this, pp, chg](float v) { *pp = (Uint8)std::lround(v); if (chg) chg(); settingsChanged(); }; r.push_back(s); }
            if (reset) act("   Reset this color", "", reset);
        }
    };
    auto page = [&](const std::string& l, int p) { act(l, ">", [this, p] { setPage = p; ovTarget = ovScroll = 0; colorEdit = -1; }); };
    switch (setPage) {
        case 0:
            page("Appearance", 1); page("Colors", 2); page("Background image", 3); page("Editor", 4); page("Keyboard & touch", 5); page("Run & build", 6); page("Plugins", 7);
            act("Reset all settings", "", [this] { st = Settings(); applySettings(); settingsChanged(); say("Settings reset"); });
            info("Settings file: " + joinPath(prefDir, "settings.json")); break;
        case 1: {
            head("Theme");
            { Row x; x.type = R_CHOICE; x.label = "Color theme"; x.val = st.theme; x.tap = [this] { int i = (findTheme(st.theme) + 1) % (int)g_themes.size(); st.theme = g_themes[i].name; st.ov.clear(); applyTheme(); settingsChanged(); }; r.push_back(x); }
            head("Text");
            sl("Font size", &st.fontScale, 0.6f, 2.5f, [this] { loadFont(); follow = true; });
            sl("Line spacing", &st.lineSpace, 0.f, 0.8f, [this] { loadFont(); follow = true; });
            act("Font file...", st.fontFile.empty() ? "default" : baseOf(st.fontFile), [this] {
                openPicker(PK_FILE, "Choose a monospace font", startDir(), "ttf otf ttc", [this](const std::string& p) { std::string old = st.fontFile; st.fontFile = p; if (!loadFont()) { st.fontFile = old; loadFont(); say("Cannot load font"); } else { settingsChanged(); follow = true; } }); });
            act("Use default font", "", [this] { st.fontFile.clear(); loadFont(); settingsChanged(); });
            head("Look & feel");
            sl("Corner radius", &st.radius, 0.f, 20.f);
            tog("Animations", &st.anim); sl("Animation speed", &st.animSpeed, 0.3f, 3.f);
            break; }
        case 2: {
            info("Tap a color to edit it. Changes are saved as overrides on top of the theme.");
            for (int i = 0; i < NCOL; i++) colorRow(COLOR_LABELS[i], &T.c[i], i, [this, i] { st.ov[COLOR_KEYS[i]] = toHex(T.c[i]); }, [this, i] { st.ov.erase(COLOR_KEYS[i]); applyTheme(); settingsChanged(); });
            act("Reset all colors", "", [this] { st.ov.clear(); applyTheme(); settingsChanged(); }); break; }
        case 3: {
            act("Choose image...", st.bgImage.empty() ? "none" : baseOf(st.bgImage), [this] {
                openPicker(PK_IMAGE, "Choose a background photo", startDir(), "png jpg jpeg bmp gif webp tga", [this](const std::string& p) { st.bgImage = p; loadBackground(); settingsChanged(); }); });
            act("Remove image", "", [this] { st.bgImage.clear(); loadBackground(); settingsChanged(); });
            ch("Fit mode", &st.bgMode, {"Cover", "Fit", "Stretch", "Tile", "Center"});
            sl("Image opacity", &st.bgOpacity, 0.f, 1.f); sl("Darken", &st.bgDim, 0.f, 0.95f);
            sl("Blur", &st.bgBlur, 0.f, 8.f, [this] { bgReloadAt = SDL_GetTicks() + 300; });
            sl("Tint strength", &st.tintStr, 0.f, 1.f); colorRow("Tint color", &st.tint, 1000, nullptr, nullptr);
            sl("Panel opacity", &st.paneAlpha, 0.2f, 1.f); tog("Slow drift animation", &st.bgDrift);
            info(FLOWY_IMG ? "Formats: PNG, JPG, WebP, BMP, GIF, TGA (SDL_image)" : "Built without SDL_image: only BMP files are supported"); break; }
        case 4:
            isl("Tab width", &st.tabW, 1, 8, [this] { TABW = st.tabW; for (auto& d : docs) d.validTo = 1; });
            tog("Line numbers", &st.lineNums); tog("Indent guides", &st.guides); tog("Show whitespace", &st.whitespace);
            tog("Highlight current line", &st.curLine); tog("Highlight matching bracket", &st.bracket);
            tog("Auto-close brackets & quotes", &st.autoClose, [this] { g_autoClose = st.autoClose; });
            tog("Smooth scrolling", &st.smooth); tog("Scrollbar", &st.scrollbar);
            ch("Caret style", &st.caret, {"Bar", "Block", "Underline"}); tog("Caret blink", &st.blink); break;
        case 5:
            ch("Keyboard", &st.kbMode, {"System / hardware", "Built-in"}, [this] { startTextInput(); if (st.kbMode == 1 && mobile) { kbWanted = true; kbA.to(1); } });
            sl("Built-in keyboard size", &st.kbScale, 0.7f, 1.5f); tog("Button toolbar", &st.toolbar);
            info("Built-in keyboard: letters, digits, symbols, arrows, Tab, autocomplete suggestions."); info("Toggle anytime with the Kbd button or F4."); break;
        case 6: {
            Lang l = doc().lang; head("Language: " + LANGS[l].name);
            info(runTemplate(l).empty() ? "(no run command)" : runTemplate(l));
            act("Edit run command...", "", [this, l] { promptInput("Run command", toU32(runTemplate(l)), [this, l](const std::string& s) { st.runCmd[LANGS[l].name] = s; settingsChanged(); }); });
            act("Reset to default", "", [this, l] { st.runCmd.erase(LANGS[l].name); settingsChanged(); });
            act("Arguments for this file...", doc().args, [this] { promptInput("Arguments", toU32(doc().args), [this](const std::string& s) { doc().args = s; }); });
            act("Working directory...", doc().cwd.empty() ? "file folder" : doc().cwd, [this] { openPicker(PK_FOLDER, "Working directory", startDir(), "", [this](const std::string& p) { doc().cwd = p; }); });
            tog("Save file before running", &st.autoSave);
            info("Placeholders: {file} {dir} {name} {stem} {ext} {out} {args}"); info("F5 run, Shift+F5 stop, Ctrl+F5 run with arguments."); break; }
        case 7:
            for (auto& p : pluginInfo) { head(p.file); info(p.status); }
            if (pluginInfo.empty()) info("No plugins found.");
            act("Reload plugins", "", [this] { initRegistries(); loadPlugins(); applyTheme(); for (auto& d : docs) d.validTo = 1; say("Plugins reloaded"); });
            act("Create sample plugin", "", [this] { createSamplePlugin(); });
            info("Plugin folder: " + joinPath(prefDir, "plugins")); info("Plugins are .json files: languages, themes, snippets, tasks (see README)."); break;
    }
    return r;
}
void App::drawSettings() {
    static const char* titles[] = {"Settings", "Appearance", "Colors", "Background image", "Editor", "Keyboard & touch", "Run & build", "Plugins"};
    drawPanelFrame(titles[setPage], setPage != 0);
    std::vector<Row> rows = settingsRows(); int rh = (int)((lh + pad * 2) * (mobile ? 1.3f : 1.1f)); float total = 0;
    for (auto& x : rows) total += x.type == R_HEAD ? rh * 0.8f : (x.type == R_INFO ? lh + pad : rh);
    ovContentH = total + pad; ovTarget = std::min(std::max(0.f, ovTarget), std::max(0.f, ovContentH - ovList.h));
    clip(ovList); float y = (float)ovList.y - ovScroll;
    for (auto& x : rows) {
        int h = x.type == R_HEAD ? (int)(rh * 0.8f) : (x.type == R_INFO ? lh + pad : rh); SDL_Rect r{ovList.x, (int)y, ovList.w, h}; y += h;
        if (r.y + r.h < ovList.y || r.y > ovList.y + ovList.h) continue;
        SDL_Rect hit = r; hit.y = std::max(hit.y, ovList.y); hit.h = std::min(r.y + r.h, ovList.y + ovList.h) - hit.y; int ty = r.y + (h - lh) / 2;
        switch (x.type) {
            case R_HEAD: text(x.label, r.x + pad * 2, r.y + h - lh - 2, T.c[C_ACCENT]); break;
            case R_INFO: { std::string s = x.label; int maxc = std::max(8, (r.w - pad * 4) / cw); if ((int)s.size() > maxc) s = s.substr(0, (size_t)maxc - 1) + "~"; text(s, r.x + pad * 2, ty, T.c[C_GUTTERFG]); break; }
            case R_ACTION: case R_CHOICE: {
                text(x.label, r.x + pad * 2, ty, T.c[C_FG]); if (!x.val.empty()) text(x.val, r.x + r.w - textW(x.val) - pad * 2, ty, T.c[C_ACCENT]);
                fill(r.x + pad, r.y + h - 1, r.w - pad * 2, 1, withA(T.c[C_FG], 0.06f)); addBtn(hit, "", x.tap); break; }
            case R_TOGGLE: {
                text(x.label, r.x + pad * 2, ty, T.c[C_FG]); int tw = lh * 2, th = lh; SDL_Rect sw{r.x + r.w - tw - pad * 2, r.y + (h - th) / 2, tw, th};
                rr(sw, x.on ? T.c[C_ACCENT] : T.c[C_BTN], th / 2); circle(sw.x + (x.on ? tw - th / 2 : th / 2), sw.y + th / 2, th / 2 - 3, T.c[C_FG]);
                fill(r.x + pad, r.y + h - 1, r.w - pad * 2, 1, withA(T.c[C_FG], 0.06f)); addBtn(hit, "", x.tap); break; }
            case R_COLOR: {
                text(x.label, r.x + pad * 2, ty, T.c[C_FG]); SDL_Rect sw{r.x + r.w - rh * 2 - pad * 2, r.y + pad, rh * 2, h - pad * 2};
                fill(sw.x, sw.y, sw.w / 2, sw.h, C(90, 90, 90)); fill(sw.x + sw.w / 2, sw.y, sw.w / 2, sw.h, C(150, 150, 150)); rr(sw, *x.col, rad / 2);
                std::string hx = toHex(*x.col); text(hx, sw.x - textW(hx) - pad, ty, T.c[C_GUTTERFG]); addBtn(hit, "", x.tap); break; }
            case R_SLIDER: {
                text(x.label, r.x + pad * 2, ty, T.c[C_FG]); float v = x.get(); char buf[24]; if (x.mx - x.mn > 20) snprintf(buf, sizeof buf, "%d", (int)std::lround(v)); else snprintf(buf, sizeof buf, "%.2f", v);
                int vw = cw * 6; text(buf, r.x + r.w - vw - pad, ty, T.c[C_ACCENT]);
                int tx = r.x + (int)(r.w * 0.40f), tw = r.x + r.w - vw - pad * 2 - tx; SDL_Rect track{tx, r.y + h / 2 - 3, std::max(10, tw), 6};
                float f = std::min(1.f, std::max(0.f, (v - x.mn) / std::max(1e-6f, x.mx - x.mn)));
                rr(track, T.c[C_BTN], 3); rr(SDL_Rect{track.x, track.y, std::max(6, (int)(track.w * f)), 6}, T.c[C_ACCENT], 3); circle(track.x + (int)(track.w * f), track.y + 3, std::max(7, lh / 2 - 2), T.c[C_FG]);
                SDL_Rect sh{track.x - lh / 2, hit.y, track.w + lh, hit.h}; if (hit.h > 4) sliders.push_back({sh, x.mn, x.mx, x.set});
                break; }
        }
    }
    unclip();
}

// =================================================================== picker ====
static bool isImgExt(const std::string& e) { return e == "png" || e == "jpg" || e == "jpeg" || e == "bmp" || e == "gif" || e == "webp" || e == "tga" || e == "tif" || e == "tiff"; }
void App::pickerGo(const std::string& dir) {
    std::error_code ec; fs::path p = fsPath(dir.empty() ? "." : dir);
    if (!fs::is_directory(p, ec)) { say("Cannot open folder"); return; }
    std::set<std::string> exts; { std::istringstream ss(pk.exts); std::string w; while (ss >> w) exts.insert(w); }
    pk.ents.clear();
    for (fs::directory_iterator it(p, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
        Entry e; e.name = fsStr(it->path().filename()); if (e.name.empty() || (!pk.hidden && e.name[0] == '.')) continue;
        std::error_code e2; e.dir = it->is_directory(e2); std::string ext; size_t dot = e.name.rfind('.'); if (dot != NPOS) ext = lowerS(e.name.substr(dot + 1));
        if (!e.dir) { if (pk.mode == PK_FOLDER) continue; if (!exts.empty() && !exts.count(ext)) continue; e.size = (uintmax_t)it->file_size(e2); e.img = isImgExt(ext); }
        pk.ents.push_back(e);
    }
    std::sort(pk.ents.begin(), pk.ents.end(), [](const Entry& a, const Entry& b) { if (a.dir != b.dir) return a.dir; return lowerS(a.name) < lowerS(b.name); });
    std::string d = fsStr(p); while (d.size() > 1 && (d.back() == '/' || d.back() == '\\') && !(d.size() == 3 && d[1] == ':')) d.pop_back();
    pk.dir = d; pk.sel.clear(); ovScroll = ovTarget = 0; loader.clearQueue();
    for (auto& t : thumbs) t.second.pending = t.second.t ? false : false;
}
void App::openPicker(int mode, const std::string& title, const std::string& dir, const std::string& exts, std::function<void(const std::string&)> done, const std::string& name) {
    pk = Picker(); pk.mode = mode; pk.title = title; pk.exts = exts; pk.done = std::move(done); pk.name = toU32(name);
    startOverlay(*this, OV_PICKER); std::string d = dir; if (d.empty() || !isDirPath(d)) d = !prefDir.empty() ? prefDir : ".";
    if (mode == PK_IMAGE) loader.start();
    pickerGo(d); if (mode == PK_SAVE && mobile && st.kbMode == 1) { kbWanted = true; kbA.to(1); }
}
void App::drawPicker() {
    drawPanelFrame(pk.title, false); bool image = pk.mode == PK_IMAGE;
    int rh = (int)((lh + pad * 2) * (mobile ? 1.35f : 1.1f)); SDL_Rect pn = ovPanel;
    // up button + path + hidden toggle
    int ph = rowH; SDL_Rect up{ovList.x + pad, ovList.y + pad / 2, ph, ph - pad}; rr(up, T.c[C_BTN], rad); icon(KC_UP, up, T.c[C_FG]);
    addBtn(up, "", [this] { fs::path pp = fsPath(pk.dir).parent_path(); std::string s = fsStr(pp); if (!s.empty() && s != pk.dir) pickerGo(s); });
    SDL_Rect hid{pn.x + pn.w - cw * 5 - pad * 2, ovList.y + pad / 2, cw * 5, ph - pad}; rr(hid, pk.hidden ? T.c[C_ACCENT] : T.c[C_BTN], rad); text("dot", hid.x + (hid.w - textW("dot")) / 2, hid.y + (hid.h - lh) / 2, T.c[C_FG]);
    addBtn(hid, "", [this] { pk.hidden = !pk.hidden; pickerGo(pk.dir); });
    { SDL_Rect pr_{up.x + up.w + pad, up.y, hid.x - up.x - up.w - pad * 2, up.h}; rr(pr_, T.c[C_BG], rad); int maxc = std::max(4, (pr_.w - pad * 2) / cw); std::string s = pk.dir; if ((int)toU32(s).size() > maxc) { U32 u = toU32(s); s = "~" + toU8(u.substr(u.size() - (size_t)maxc + 1)); } clip(pr_); text(s, pr_.x + pad, pr_.y + (pr_.h - lh) / 2, T.c[C_FG]); unclip(); }
    ovList.y += ph; ovList.h -= ph;
    // bottom bar
    int bottomH = pk.mode == PK_OPEN || pk.mode == PK_FILE ? rowH : (image ? rowH + (int)(lh * 4.2f) : rowH); SDL_Rect bar{pn.x, pn.y + pn.h - bottomH, pn.w, bottomH};
    ovList.h -= bottomH;
    // places (sidebar if wide, chips otherwise)
    bool wide = pn.w > 700; int side = wide ? std::min(pn.w / 4, cw * 20) : 0;
    if (wide) {
        SDL_Rect sb{ovList.x, ovList.y, side, ovList.h}; fill(sb, withA(T.c[C_BG], 0.5f)); clip(sb); int y = sb.y + pad;
        for (auto& pl : places) { SDL_Rect r{sb.x + pad / 2, y, side - pad, lh + pad}; if (pk.dir == pl.second) rr(r, withA(T.c[C_SEL], 0.8f), rad / 2); text(pl.first, r.x + pad, r.y + pad / 2, T.c[C_FG]); std::string d = pl.second; addBtn(r, "", [this, d] { pickerGo(d); }); y += lh + pad; }
        unclip(); ovList.x += side; ovList.w -= side;
    } else {
        int x = ovList.x + pad, y = ovList.y; int chipH = lh + pad; SDL_Rect row{ovList.x, y, ovList.w, chipH + pad / 2}; clip(row);
        for (auto& pl : places) { int w = textW(pl.first) + pad * 2; if (x + w > ovList.x + ovList.w) break; SDL_Rect r{x, y + pad / 4, w, chipH}; rr(r, pk.dir == pl.second ? T.c[C_ACCENT] : T.c[C_BTN], rad); text(pl.first, r.x + pad, r.y + pad / 2, T.c[C_FG]); std::string d = pl.second; addBtn(r, "", [this, d] { pickerGo(d); }); x += w + pad / 2; }
        unclip(); ovList.y += chipH + pad; ovList.h -= chipH + pad;
    }
    // entries
    int cell = image ? std::max(lh * 5, (int)(110 * uiScale)) : 0, cols = image ? std::max(2, ovList.w / cell) : 1, cellW = image ? ovList.w / cols : ovList.w, cellH = image ? cellW + lh + pad : rh;
    int rowsN = ((int)pk.ents.size() + cols - 1) / cols; ovContentH = (float)(rowsN * cellH + pad); ovTarget = std::min(std::max(0.f, ovTarget), std::max(0.f, ovContentH - ovList.h));
    clip(ovList); int pend = 0; for (auto& t : thumbs) if (t.second.pending) pend++;
    if (pk.ents.empty()) text("(empty)", ovList.x + pad * 2, ovList.y + pad, T.c[C_GUTTERFG]);
    for (int i = 0; i < (int)pk.ents.size(); i++) {
        Entry& e = pk.ents[i]; int cx = i % cols, cy = i / cols; SDL_Rect r{ovList.x + cx * cellW, ovList.y + cy * cellH - (int)ovScroll, cellW, cellH};
        if (r.y + r.h < ovList.y || r.y > ovList.y + ovList.h) continue;
        std::string full = joinPath(pk.dir, e.name); bool sel = !e.dir && (pk.sel == full || (pk.mode == PK_SAVE && toU8(pk.name) == e.name));
        SDL_Rect hit = r; hit.y = std::max(hit.y, ovList.y); hit.h = std::min(r.y + r.h, ovList.y + ovList.h) - hit.y;
        if (image) {
            SDL_Rect in{r.x + 3, r.y + 3, r.w - 6, r.h - 6}; rr(in, sel ? withA(T.c[C_SEL], 0.9f) : withA(T.c[C_BG], 0.6f), rad); SDL_Rect pic{in.x + 4, in.y + 4, in.w - 8, in.w - 8};
            if (e.dir) icon(IC_FOLDER, pic, T.c[C_ACCENT]);
            else if (e.img) {
                Thumb& th = thumbs[full];
                if (!th.t && !th.pending && !th.failed && pend < 6) { th.pending = true; loader.request(full); pend++; }
                if (th.t) { float sc = std::min((float)pic.w / th.w, (float)pic.h / th.h); int dw = (int)(th.w * sc), dh = (int)(th.h * sc); SDL_Rect d{pic.x + (pic.w - dw) / 2, pic.y + (pic.h - dh) / 2, dw, dh}; SDL_RenderCopy(ren, th.t, nullptr, &d); }
                else icon(IC_IMG, pic, T.c[C_GUTTERFG]);
            } else icon(IC_FILE, pic, T.c[C_GUTTERFG]);
            std::string nm = e.name; int maxc = std::max(3, (in.w - pad) / cw); U32 u = toU32(nm); if ((int)u.size() > maxc) nm = toU8(u.substr(0, (size_t)maxc - 1)) + "~"; text(nm, in.x + pad / 2, pic.y + pic.h + 2, T.c[C_FG]);
        } else {
            if (sel) fill(r, withA(T.c[C_SEL], 0.8f));
            icon(e.dir ? IC_FOLDER : IC_FILE, SDL_Rect{r.x + pad, r.y, rh, rh}, e.dir ? T.c[C_ACCENT] : T.c[C_GUTTERFG]);
            text(e.name, r.x + rh + pad * 2, r.y + (rh - lh) / 2, T.c[C_FG]);
            if (!e.dir) { char b[32]; double s = (double)e.size; const char* u = "B"; if (s > 1024) { s /= 1024; u = "KB"; } if (s > 1024) { s /= 1024; u = "MB"; } snprintf(b, sizeof b, "%.*f %s", s < 10 && *u != 'B' ? 1 : 0, s, u); text(b, r.x + r.w - textW(b) - pad * 2, r.y + (rh - lh) / 2, T.c[C_GUTTERFG]); }
            fill(r.x + pad, r.y + rh - 1, r.w - pad * 2, 1, withA(T.c[C_FG], 0.05f));
        }
        std::string name = e.name; bool isDir = e.dir;
        addBtn(hit, "", [this, name, isDir, full] {
            if (isDir) { pickerGo(full); return; }
            if (pk.mode == PK_SAVE) { pk.name = toU32(name); pk.sel.clear(); return; }
            if (pk.mode == PK_IMAGE) { pk.sel = full; return; }
            auto f = pk.done; closeOverlay(); if (f) f(full); });
    }
    unclip();
    // bottom bar content
    fill(bar, T.c[C_POPUP]); fill(bar.x, bar.y, bar.w, 1, withA(T.c[C_FG], 0.1f)); int right = bar.x + bar.w - pad;
    auto btn = [&](const std::string& l, std::function<void()> f, bool acc, int row) { int w = std::max(cw * 8, textW(l) + pad * 3); right -= w; SDL_Rect r{right, bar.y + pad / 2 + row, w, rowH - pad}; rr(r, acc ? T.c[C_ACCENT] : T.c[C_BTN], rad); text(l, r.x + (w - textW(l)) / 2, r.y + (r.h - lh) / 2, T.c[C_FG]); addBtn(r, l, std::move(f)); right -= pad; };
    int row0 = image ? (int)(lh * 4.2f) : 0;
    if (pk.mode == PK_SAVE) {
        btn("Save", [this] {
            if (pk.name.empty()) { say("Enter a file name"); return; } std::string p = joinPath(pk.dir, toU8(pk.name));
            if (pathExists(p) && pk.sel != p) { pk.sel = p; say("File exists - press Save again to overwrite"); return; }
            auto f = pk.done; closeOverlay(); if (f) f(p); }, true, 0);
        SDL_Rect f{bar.x + pad, bar.y + pad / 2, right - bar.x - pad, rowH - pad}; rr(f, T.c[C_BG], rad); setc(T.c[C_ACCENT]); SDL_RenderDrawRect(ren, &f);
        int maxc = std::max(1, (f.w - pad * 2) / cw); U32 shown = (int)pk.name.size() > maxc ? pk.name.substr(pk.name.size() - (size_t)maxc) : pk.name; clip(f); textU(shown, f.x + pad, f.y + (f.h - lh) / 2, T.c[C_FG]);
        fill(f.x + pad + (int)shown.size() * cw, f.y + 3, 2, f.h - 6, withA(T.c[C_CURSOR], caretAlpha())); unclip();
    } else if (pk.mode == PK_FOLDER) {
        btn("Select this folder", [this] { auto f = pk.done; std::string d = pk.dir; closeOverlay(); if (f) f(d); }, true, 0); btn("Cancel", [this] { closeOverlay(); }, false, 0);
    } else if (image) {
        SDL_Rect pv{bar.x + pad, bar.y + pad, (int)(lh * 4.2f) - pad, (int)(lh * 4.2f) - pad}; rr(pv, withA(T.c[C_BG], 0.7f), rad);
        if (!pk.sel.empty()) { auto it = thumbs.find(pk.sel); if (it != thumbs.end() && it->second.t) { Thumb& th = it->second; float sc = std::min((float)pv.w / th.w, (float)pv.h / th.h); int dw = (int)(th.w * sc), dh = (int)(th.h * sc); SDL_Rect d{pv.x + (pv.w - dw) / 2, pv.y + (pv.h - dh) / 2, dw, dh}; SDL_RenderCopy(ren, th.t, nullptr, &d); } text(baseOf(pk.sel), pv.x + pv.w + pad, pv.y, T.c[C_FG]); }
        else text("Tap a photo to preview", pv.x + pv.w + pad, pv.y, T.c[C_GUTTERFG]);
        right = bar.x + bar.w - pad; int y2 = row0;
        auto btn2 = [&](const std::string& l, std::function<void()> f, bool acc) { int w = std::max(cw * 9, textW(l) + pad * 3); right -= w; SDL_Rect r{right, bar.y + y2 + pad / 2, w, rowH - pad}; rr(r, acc ? T.c[C_ACCENT] : T.c[C_BTN], rad); text(l, r.x + (w - textW(l)) / 2, r.y + (r.h - lh) / 2, T.c[C_FG]); addBtn(r, l, std::move(f)); right -= pad; };
        btn2("Use photo", [this] { if (pk.sel.empty()) { say("Tap a photo first"); return; } auto f = pk.done; std::string s = pk.sel; closeOverlay(); if (f) f(s); }, true); btn2("Cancel", [this] { closeOverlay(); }, false);
    } else btn("Cancel", [this] { closeOverlay(); }, false, 0);
}
void App::overlayKey(SDL_Keycode k, bool ctrl, bool shift) {
    (void)ctrl; (void)shift;
    if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) { if (ov == OV_SETTINGS && setPage > 0) { setPage = 0; ovTarget = ovScroll = 0; colorEdit = -1; } else closeOverlay(); return; }
    if (ov == OV_MENU) {
        std::vector<int> idx; std::string lq = lowerS(toU8(palQuery));
        for (int i = 0; i < (int)pal.size(); i++) if (lq.empty() || lowerS(pal[i].name).find(lq) != NPOS) idx.push_back(i);
        if (k == SDLK_UP) palSel = std::max(0, palSel - 1); else if (k == SDLK_DOWN) palSel = std::min((int)idx.size() - 1, palSel + 1);
        else if (k == SDLK_BACKSPACE) { if (!palQuery.empty()) palQuery.pop_back(); palSel = 0; }
        else if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && !idx.empty()) { auto f = pal[idx[(size_t)palSel]].fn; closeOverlay(); if (f) f(); }
        int rh = (int)((lh + pad * 2) * (mobile ? 1.25f : 1.f)); if (palSel * rh < ovTarget) ovTarget = (float)(palSel * rh); if ((palSel + 1) * rh > ovTarget + ovList.h) ovTarget = (float)((palSel + 1) * rh - ovList.h);
    } else if (ov == OV_PICKER) {
        if (k == SDLK_BACKSPACE) { if (pk.mode == PK_SAVE && !pk.name.empty()) pk.name.pop_back(); else { std::string s = fsStr(fsPath(pk.dir).parent_path()); if (!s.empty() && s != pk.dir) pickerGo(s); } }
        else if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && pk.mode == PK_SAVE && !pk.name.empty()) {
            std::string p = joinPath(pk.dir, toU8(pk.name)); if (pathExists(p) && pk.sel != p) { pk.sel = p; say("File exists - Enter again to overwrite"); return; }
            auto f = pk.done; closeOverlay(); if (f) f(p);
        } else if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && pk.mode == PK_FOLDER) { auto f = pk.done; std::string d = pk.dir; closeOverlay(); if (f) f(d); }
    }
}
