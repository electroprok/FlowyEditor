#pragma once
// ================================================================ primitives ====
void App::rr(SDL_Rect r, SDL_Color c, int radius) {
    if (c.a == 0 || r.w <= 0 || r.h <= 0) return;
    if (radius < 0) radius = rad;
    radius = std::min(radius, std::min(r.w, r.h) / 2); setc(c);
    if (radius <= 1) { SDL_RenderFillRect(ren, &r); return; }
    for (int i = 0; i < radius; i++) {
        float dy = radius - i - 0.5f; int inset = radius - (int)std::sqrt((float)radius * radius - dy * dy);
        SDL_Rect a{r.x + inset, r.y + i, r.w - 2 * inset, 1}, b{r.x + inset, r.y + r.h - 1 - i, r.w - 2 * inset, 1};
        SDL_RenderFillRect(ren, &a); SDL_RenderFillRect(ren, &b);
    }
    SDL_Rect m{r.x, r.y + radius, r.w, r.h - 2 * radius}; if (m.h > 0) SDL_RenderFillRect(ren, &m);
}
void App::circle(int cx, int cy, int r, SDL_Color c) {
    setc(c);
    for (int dy = -r; dy <= r; dy++) { int hw = (int)std::sqrt((float)(r * r - dy * dy)); SDL_Rect q{cx - hw, cy + dy, hw * 2 + 1, 1}; SDL_RenderFillRect(ren, &q); }
}
void App::shadow(const SDL_Rect& r, int size, int alpha) {
    for (int i = size; i >= 1; i--) { SDL_Rect q{r.x - i, r.y - i + size / 3, r.w + 2 * i, r.h + 2 * i}; rr(q, C(0, 0, 0, std::max(1, alpha / size / 2)), rad + i); }
}
void App::tri(int cx, int cy, int s, int dir, SDL_Color c) {   // dir: 0 right 1 left 2 up 3 down
    setc(c); int x0 = cx - s / 2, y0 = cy - s / 2;
    for (int i = 0; i < s; i++) {
        SDL_Rect q;
        if (dir == 0) q = {x0 + i, cy - (s - i) / 2, 1, s - i};
        else if (dir == 1) q = {x0 + s - 1 - i, cy - (s - i) / 2, 1, s - i};
        else if (dir == 2) q = {cx - (i + 1) / 2, y0 + i, i + 1, 1};
        else q = {cx - (s - i) / 2, y0 + i, s - i, 1};
        SDL_RenderFillRect(ren, &q);
    }
}
void App::icon(int code, const SDL_Rect& r, SDL_Color c) {
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2, s = std::max(6, (int)(std::min(r.w, r.h) * 0.42f)), t = std::max(1, (int)(uiScale * 1.2f)); setc(c);
    auto line = [&](int x1, int y1, int x2, int y2) { for (int k = 0; k < t; k++) { SDL_RenderDrawLine(ren, x1, y1 + k, x2, y2 + k); } };
    switch (code) {
        case KC_LEFT: tri(cx, cy, s, 1, c); break; case KC_RIGHT: tri(cx, cy, s, 0, c); break;
        case KC_UP: tri(cx, cy, s, 2, c); break; case KC_DOWN: tri(cx, cy, s, 3, c); break;
        case IC_RUN: tri(cx, cy, s + 2, 0, c); break;
        case IC_STOP: fill(cx - s / 2, cy - s / 2, s, s, c); break;
        case KC_SHIFT: tri(cx, cy - s / 5, s, 2, c); fill(cx - s / 6, cy + s / 4, std::max(2, s / 3), s / 3 + 1, c); break;
        case KC_BKSP: { int w = s + s / 2, h = s / 2 + 2; line(cx - w / 2, cy, cx - w / 4, cy - h); line(cx - w / 4, cy - h, cx + w / 2, cy - h); line(cx + w / 2, cy - h, cx + w / 2, cy + h);
            line(cx + w / 2, cy + h, cx - w / 4, cy + h); line(cx - w / 4, cy + h, cx - w / 2, cy); line(cx - w / 8, cy - h / 2, cx + w / 4, cy + h / 2); line(cx - w / 8, cy + h / 2, cx + w / 4, cy - h / 2); break; }
        case KC_ENTER: { int w = s; line(cx + w / 2, cy - w / 2, cx + w / 2, cy); line(cx + w / 2, cy, cx - w / 2, cy); line(cx - w / 2, cy, cx - w / 4, cy - w / 4); line(cx - w / 2, cy, cx - w / 4, cy + w / 4); break; }
        case KC_HIDE: line(cx - s / 2, cy - s / 4, cx, cy + s / 4); line(cx, cy + s / 4, cx + s / 2, cy - s / 4); break;
        case IC_BACK: line(cx + s / 4, cy - s / 2, cx - s / 4, cy); line(cx - s / 4, cy, cx + s / 4, cy + s / 2); break;
        case IC_CLOSE: line(cx - s / 2, cy - s / 2, cx + s / 2, cy + s / 2); line(cx - s / 2, cy + s / 2, cx + s / 2, cy - s / 2); break;
        case IC_PLUS: fill(cx - s / 2, cy - t / 2, s, t + 1, c); fill(cx - t / 2, cy - s / 2, t + 1, s, c); break;
        case IC_MENU: for (int k = -1; k <= 1; k++) fill(cx - s / 2, cy + k * (s / 2) - t / 2, s, t + 1, c); break;
        case IC_FOLDER: fill(cx - s / 2, cy - s / 3, s / 2, s / 4, c); fill(cx - s / 2, cy - s / 3 + s / 5, s, s * 2 / 3, c); break;
        case IC_FILE: { SDL_Rect q{cx - s / 3, cy - s / 2, s * 2 / 3 + 1, s}; outline(q, c); line(cx - s / 6, cy - s / 6, cx + s / 6, cy - s / 6); line(cx - s / 6, cy + s / 8, cx + s / 6, cy + s / 8); break; }
        case IC_IMG: { SDL_Rect q{cx - s / 2, cy - s / 3, s + 1, s * 2 / 3 + 1}; outline(q, c); circle(cx - s / 5, cy - s / 8, std::max(1, s / 8), c); tri(cx + s / 6, cy + s / 12, s / 2, 2, c); break; }
        case IC_GEAR: circle(cx, cy, s / 2, c); circle(cx, cy, std::max(1, s / 5), T.c[C_BAR]); break;
        default: break;
    }
}
void App::glyph(char32_t ch, int x, int y, SDL_Color c) {
    if (ch < 32 || !font) return;
    auto it = glyphs.find((Uint32)ch);
    if (it == glyphs.end()) {
        Glyph g; Uint16 gc = ch < 0x10000 ? (Uint16)ch : (Uint16)'?'; SDL_Color white{255, 255, 255, 255};
        SDL_Surface* sf = TTF_RenderGlyph_Blended(font, gc, white);
        if (sf) { g.t = SDL_CreateTextureFromSurface(ren, sf); g.w = sf->w; g.h = sf->h; SDL_FreeSurface(sf); if (g.t) SDL_SetTextureBlendMode(g.t, SDL_BLENDMODE_BLEND); }
        it = glyphs.emplace((Uint32)ch, g).first;
    }
    Glyph& g = it->second; if (!g.t) return;
    SDL_SetTextureColorMod(g.t, c.r, c.g, c.b); SDL_SetTextureAlphaMod(g.t, c.a);
    SDL_Rect d{x, y + (lh - g.h) / 2, g.w, g.h}; SDL_RenderCopy(ren, g.t, nullptr, &d);
}
int App::textU(const U32& u, int x, int y, SDL_Color c) { int px = x; for (char32_t ch : u) { if (ch != ' ') glyph(ch, px, y, c); px += cw; } return px - x; }
int App::text(const std::string& s, int x, int y, SDL_Color c) { return textU(toU32(s), x, y, c); }

// ================================================================== layout =====
std::vector<TBItem> App::toolbarItems() {
    std::vector<TBItem> v;
    auto sym = [&](const char* s) { v.push_back({s, [this, s] { doc().typeText(toU32(s)); afterEdit(false); }, 0}); };
    v.push_back({"Tab", [this] { sendKey(SDLK_TAB); }, 0});
    v.push_back({"Undo", [this] { doc().doUndo(); follow = true; }, 0});
    v.push_back({"Redo", [this] { doc().doRedo(); follow = true; }, 0});
    v.push_back({"<", [this] { sendKey(SDLK_LEFT); }, 0}); v.push_back({">", [this] { sendKey(SDLK_RIGHT); }, 0});
    v.push_back({"^", [this] { sendKey(SDLK_UP); }, 0}); v.push_back({"v", [this] { sendKey(SDLK_DOWN); }, 0});
    for (const char* s : {"{", "}", "(", ")", "[", "]", ";", "\"", "'", "=", "/", "#", "_", ":", "*", "&"}) sym(s);
    v.push_back({"Cmp", [this] { refreshComp(true); }, 0}); v.push_back({"Save", [this] { saveDoc(); }, 0});
    v.push_back({"Open", [this] { openDialog(); }, 0}); v.push_back({"Run", [this] { runDoc(false); }, 0});
    v.push_back({"Find", [this] { openPrompt(P_FIND); }, 0}); v.push_back({"Menu", [this] { mainMenu(); }, 0});
    return v;
}
void App::layout() {
    SDL_GetRendererOutputSize(ren, &W, &H); int ww, wh; SDL_GetWindowSize(win, &ww, &wh);
    pxX = ww ? (float)W / ww : 1.f; pxY = wh ? (float)H / wh : 1.f; rad = (int)(st.radius * uiScale * 0.8f);
    tabH = lh + pad * 2; statH = lh + pad; rowH = lh + pad * 2; barH = (int)(rowH * barA.v + 0.5f);
    int keyH = (int)(lh * 1.75f * st.kbScale), gap = std::max(2, pad / 2), maxKb = (int)(H * 0.58f);
    kbFull = 5 * (keyH + gap) + gap; if (kbFull > maxKb) { keyH = std::max(10, (maxKb - gap) / 5 - gap); kbFull = 5 * (keyH + gap) + gap; }
    kbKeyH = keyH; kbGap = gap; kbH = (int)(kbFull * kbA.v + 0.5f);
    tbH = 0; tbItems.clear();
    if (st.toolbar && kbA.v < 0.01f) {
        tbItems = toolbarItems(); tbBh = (int)(rowH * (mobile ? 1.25f : 1.f)); int x = 0, rows = 1;
        for (auto& t : tbItems) { t.w = std::max(tbBh, textW(t.l) + pad * 3); if (x + t.w > W && x > 0) { rows++; x = 0; } x += t.w; }
        tbH = rows * tbBh;
    }
    outFull = std::max(lh * 6, (int)(H * 0.34f)); outH = (int)(outFull * outA.v + 0.5f);
    statRect = {0, H - kbH - statH, W, statH}; int tbY = statRect.y - tbH, outY = tbY - outH;
    edRect = {0, tabH + barH, W, std::max(lh * 2, outY - (tabH + barH))};
    outRect = {0, outY, W, outH}; kbRect = {0, H - kbH, W, kbH};
    int digits = 1; for (size_t n = docs.empty() ? 1 : doc().lines.size(); n >= 10; n /= 10) digits++;
    gutterW = st.lineNums ? (std::max(3, digits) + 2) * cw : cw;
}

// ================================================================ background ====
void App::drawBackground() {
    fill(0, 0, W, H, T.c[C_BG]);
    if (!bgTex) return;
    SDL_SetTextureAlphaMod(bgTex, (Uint8)(255 * std::min(1.f, std::max(0.f, st.bgOpacity))));
    float t = SDL_GetTicks() / 1000.f, dx = 0, dy = 0, extra = 1.f;
    if (st.bgDrift) { dx = std::sin(t * 0.07f) * 24 * uiScale; dy = std::cos(t * 0.05f) * 16 * uiScale; extra = 1.08f; }
    if (st.bgMode == 3) {   // tile
        int tw = std::max(8, (int)(bgW * std::max(1.f, uiScale * 0.5f))), th = std::max(8, (int)(bgH * std::max(1.f, uiScale * 0.5f)));
        for (int y = 0; y < H; y += th) for (int x = 0; x < W; x += tw) { SDL_Rect d{x, y, tw, th}; SDL_RenderCopy(ren, bgTex, nullptr, &d); }
    } else {
        float sc = 1.f;
        if (st.bgMode == 0) sc = std::max((float)W / bgW, (float)H / bgH) * extra;
        else if (st.bgMode == 1) sc = std::min((float)W / bgW, (float)H / bgH);
        else if (st.bgMode == 4) sc = std::max(1.f, uiScale * 0.5f);
        int dw = st.bgMode == 2 ? W : (int)(bgW * sc), dh = st.bgMode == 2 ? H : (int)(bgH * sc);
        SDL_Rect d{(W - dw) / 2 + (int)dx, (H - dh) / 2 + (int)dy, dw, dh}; SDL_RenderCopy(ren, bgTex, nullptr, &d);
    }
    fill(0, 0, W, H, C(0, 0, 0, (int)(255 * std::min(1.f, std::max(0.f, st.bgDim)))));
    if (st.tintStr > 0.01f) fill(0, 0, W, H, withA(st.tint, st.tintStr * 0.7f));
}

// ===================================================================== tabs ====
void App::drawTabs() {
    fill(0, 0, W, tabH, pane(T.c[C_BAR]));
    int ic = tabH, right = W;
    SDL_Rect mr{right - ic, 0, ic, tabH}; right -= ic; icon(IC_MENU, mr, T.c[C_BARFG]); addBtn(mr, "", [this] { mainMenu(); });
    SDL_Rect rn{right - ic, 0, ic, tabH}; right -= ic; bool run = proc.running;
    icon(run ? IC_STOP : IC_RUN, rn, run ? T.c[C_BAD] : T.c[C_GOOD]); addBtn(rn, "", [this] { if (proc.running) stopRun(); else runDoc(false); });
    int avail = right - tabH, n = (int)docs.size(), tw = std::min(cw * 22, avail / std::max(1, n)), x = 0;
    for (int i = 0; i < n; i++) {
        SDL_Rect r{x + 2, 3, tw - 4, tabH - 3}; bool act = i == ci;
        if (act) rr(r, pane(T.c[C_BG]), rad);
        SDL_Rect tc{r.x, r.y, r.w - cw * 2, r.h}; clip(tc);
        int tx = r.x + pad; if (docs[i].modified) { circle(tx + cw / 3, tabH / 2, std::max(2, cw / 4), T.c[C_ACCENT]); tx += cw; }
        text(docs[i].name(), tx, pad, act ? T.c[C_FG] : T.c[C_BARFG]); unclip();
        SDL_Rect cr{r.x + r.w - cw * 2 - pad / 2, 0, cw * 2 + pad / 2, tabH}; icon(IC_CLOSE, SDL_Rect{cr.x, cr.y, cr.w, cr.h}, withA(T.c[C_BARFG], 0.8f));
        int idx = i; addBtn(cr, "", [this, idx] { ci = idx; closeDoc(); }, -1); addBtn(r, "", [this, idx] { ci = idx; comp.open = false; follow = true; });
        if (act) { if (tabW2.v < 1) { tabX.snap((float)x); tabW2.snap((float)tw); } tabX.to((float)x); tabW2.to((float)tw); }
        x += tw;
    }
    SDL_Rect pl{x, 0, tabH, tabH}; icon(IC_PLUS, pl, T.c[C_ACCENT]); addBtn(pl, "", [this] { newDoc(); });
    rr(SDL_Rect{(int)tabX.v + pad, tabH - 3, std::max(4, (int)tabW2.v - pad * 2), 3}, T.c[C_ACCENT], 1);
}

// ================================================================== editor =====
float App::caretAlpha() {
    if (!st.blink) return 1.f;
    Uint32 t = SDL_GetTicks() - caretT; if (t < 500) return 1.f;
    t = (t - 500) % 1100;
    if (t < 550) return 1.f; if (t < 700) return 1.f - (t - 550) / 150.f; if (t < 950) return 0.f; return (t - 950) / 150.f;
}
bool App::matchBracketCached(Pos& a, Pos& b) {
    Doc& d = doc();
    if (mbDoc != ci || mbCacheFor != d.cur || mbEdits != d.edits) { mbDoc = ci; mbCacheFor = d.cur; mbEdits = d.edits; mbOk = d.matchBracket(mbA, mbB); }
    a = mbA; b = mbB; return mbOk;
}
void App::drawEditor() {
    Doc& d = doc(); SDL_Rect ed = edRect; fill(ed, pane(T.c[C_BG]));
    SDL_Rect code{ed.x + gutterW, ed.y, ed.w - gutterW, ed.h}; clip(code);
    int first = std::max(0, (int)(d.vsy / lh)), x0 = code.x + cw / 2 - (int)d.vsx, y0 = ed.y - (int)(d.vsy - first * lh), n = (int)d.lines.size();
    if (st.curLine && !d.hasSel()) fill(code.x, ed.y + (int)(clY.v - d.vsy), code.w, lh, T.c[C_CURLINE]);
    Pos sa, sb; d.selRange(sa, sb); bool sel = d.hasSel();
    bool search = (pr.mode == P_FIND || pr.mode == P_REPLACE) && !pr.f[0].empty();
    std::vector<uint8_t> tk; int lastInd = 0; SDL_Color gcol = withA(T.c[C_FG], 0.13f), wcol = withA(T.c[C_FG], 0.22f);
    for (int l = first, y = y0; l < n && y < ed.y + ed.h; l++, y += lh) {
        const U32& s = d.lines[l];
        if (sel && l >= sa.l && l <= sb.l) {
            int sc = l == sa.l ? sa.c : 0, ec = l == sb.l ? sb.c : (int)s.size();
            int x1 = x0 + d.colToVis(l, sc) * cw, x2 = x0 + d.colToVis(l, ec) * cw + (l < sb.l ? cw / 2 : 0); rr(SDL_Rect{x1, y, std::max(2, x2 - x1), lh}, T.c[C_SEL], std::max(0, rad / 3));
        }
        if (search) { size_t p = 0; while ((p = findIn(s, pr.f[0], p, pr.mc)) != NPOS) { int x1 = x0 + d.colToVis(l, (int)p) * cw; rr(SDL_Rect{x1, y + 1, d.colToVis(l, (int)(p + pr.f[0].size())) * cw + x0 - x1, lh - 2}, T.c[C_MATCH], 3); p += std::max<size_t>(1, pr.f[0].size()); } }
        if (st.guides) {
            size_t f = s.find_first_not_of(U" \t"); int ind = f == NPOS ? lastInd : d.colToVis(l, (int)f); if (f != NPOS) lastInd = ind;
            for (int g = 0; g < ind; g += TABW) fill(x0 + g * cw, y, 1, lh, gcol);
        }
        d.hl(l, tk); int vis = 0;
        for (size_t c = 0; c < s.size(); c++) {
            char32_t ch = s[c];
            if (ch == '\t') { if (st.whitespace) glyph(0xBB, x0 + vis * cw, y, wcol); vis += TABW - vis % TABW; continue; }
            int px = x0 + vis * cw; if (px > code.x + code.w) break;
            if (px + cw >= code.x) { if (ch != ' ') glyph(ch, px, y, T.c[(int)C_TOK0 + tk[c]]); else if (st.whitespace) glyph(0xB7, px, y, wcol); }
            vis++;
        }
    }
    Pos ma, mb;
    if (st.bracket && matchBracketCached(ma, mb)) for (Pos p : {ma, mb}) { SDL_Rect r{x0 + d.colToVis(p.l, p.c) * cw, ed.y + p.l * lh - (int)d.vsy, cw, lh}; setc(withA(T.c[C_ACCENT], 0.9f)); SDL_RenderDrawRect(ren, &r); fill(r, withA(T.c[C_ACCENT], 0.18f)); }
    float ca = pr.mode == P_NONE && ov == OV_NONE ? caretAlpha() : 0.f;
    if (ca > 0.01f) {
        int sx = code.x + cw / 2 + (int)carX.v - (int)d.vsx, sy = ed.y + (int)carY.v - (int)d.vsy; SDL_Color cc = withA(T.c[C_CURSOR], ca);
        if (st.caret == 0) fill(sx, sy, std::max(2, (int)uiScale * 2), lh, cc);
        else if (st.caret == 1) fill(sx, sy, cw, lh, withA(cc, 0.5f)); else fill(sx, sy + lh - std::max(2, (int)uiScale * 2), cw, std::max(2, (int)uiScale * 2), cc);
    }
    unclip();
    if (st.lineNums) {
        SDL_Rect g{ed.x, ed.y, gutterW, ed.h}; clip(g); fill(g, pane(T.c[C_GUTTER]));
        for (int l = first, y = y0; l < n && y < ed.y + ed.h; l++, y += lh) {
            std::string num = std::to_string(l + 1);
            text(num, gutterW - cw * 2 - (int)num.size() * cw + cw, y, l == d.cur.l ? T.c[C_FG] : T.c[C_GUTTERFG]);
        }
        unclip(); fill(gutterW - 1, ed.y, 1, ed.h, withA(T.c[C_FG], 0.08f));
    }
    if (st.scrollbar && sbA.v > 0.02f) {
        float total = (float)(n * lh), maxY = std::max(1.f, total - ed.h + lh); int th = std::max(lh * 2, (int)(ed.h * std::min(1.f, ed.h / total))), tw = std::max(4, (int)(4 * uiScale));
        int ty = ed.y + (int)((ed.h - th) * std::min(1.f, d.vsy / maxY));
        if (total > ed.h) rr(SDL_Rect{ed.x + ed.w - tw - 3, ty, tw, th}, withA(T.c[C_FG], 0.35f * sbA.v), tw / 2);
    }
    drawPopup(d);
}
void App::drawPopup(Doc& d) {
    if (popA.v < 0.02f || comp.list.empty()) return;
    float pa = popA.v; int rows = std::min((int)comp.list.size(), 8);
    if (comp.sel < comp.top) comp.top = comp.sel;
    if (comp.sel >= comp.top + rows) comp.top = comp.sel - rows + 1;
    int maxc = 0;
    for (int i = comp.top; i < comp.top + rows && i < (int)comp.list.size(); i++) { auto& it = comp.list[i]; maxc = std::max(maxc, 3 + (int)toU32(it.name).size() + (it.sig.empty() ? 0 : 2 + (int)toU32(it.sig).size())); }
    maxc = std::min(maxc, std::max(20, W / cw - 4)); int w = std::min(W, maxc * cw + pad * 2);
    int cx = edRect.x + gutterW + cw / 2 + d.colToVis(comp.start.l, comp.start.c) * cw - (int)d.vsx, cy = edRect.y + comp.start.l * lh - (int)d.vsy;
    int x = std::max(0, std::min(cx, W - w)), y = cy + lh + (int)((1 - pa) * 8);
    if (cy + lh + rows * lh > edRect.y + edRect.h) y = std::max(edRect.y, cy - rows * lh - (int)((1 - pa) * 8));
    comp.rect = {x, y, w, rows * lh};
    shadow(comp.rect, 8, (int)(110 * pa)); rr(comp.rect, withA(T.c[C_POPUP], pa), rad); clip(comp.rect);
    for (int i = 0; i < rows; i++) {
        auto& it = comp.list[comp.top + i]; int ry = y + i * lh;
        if (comp.top + i == comp.sel) fill(x, ry, w, lh, withA(T.c[C_SEL], pa));
        const char* kc = it.kind == K_FUNC ? "f" : it.kind == K_KW ? "k" : it.kind == K_TYPE ? "t" : it.kind == K_SNIP ? "s" : it.kind == K_CONST ? "c" : "w";
        SDL_Color kcol = it.kind == K_FUNC ? T.c[(int)C_TOK0 + T_FUNC] : it.kind == K_KW ? T.c[(int)C_TOK0 + T_KW] : it.kind == K_TYPE ? T.c[(int)C_TOK0 + T_TYPE] : it.kind == K_SNIP ? T.c[C_GOOD] : T.c[C_GUTTERFG];
        text(kc, x + pad, ry, withA(kcol, pa)); int nw = text(it.name, x + pad + cw * 2, ry, withA(T.c[C_FG], pa));
        if (!it.sig.empty()) text(it.sig, x + pad + cw * 4 + nw, ry, withA(T.c[C_GUTTERFG], pa));
    }
    unclip();
}

// ============================================================== output panel ===
void App::drawOutput() {
    if (outH < 3) return;
    SDL_Rect r = outRect; fill(r, pane(T.c[C_BAR])); fill(r.x, r.y, r.w, 1, withA(T.c[C_ACCENT], 0.7f));
    int hh = rowH; text(proc.running ? "Output - running..." : "Output", pad, r.y + pad, T.c[C_BARFG]);
    int right = W - pad; int bw = std::max(cw * 6, hh * 2);
    auto small = [&](const char* l, std::function<void()> f, bool acc = false) { right -= bw + pad / 2; button(SDL_Rect{right, r.y + pad / 2, bw, hh - pad}, l, std::move(f), acc); };
    small("Close", [this] { outA.to(0); });
    small("Clear", [this] { outLines.clear(); outPartial.clear(); outTarget = outScroll = 0; });
    if (proc.running) { small("Stop", [this] { stopRun(); }); if (proc.canSend()) small("Input", [this] { openPrompt(P_STDIN); }, true); }
    else small("Run", [this] { runDoc(false); }, true);
    outBody = {r.x, r.y + hh, r.w, r.h - hh}; clip(outBody);
    int total = (int)outLines.size() + (outPartial.empty() ? 0 : 1); float maxS = std::max(0.f, (float)(total * lh - outBody.h + pad));
    if (outStick) outTarget = maxS; outTarget = std::min(maxS, std::max(0.f, outTarget)); if (outTarget >= maxS - 2) outStick = true;
    if (!st.anim) outScroll = outTarget;
    int first = std::max(0, (int)(outScroll / lh)), y = outBody.y - (int)(outScroll - first * lh);
    for (int i = first; i < total && y < outBody.y + outBody.h; i++, y += lh) {
        bool part = i >= (int)outLines.size(); const std::string& s = part ? outPartial : outLines[i].text; int kind = part ? 0 : outLines[i].kind;
        SDL_Color col = kind == 1 || kind == 5 ? T.c[C_BAD] : kind == 2 ? T.c[C_WARN] : kind == 3 ? T.c[C_ACCENT] : kind == 4 ? T.c[C_GOOD] : T.c[C_FG];
        text(s, pad, y, col);
        std::string f; int ln, cl;
        if (!part && parseLoc(s, f, ln, cl)) { SDL_Rect lr{0, y, W, lh}; fill(0, y + lh - 1, std::min(W, textW(s) + pad), 1, withA(col, 0.4f)); std::string copy = s; addBtn(lr, "", [this, copy] { jumpToLoc(copy); }, -1); }
    }
    unclip();
}

// ================================================== toolbar / status / toast ===
void App::button(SDL_Rect r, const std::string& l, std::function<void()> f, bool accent) {
    rr(r, accent ? mix(T.c[C_ACCENT], T.c[C_BTN], 0.45f) : T.c[C_BTN], rad); text(l, r.x + (r.w - textW(l)) / 2, r.y + (r.h - lh) / 2, accent ? T.c[C_FG] : T.c[C_FG]);
    addBtn(r, l, std::move(f), 0);
}
void App::drawToolbar() {
    if (tbH <= 0) return; int y0 = statRect.y - tbH; fill(0, y0, W, tbH, pane(T.c[C_BAR])); int x = 0, row = 0;
    for (auto& t : tbItems) { if (x + t.w > W && x > 0) { row++; x = 0; } button(SDL_Rect{x + 2, y0 + row * tbBh + 2, t.w - 4, tbBh - 4}, t.l, t.f); x += t.w; }
}
void App::drawStatus() {
    Doc& d = doc(); fill(statRect, pane(T.c[C_BAR])); int y = statRect.y;
    std::string left = d.name() + (d.modified ? "  *" : ""); text(left, pad, y + pad / 2, T.c[C_BARFG]);
    int right = W - pad; auto chip = [&](const std::string& l, std::function<void()> f, bool on) {
        int w = textW(l) + pad * 2; right -= w; SDL_Rect r{right, y + 2, w, statH - 4}; rr(r, on ? mix(T.c[C_ACCENT], T.c[C_BTN], 0.5f) : T.c[C_BTN], rad); text(l, r.x + pad, y + pad / 2, on ? T.c[C_FG] : T.c[C_ACCENT]); addBtn(r, l, std::move(f), 0); right -= pad / 2; };
    chip(LANGS[d.lang].name, [this] { languageMenu(); }, false);
    chip("Out", [this] { outA.to(outA.t > 0.5f ? 0.f : 1.f); }, outA.t > 0.5f);
    chip("Kbd", [this] { kbWanted = !kbWanted; if (kbWanted) kbA.to(1); else kbA.to(0); follow = true; }, kbWanted);
    std::string pos = "Ln " + std::to_string(d.cur.l + 1) + ", Col " + std::to_string(d.colToVis(d.cur.l, d.cur.c) + 1);
    if (d.hasSel()) { Pos a, b; d.selRange(a, b); size_t cnt = 0; if (a.l == b.l) cnt = (size_t)(b.c - a.c); else { cnt = (size_t)(d.lines[a.l].size() - a.c) + (size_t)b.c; for (int l = a.l + 1; l < b.l; l++) cnt += d.lines[l].size() + 1; cnt++; } pos += " (" + std::to_string(cnt) + " sel)"; }
    int pw = textW(pos); text(pos, right - pw - pad, y + pad / 2, T.c[C_BARFG]);
}
void App::drawToast() {
    Uint32 now = SDL_GetTicks(); bool on = !msg.empty() && now - msgTick < 2800; toastA.to(on ? 1.f : 0.f);
    if (toastA.v < 0.02f) return;
    float a = toastA.v; int w = std::min(W - pad * 2, textW(msg) + pad * 4), h = lh + pad; int bottom = statRect.y - (tbH + outH) - pad;
    if (kbH > 0) bottom = statRect.y - pad;
    SDL_Rect r{(W - w) / 2, bottom - h - (int)((1 - a) * 12), w, h}; shadow(r, 8, (int)(90 * a)); rr(r, withA(mix(T.c[C_POPUP], T.c[C_ACCENT], 0.18f), a), h / 2);
    clip(r); text(msg, r.x + (w - textW(msg)) / 2, r.y + pad / 2, withA(T.c[C_FG], a)); unclip();
}
void App::drawRipples() {
    if (!st.anim) { ripples.clear(); return; }
    Uint32 now = SDL_GetTicks();
    for (size_t i = 0; i < ripples.size();) {
        float t = (now - ripples[i].t0) / 380.f; if (t >= 1.f) { ripples.erase(ripples.begin() + (long)i); continue; }
        auto& r = ripples[i]; clip(r.clip); int mx = std::max(r.clip.w, r.clip.h); float e = 1 - (1 - t) * (1 - t);
        circle(r.x, r.y, std::max(2, (int)(mx * 0.7f * e)), withA(T.c[C_FG], 0.16f * (1 - t))); unclip(); i++;
    }
}

// ================================================================= keyboard ====
void App::drawKeyboard() {
    keys.clear(); if (kbH < 2) return;
    fill(kbRect, T.c[C_BAR]); fill(kbRect.x, kbRect.y, kbRect.w, 1, withA(T.c[C_ACCENT], 0.5f));
    clip(kbRect);
    auto layer = kbLayout(kbLayer); int y = kbRect.y + kbGap, gap = kbGap, idx = 0;
    SDL_Color keyc = T.c[C_BTN], spec = mix(T.c[C_BTN], T.c[C_BAR], 0.55f), txt = T.c[C_FG];
    auto place = [&](std::vector<VKey>& row, int ry, bool accessory) {
        float sum = 0; int cnt = 0; for (auto& k : row) { sum += k.w; cnt++; }
        float avail = (float)(W - 2 * gap - (cnt - 1) * gap), x = (float)gap;
        for (auto& k : row) {
            int kw = (int)(avail * k.w / sum); SDL_Rect r{(int)x, ry, kw, kbKeyH}; x += kw + gap;
            if (k.code == KC_GAP) continue;
            int ch = 0; std::string lab = k.lab;
            if (k.code > 0) { ch = k.code; if (kbLayer == 0 && kbShift && !k.alt.empty()) { ch = (unsigned char)k.alt[0]; lab = k.alt; } }
            bool isSpec = (k.code < 0 && k.code != KC_SPACE) || (accessory && k.code > 0);
            float pa = keyAnim.count(idx) ? keyAnim[idx] : 0.f;
            SDL_Rect dr{r.x + (int)(pa * 2), r.y + (int)(pa * 2), r.w - (int)(pa * 4), r.h - (int)(pa * 4)};
            SDL_Color bg = k.code == KC_SHIFT && kbShift ? T.c[C_ACCENT] : (isSpec ? spec : keyc);
            if (k.code == KC_ENTER) bg = mix(T.c[C_ACCENT], T.c[C_BTN], 0.4f);
            rr(dr, mix(bg, T.c[C_ACCENT], pa * 0.6f), std::max(3, rad)); 
            if (k.code == KC_SPACE) { std::string nm = LANGS[doc().lang].name; text(nm, r.x + (r.w - textW(nm)) / 2, r.y + (r.h - lh) / 2, withA(txt, 0.35f)); }
            else if (k.code <= KC_SUG + 1 && k.code > KC_SUG - 50) { text(lab, r.x + pad, r.y + (r.h - lh) / 2, txt); }
            else if ((k.code < 0 && lab.empty()) || k.code == KC_SHIFT || k.code == KC_BKSP || k.code == KC_ENTER || k.code == KC_HIDE || (k.code <= KC_LEFT && k.code >= KC_DOWN)) icon(k.code, r, txt);
            else text(lab, r.x + (r.w - textW(lab)) / 2, r.y + (r.h - lh) / 2, txt);
            keys.push_back({r, k.code, ch, lab}); idx++;
        }
    };
    // accessory row: hide + (suggestions | code keys)
    std::vector<VKey> acc; acc.push_back({"", "", KC_HIDE, 1.f});
    if (comp.open && !comp.list.empty()) {
        int n = std::min<int>(5, (int)comp.list.size()); for (int i = 0; i < n; i++) acc.push_back({comp.list[i].name, comp.list[i].name, KC_SUG - i, 2.f});
    } else {
        acc.push_back({"Tab", "Tab", KC_TAB, 1.2f}); acc.push_back({"", "", KC_LEFT, 1.f}); acc.push_back({"", "", KC_RIGHT, 1.f}); acc.push_back({"", "", KC_UP, 1.f}); acc.push_back({"", "", KC_DOWN, 1.f});
        for (char c : std::string("{};")) acc.push_back({std::string(1, c), std::string(1, c), (int)c, 1.f});
        acc.push_back({"Cmp", "Cmp", KC_CMP, 1.2f});
    }
    place(acc, y, true); y += kbKeyH + gap;
    for (auto& row : layer) { place(row, y, false); y += kbKeyH + gap; }
    for (auto& kv : kbHeld) {   // key preview bubble
        if (kv.second < 0 || kv.second >= (int)keys.size()) continue; const KeyRect& k = keys[kv.second];
        if (k.ch > 32 && kbLayer >= 0) { SDL_Rect b{k.r.x - k.r.w / 4, k.r.y - (int)(k.r.h * 1.15f), k.r.w + k.r.w / 2, (int)(k.r.h * 1.05f)}; if (b.y < 0) b.y = 0; unclip(); shadow(b, 6, 90); rr(b, T.c[C_POPUP], rad + 2); std::string s = toU8(U32(1, (char32_t)k.ch)); text(s, b.x + (b.w - textW(s)) / 2, b.y + (b.h - lh) / 2, T.c[C_FG]); clip(kbRect); }
    }
    unclip();
}

// ================================================================ prompt bar ===
void App::drawPromptBar() {
    if (barA.v < 0.02f) return; int rows = pr.mode == P_REPLACE ? 2 : 1;
    int by = ov == OV_NONE ? tabH : 0, bh = (int)(rowH * rows * std::min(1.f, barA.v / std::max(1, rows))); if (bh < 2) return;
    SDL_Rect bar{0, by, W, bh}; shadow(bar, 6, 70); fill(bar, T.c[C_POPUP]); fill(0, by + bh - 1, W, 1, T.c[C_ACCENT]); clip(bar);
    const char* lab = pr.mode == P_FIND || pr.mode == P_REPLACE ? "Find" : pr.mode == P_GOTO ? "Line[:col]" : pr.mode == P_STDIN ? "stdin" : pr.label.c_str();
    int lw = std::max(cw * 8, textW(lab) + cw); text(lab, pad, by + pad, T.c[C_BARFG]);
    int right = W - pad, bw = std::max(cw * 4, rowH);
    auto small = [&](int row, const char* l, std::function<void()> f, bool on = false, int w = 0) {
        if (!w) w = bw; right -= w + pad / 2; SDL_Rect r{right, by + row * rowH + pad / 2, w, rowH - pad}; rr(r, on ? T.c[C_ACCENT] : T.c[C_BTN], rad);
        text(l, r.x + (w - textW(l)) / 2, r.y + (r.h - lh) / 2, T.c[C_FG]); addBtn(r, l, std::move(f), 0); };
    if (pr.mode == P_FIND || pr.mode == P_REPLACE) { small(0, "x", [this] { closePrompt(); }); small(0, "Aa", [this] { pr.mc = !pr.mc; }, pr.mc); small(0, ">", [this] { findStep(true); }); small(0, "<", [this] { findStep(false); }); }
    else { small(0, "x", [this] { closePrompt(); }); small(0, "OK", [this] { promptEnter(false, false); }); }
    for (int row = 0; row < rows; row++) {
        int fx = lw + pad, rr_ = row == 0 ? right : W - pad;
        if (row == 1) {
            text("With", pad, by + rowH + pad, T.c[C_BARFG]);
            int w2 = std::max(cw * 6, rowH * 2); rr_ -= w2 + pad / 2; SDL_Rect a{rr_, by + rowH + pad / 2, w2, rowH - pad}; rr(a, T.c[C_BTN], rad); text("Repl", a.x + (w2 - textW("Repl")) / 2, a.y + (a.h - lh) / 2, T.c[C_FG]); addBtn(a, "", [this] { pr.active = 1; promptEnter(false, false); }, 0);
            int w3 = std::max(cw * 5, rowH * 2); rr_ -= w3 + pad / 2; SDL_Rect b{rr_, by + rowH + pad / 2, w3, rowH - pad}; rr(b, T.c[C_BTN], rad); text("All", b.x + (w3 - textW("All")) / 2, b.y + (b.h - lh) / 2, T.c[C_FG]); addBtn(b, "", [this] { pr.active = 1; promptEnter(false, true); }, 0);
        }
        SDL_Rect f{fx, by + row * rowH + pad / 2, std::max(cw * 4, rr_ - fx - pad), rowH - pad}; rr(f, T.c[C_BG], rad);
        bool act = pr.mode == P_REPLACE ? pr.active == row : row == 0; setc(act ? T.c[C_ACCENT] : T.c[C_BTN]); SDL_RenderDrawRect(ren, &f);
        int maxc = std::max(1, (f.w - pad * 2) / cw); const U32& s = pr.f[row]; U32 shown = (int)s.size() > maxc ? s.substr(s.size() - maxc) : s;
        clip(f); textU(shown, f.x + pad, f.y + (f.h - lh) / 2, T.c[C_FG]);
        if (act) fill(f.x + pad + (int)shown.size() * cw, f.y + 3, 2, f.h - 6, withA(T.c[C_CURSOR], caretAlpha()));
        clip(bar); int rw = row; addBtn(f, "", [this, rw] { pr.active = rw; startTextInput(); }, -2);
    }
    unclip();
}

// ======================================================================= draw ===
void App::draw() {
    layout(); btns.clear(); sliders.clear();
    drawBackground(); drawTabs(); drawEditor(); drawOutput(); drawToolbar(); drawStatus();
    if (ov != OV_NONE) drawOverlay();
    drawKeyboard(); drawPromptBar(); drawToast(); drawRipples();
    Doc& d = doc(); std::string title = "FlowyEditor - " + d.name() + (d.modified ? " *" : "");
    if (title != lastTitle) { SDL_SetWindowTitle(win, title.c_str()); lastTitle = title; }
    SDL_RenderPresent(ren);
}
