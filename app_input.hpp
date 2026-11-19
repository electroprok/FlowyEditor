#pragma once
static const int64_t MOUSE_PID = -1000000;

// ================================================================== scrolling ====
Pos App::posAt(int x, int y) {
    Doc& d = doc(); int l = (int)std::floor((y - edRect.y + d.vsy) / lh); l = std::max(0, std::min(l, (int)d.lines.size() - 1));
    double vc = (x - edRect.x - gutterW - cw / 2 + d.vsx) / (double)cw; if (vc < 0) vc = 0;
    return {l, d.visToCol(l, vc)};
}
void App::ensureVisible() {
    Doc& d = doc(); float cy = (float)(d.cur.l * lh);
    if (cy < d.sy) d.sy = cy; else if (cy > d.sy + edRect.h - lh) d.sy = cy - (edRect.h - lh);
    int codeW = edRect.w - gutterW - cw; float cx = (float)(d.colToVis(d.cur.l, d.cur.c) * cw);
    if (cx < d.sx) d.sx = std::max(0.f, cx - cw * 2); else if (cx + cw * 2 > d.sx + codeW) d.sx = cx + cw * 2 - codeW;
}
void App::clampScroll() {
    Doc& d = doc(); float maxY = std::max(0.f, (float)(((int)d.lines.size() - 1) * lh)); d.sy = std::min(maxY, std::max(0.f, d.sy));
    int first = std::max(0, (int)(d.sy / lh)), last = std::min((int)d.lines.size() - 1, first + edRect.h / lh + 1), mx = 0;
    for (int l = first; l <= last; l++) mx = std::max(mx, d.colToVis(l, (int)d.lines[l].size()));
    float maxX = std::max(0.f, (float)((mx + 2) * cw - (edRect.w - gutterW))); d.sx = std::min(maxX, std::max(0.f, d.sx));
}
int App::scrollTargetAt(int x, int y) {
    if (ov != OV_NONE) return inRect(ovList, x, y) ? 2 : -1;
    if (outH > 4 && inRect(outBody, x, y)) return 1;
    return inRect(edRect, x, y) ? 0 : -1;
}
void App::scrollBy(int tg, float dx, float dy) {
    lastScroll = SDL_GetTicks();
    if (tg == 0) { Doc& d = doc(); d.sy += dy; d.sx += dx; }
    else if (tg == 1) { outTarget += dy; outStick = false; }
    else if (tg == 2) { ovTarget += dy; if (ovTarget < 0) ovTarget = 0; }
}

// =================================================================== sliders ====
int App::beginSlider(int x, int y) {
    for (int i = (int)sliders.size() - 1; i >= 0; i--) if (inRect(sliders[i].r, x, y)) { activeSlider = i; moveSlider(i, x); return i; }
    return -1;
}
void App::moveSlider(int idx, int x) {
    if (idx < 0 || idx >= (int)sliders.size()) return; Slider& s = sliders[idx];
    float lo = (float)(s.r.x + lh / 2), w = (float)std::max(1, s.r.w - lh); float f = std::min(1.f, std::max(0.f, ((float)x - lo) / w));
    if (s.set) s.set(s.mn + (s.mx - s.mn) * f);
}

// ================================================================== keyboard ====
void App::kbDown(int x, int y, int64_t pid) {
    for (int i = 0; i < (int)keys.size(); i++) if (inRect(keys[i].r, x, y)) {
        kbHeld[pid] = i; keyAnim[i] = 1.f; rpKey = keys[i]; rpCode = keys[i].code; rpPid = pid; rpT0 = rpLast = SDL_GetTicks(); kbPress(keys[i]); return;
    }
}
void App::kbUp(int64_t pid) { kbHeld.erase(pid); if (rpPid == pid) rpCode = 0; }
void App::kbRepeat() {
    if (rpCode == 0 || !kbHeld.count(rpPid)) return;
    bool rep = rpCode == KC_BKSP || rpCode == KC_SPACE || (rpCode <= KC_LEFT && rpCode >= KC_DOWN);
    Uint32 now = SDL_GetTicks(); if (rep && now - rpT0 > 420 && now - rpLast > 45) { rpLast = now; kbPress(rpKey); }
}
void App::sendKey(SDL_Keycode k) { SDL_Keysym ks{}; ks.sym = k; ks.mod = 0; onKey(ks); }
void App::kbPress(const KeyRect& k) {
    int c = k.code; caretT = SDL_GetTicks();
    if (c > 0) { inputText(U32(1, (char32_t)k.ch)); if (kbShift == 1 && kbLayer == 0) kbShift = 0; return; }
    switch (c) {
        case KC_SHIFT: { Uint32 now = SDL_GetTicks(); if (kbShift == 1 && now - kbLastShift < 350) kbShift = 2; else kbShift = kbShift == 0 ? 1 : 0; kbLastShift = now; break; }
        case KC_BKSP: sendKey(SDLK_BACKSPACE); break;
        case KC_ENTER: comp.open = false; sendKey(SDLK_RETURN); break;
        case KC_SPACE: inputText(U" "); break;
        case KC_TAB: comp.open = false; sendKey(SDLK_TAB); break;
        case KC_L123: kbLayer = 1; break; case KC_LABC: kbLayer = 0; break; case KC_LSYM: kbLayer = 2; break;
        case KC_LEFT: sendKey(SDLK_LEFT); break; case KC_RIGHT: sendKey(SDLK_RIGHT); break; case KC_UP: sendKey(SDLK_UP); break; case KC_DOWN: sendKey(SDLK_DOWN); break;
        case KC_HIDE: kbWanted = false; kbA.to(0); follow = true; break;
        case KC_CMP: refreshComp(true); break;
        default: if (c <= KC_SUG && c > KC_SUG - 10) { comp.sel = KC_SUG - c; acceptComp(); } break;
    }
}

// ===================================================================== text ====
void App::inputText(const U32& u) {
    quitWarn = false; caretT = SDL_GetTicks();
    if (pr.mode != P_NONE) { promptText(u); return; }
    if (ov != OV_NONE) {
        if (ovClosing) return;
        if (ov == OV_MENU) { for (char32_t c : u) if (c >= 32) palQuery += c; palSel = 0; ovTarget = 0; }
        else if (ov == OV_PICKER && pk.mode == PK_SAVE) for (char32_t c : u) if (c >= 32 && c != '/' && c != '\\') pk.name += c;
        return;
    }
    Doc& d = doc(); d.typeText(u);
    bool word = !u.empty() && isIdC(u.back()); if (word) afterEdit(true); else { comp.open = false; follow = true; }
}
void App::onText(const char* t) {
    SDL_Keymod m = SDL_GetModState(); bool ctrl = (m & (KMOD_CTRL | KMOD_GUI)) && !(m & KMOD_ALT); if (ctrl) return;
    inputText(toU32(t));
}

// ====================================================================== keys ====
void App::onKey(SDL_Keysym ks) {
    SDL_Keymod mod = (SDL_Keymod)ks.mod; bool ctrl = (mod & (KMOD_CTRL | KMOD_GUI)) != 0, shift = (mod & KMOD_SHIFT) != 0, alt = (mod & KMOD_ALT) != 0;
    SDL_Keycode k = ks.sym; caretT = SDL_GetTicks();
    if (!(ctrl && k == SDLK_q)) quitWarn = false; if (!(ctrl && k == SDLK_w)) closeWarn = false;
    if (pr.mode != P_NONE && promptKey(ks, ctrl, shift)) return;
    if (ov != OV_NONE) { if (!ovClosing) overlayKey(k, ctrl, shift); return; }
    if (k == SDLK_AC_BACK && kbA.t > 0.5f) { kbWanted = false; kbA.to(0); return; }
    Doc& d = doc();
    if (comp.open) {
        switch (k) {
            case SDLK_UP: comp.sel = std::max(0, comp.sel - 1); return;
            case SDLK_DOWN: comp.sel = std::min((int)comp.list.size() - 1, comp.sel + 1); return;
            case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_TAB: acceptComp(); return;
            case SDLK_ESCAPE: case SDLK_AC_BACK: comp.open = false; return;
            default: break;
        }
    }
    if (ctrl) {
        switch (k) {
            case SDLK_s: if (shift) saveAsDialog(); else saveDoc(); return;
            case SDLK_o: openDialog(); return;
            case SDLK_n: newDoc(); return;
            case SDLK_w: closeDoc(); return;
            case SDLK_q: { bool un = false; for (auto& x : docs) un |= x.modified; if (un && !quitWarn) { quitWarn = true; say("Unsaved changes! Ctrl+Q again to quit."); } else running = false; return; }
            case SDLK_p: if (shift) mainMenu(); return;
            case SDLK_COMMA: openSettings(0); return;
            case SDLK_f: openPrompt(P_FIND); return;
            case SDLK_h: openPrompt(P_REPLACE); return;
            case SDLK_g: openPrompt(P_GOTO); return;
            case SDLK_z: if (shift) d.doRedo(); else d.doUndo(); afterEdit(false); return;
            case SDLK_y: d.doRedo(); afterEdit(false); return;
            case SDLK_a: d.selectAll(); return;
            case SDLK_c: copySel(false); return;
            case SDLK_x: copySel(true); afterEdit(false); return;
            case SDLK_v: paste(); return;
            case SDLK_d: d.duplicate(); afterEdit(false); return;
            case SDLK_k: if (shift) { d.deleteLines(); afterEdit(false); } return;
            case SDLK_l: d.selectLine(d.cur.l); return;
            case SDLK_SLASH: d.toggleComment(); afterEdit(false); return;
            case SDLK_EQUALS: case SDLK_PLUS: case SDLK_KP_PLUS: st.fontScale = std::min(3.f, st.fontScale * 1.1f); loadFont(); settingsChanged(); follow = true; return;
            case SDLK_MINUS: case SDLK_KP_MINUS: st.fontScale = std::max(0.5f, st.fontScale / 1.1f); loadFont(); settingsChanged(); follow = true; return;
            case SDLK_0: st.fontScale = 1.f; loadFont(); settingsChanged(); follow = true; return;
            case SDLK_t: { int i = (findTheme(st.theme) + 1) % (int)g_themes.size(); st.theme = g_themes[i].name; st.ov.clear(); applyTheme(); settingsChanged(); say("Theme: " + st.theme); return; }
            case SDLK_SPACE: refreshComp(true); return;
            case SDLK_LEFTBRACKET: d.indentSel(true); afterEdit(false); return;
            case SDLK_RIGHTBRACKET: d.indentSel(false); afterEdit(false); return;
            case SDLK_RETURN: case SDLK_KP_ENTER: d.end(false); d.newline(); afterEdit(false); return;
            case SDLK_F5: runDoc(true); return;
            case SDLK_TAB: if (docs.size() > 1) { ci = (ci + (shift ? (int)docs.size() - 1 : 1)) % (int)docs.size(); follow = true; } return;
            case SDLK_PAGEUP: if (docs.size() > 1) { ci = (ci + (int)docs.size() - 1) % (int)docs.size(); follow = true; } return;
            case SDLK_PAGEDOWN: if (docs.size() > 1) { ci = (ci + 1) % (int)docs.size(); follow = true; } return;
            default: break;
        }
    }
    int vis = std::max(1, edRect.h / lh - 1);
    switch (k) {
        case SDLK_LEFT: d.left(shift, ctrl); break;
        case SDLK_RIGHT: d.right(shift, ctrl); break;
        case SDLK_UP: if (alt) d.moveLines(-1); else d.vert(-1, shift); break;
        case SDLK_DOWN: if (alt) d.moveLines(1); else d.vert(1, shift); break;
        case SDLK_HOME: if (ctrl) d.setCur({0, 0}, shift); else d.home(shift); break;
        case SDLK_END: if (ctrl) d.setCur({(int)d.lines.size() - 1, (int)d.lines.back().size()}, shift); else d.end(shift); break;
        case SDLK_PAGEUP: d.vert(-vis, shift); d.sy -= vis * lh; break;
        case SDLK_PAGEDOWN: d.vert(vis, shift); d.sy += vis * lh; break;
        case SDLK_BACKSPACE: d.backspace(ctrl); afterEdit(false); return;
        case SDLK_DELETE: d.del(ctrl); afterEdit(false); return;
        case SDLK_RETURN: case SDLK_KP_ENTER: d.newline(); comp.open = false; afterEdit(false); return;
        case SDLK_TAB:
            if (shift) d.indentSel(true); else if (d.hasSel() && d.cur.l != d.anc.l) d.indentSel(false);
            else d.insertText(U32(TABW - d.colToVis(d.cur.l, d.cur.c) % TABW, U' '));
            afterEdit(false); return;
        case SDLK_ESCAPE: case SDLK_AC_BACK: d.anc = d.cur; comp.open = false; break;
        case SDLK_F1: mainMenu(); return;
        case SDLK_F2: st.toolbar = !st.toolbar; settingsChanged(); return;
        case SDLK_F3: if (pr.f[0].empty()) openPrompt(P_FIND); else findStep(!shift); return;
        case SDLK_F4: kbWanted = !kbWanted; kbA.to(kbWanted ? 1.f : 0.f); follow = true; return;
        case SDLK_F5: if (shift) stopRun(); else runDoc(false); return;
        case SDLK_F6: languageMenu(); return;
        case SDLK_F11: { Uint32 f = SDL_GetWindowFlags(win); SDL_SetWindowFullscreen(win, (f & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP); return; }
        default: return;
    }
    follow = true; validateComp();
}

// ======================================================================= tap ====
bool App::tap(int x, int y, int clicks, bool shift) {   // returns true if the editor text area was hit (drag allowed)
    if (ovClosing) return false;
    for (int i = (int)btns.size() - 1; i >= 0; i--) if (inRect(btns[i].r, x, y)) {
        Btn b = btns[i]; caretT = SDL_GetTicks();
        if (b.style != -2 && st.anim) ripples.push_back({x, y, SDL_GetTicks(), b.r});
        if (b.fn) b.fn(); return false;
    }
    if (ov != OV_NONE) return false;
    if (comp.open && popA.v > 0.5f && inRect(comp.rect, x, y)) { comp.sel = comp.top + (y - comp.rect.y) / lh; acceptComp(); return false; }
    if (inRect(edRect, x, y)) {
        Doc& d = doc(); Pos p = posAt(x, y); comp.open = false; d.wantCol = -1; caretT = SDL_GetTicks();
        if (clicks >= 3) d.selectLine(p.l); else if (clicks == 2) d.selectWord(p); else d.setCur(p, shift);
        follow = true; if (mobile && st.kbMode == 1) { kbWanted = true; kbA.to(1); } startTextInput();
        return true;
    }
    return false;
}

// =================================================================== touch ====
void App::onFinger(const SDL_Event& e) {
    float x = e.tfinger.x * W, y = e.tfinger.y * H; Uint32 now = SDL_GetTicks(); float slop = 10.f * uiScale; SDL_FingerID id = e.tfinger.fingerId;
    if (e.type == SDL_FINGERDOWN) {
        if (hitKeyboard((int)x, (int)y)) { kbDown((int)x, (int)y, id); return; }
        if (beginSlider((int)x, (int)y) >= 0) { sliderFinger = id; return; }
        fingers[id] = {x, y};
        if (fingers.size() == 1) { tDown = true; tMoved = tLong = tPinch = false; tx0 = x; ty0 = y; tDownTick = now; velX = velY = 0; lastMoveTick = now; scrollTgt = scrollTargetAt((int)x, (int)y); }
        else if (fingers.size() == 2) { tPinch = true; auto it = fingers.begin(); Fin a = it->second, b = (++it)->second; pinchD = std::hypot(a.x - b.x, a.y - b.y); }
    } else if (e.type == SDL_FINGERMOTION) {
        if (kbHeld.count(id)) return;
        if (id == sliderFinger) { moveSlider(activeSlider, (int)x); return; }
        if (!fingers.count(id)) return; fingers[id] = {x, y};
        if (tPinch && fingers.size() >= 2) {
            if (ov != OV_NONE) return;
            auto it = fingers.begin(); Fin a = it->second, b = (++it)->second; float nd = std::hypot(a.x - b.x, a.y - b.y);
            if (pinchD > 1) { float r = nd / pinchD; if (r > 1.05f || r < 0.95f) { st.fontScale = std::min(3.f, std::max(0.5f, st.fontScale * r)); loadFont(); settingsChanged(); follow = true; pinchD = nd; } }
            return;
        }
        if (fingers.size() != 1) return;
        if (!tMoved && std::hypot(x - tx0, y - ty0) > slop) tMoved = true;
        if (tLong) { doc().setCur(posAt((int)x, (int)y), true); follow = true; }
        else if (tMoved && scrollTgt >= 0) {
            float dx = e.tfinger.dx * W, dy = e.tfinger.dy * H; scrollBy(scrollTgt, -dx, -dy);
            Doc& d = doc(); if (scrollTgt == 0) { d.vsy = d.sy; d.vsx = d.sx; } else if (scrollTgt == 1) outScroll = outTarget; else ovScroll = ovTarget;
            Uint32 dt = std::max<Uint32>(1, now - lastMoveTick); velY = -dy / dt * 16.f; velX = -dx / dt * 16.f; lastMoveTick = now;
        }
    } else if (e.type == SDL_FINGERUP) {
        if (kbHeld.count(id)) { kbUp(id); return; }
        if (id == sliderFinger) { sliderFinger = -1; activeSlider = -1; return; }
        fingers.erase(id);
        if (tPinch) { if (fingers.empty()) tPinch = false; tDown = false; return; }
        tDown = false;
        if (!tMoved && !tLong) {
            int clicks = (now - lastTapTick < 350 && std::hypot(x - lastTapX, y - lastTapY) < 30 * uiScale) ? 2 : 1;
            lastTapTick = now; lastTapX = x; lastTapY = y; tap((int)x, (int)y, clicks, false);
        } else if (tMoved && now - lastMoveTick > 80) velX = velY = 0;
    }
}

// ===================================================================== tick ====
void App::tick(float dt) {
    Uint32 now = SDL_GetTicks(); bool mv = false; float s = st.animSpeed;
    float k = st.anim ? 1.f - std::exp(-dt * 14.f * s) : 1.f, kf = st.anim ? 1.f - std::exp(-dt * 32.f * s) : 1.f;
    drainProc();
    { std::string p; SDL_Surface* sf; int n = 0;
      while (n++ < 4 && loader.pop(p, sf)) { Thumb& t = thumbs[p]; t.pending = false; if (sf) { t.t = SDL_CreateTextureFromSurface(ren, sf); t.w = sf->w; t.h = sf->h; SDL_FreeSurface(sf); } else t.failed = true; } }
    if (bgReloadAt && now >= bgReloadAt) { bgReloadAt = 0; loadBackground(); }
    if (settingsDirty && now - settingsDirtyAt > 1500) saveSettings();
    Doc& d = doc();
    if (lastCi != ci) { lastCi = ci; d.vsy = d.sy; d.vsx = d.sx; carX.snap((float)(d.colToVis(d.cur.l, d.cur.c) * cw)); carY.snap((float)(d.cur.l * lh)); clY.snap(carY.v); follow = true; }
    carX.to((float)(d.colToVis(d.cur.l, d.cur.c) * cw)); carY.to((float)(d.cur.l * lh)); clY.to((float)(d.cur.l * lh));
    if (follow || kbA.v != kbA.t || outA.v != outA.t || barA.v != barA.t) { ensureVisible(); follow = false; }
    clampScroll();
    if (tDown && !tMoved && !tLong && !tPinch && now - tDownTick > 450 && scrollTgt == 0 && ov == OV_NONE) {
        tLong = true; Pos p = posAt((int)tx0, (int)ty0); d.selectWord(p); comp.open = false; follow = true;
    }
    if (!tDown && (std::fabs(velY) > 0.5f || std::fabs(velX) > 0.5f) && scrollTgt >= 0) { scrollBy(scrollTgt, velX, velY); velY *= 0.93f; velX *= 0.93f; mv = true; }
    if (!st.smooth || !st.anim) { d.vsy = d.sy; d.vsx = d.sx; }
    else {
        d.vsy += (d.sy - d.vsy) * kf; d.vsx += (d.sx - d.vsx) * kf;
        if (std::fabs(d.sy - d.vsy) < 0.4f) d.vsy = d.sy; if (std::fabs(d.sx - d.vsx) < 0.4f) d.vsx = d.sx; mv |= d.vsy != d.sy || d.vsx != d.sx;
    }
    outScroll += (outTarget - outScroll) * kf; if (std::fabs(outTarget - outScroll) < 0.5f) outScroll = outTarget; mv |= outScroll != outTarget;
    ovScroll += (ovTarget - ovScroll) * kf; if (std::fabs(ovTarget - ovScroll) < 0.5f) ovScroll = ovTarget; mv |= ovScroll != ovTarget;
    static float prevSy = 0; if (std::fabs(d.sy - prevSy) > 0.5f) { lastScroll = now; prevSy = d.sy; }
    sbA.to(now - lastScroll < 900 ? 1.f : 0.f); popA.to(comp.open && !kbVisible() ? 1.f : 0.f);
    mv |= kbA.step(k); mv |= outA.step(k); mv |= barA.step(k); mv |= ovA.step(k); mv |= toastA.step(k); mv |= sbA.step(k); mv |= popA.step(k);
    mv |= carX.step(kf); mv |= carY.step(kf); mv |= clY.step(kf); mv |= tabX.step(k); mv |= tabW2.step(k);
    if (ovClosing && ovA.v <= 0.01f) { ov = OV_NONE; ovClosing = false; follow = true; }
    for (auto& kv : keyAnim) if (kv.second > 0) { kv.second *= std::exp(-dt * 10.f); if (kv.second < 0.02f) kv.second = 0; mv = true; }
    kbRepeat();
    animating = mv || !ripples.empty();
}

// ==================================================================== events ====
void App::event(const SDL_Event& e) {
    switch (e.type) {
        case SDL_QUIT: {
            if (mobile) { autosaveAll(); running = false; break; }
            bool un = false; for (auto& x : docs) un |= x.modified;
            if (un && !quitWarn) { quitWarn = true; say("Unsaved changes! Close again to quit."); } else { if (settingsDirty) saveSettings(); running = false; }
            break; }
        case SDL_APP_WILLENTERBACKGROUND: case SDL_APP_TERMINATING: autosaveAll(); break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED) { if (calcScale()) loadFont(); follow = true; }
            break;
        case SDL_KEYDOWN: onKey(e.key.keysym); break;
        case SDL_TEXTINPUT: onText(e.text.text); break;
        case SDL_DROPFILE: openFile(e.drop.file); SDL_free(e.drop.file); break;
        case SDL_MOUSEBUTTONDOWN: {
            if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT) break;
            int x = (int)(e.button.x * pxX), y = (int)(e.button.y * pxY);
            if (hitKeyboard(x, y)) { kbDown(x, y, MOUSE_PID); break; }
            if (beginSlider(x, y) >= 0) { sliderFinger = MOUSE_PID; break; }
            dragging = tap(x, y, e.button.clicks, (SDL_GetModState() & KMOD_SHIFT) != 0); break; }
        case SDL_MOUSEBUTTONUP:
            if (e.button.which == SDL_TOUCH_MOUSEID) break;
            dragging = false; kbUp(MOUSE_PID); if (sliderFinger == MOUSE_PID) { sliderFinger = -1; activeSlider = -1; } break;
        case SDL_MOUSEMOTION:
            if (e.motion.which == SDL_TOUCH_MOUSEID) break;
            if (sliderFinger == MOUSE_PID) moveSlider(activeSlider, (int)(e.motion.x * pxX));
            else if (dragging && (e.motion.state & SDL_BUTTON_LMASK)) { doc().setCur(posAt((int)(e.motion.x * pxX), (int)(e.motion.y * pxY)), true); follow = true; }
            break;
        case SDL_MOUSEWHEEL: {
            if (e.wheel.which == SDL_TOUCH_MOUSEID) break; SDL_Keymod m = SDL_GetModState(); int mx, my; SDL_GetMouseState(&mx, &my); mx = (int)(mx * pxX); my = (int)(my * pxY);
            if ((m & (KMOD_CTRL | KMOD_GUI)) && ov == OV_NONE) { st.fontScale = std::min(3.f, std::max(0.5f, st.fontScale * (e.wheel.y > 0 ? 1.1f : 1.f / 1.1f))); loadFont(); settingsChanged(); follow = true; break; }
            int tg = scrollTargetAt(mx, my); if (tg < 0) break;
            if (m & KMOD_SHIFT) scrollBy(tg, -(float)(e.wheel.y + e.wheel.x) * cw * 4, 0); else scrollBy(tg, (float)e.wheel.x * cw * 4, -(float)e.wheel.y * lh * 3);
            break; }
        case SDL_FINGERDOWN: case SDL_FINGERMOTION: case SDL_FINGERUP: onFinger(e); break;
        default: break;
    }
}
void App::run() {
    Uint32 last = SDL_GetTicks(), lastDraw = 0;
    while (running) {
        SDL_Event e; bool busy = animating || tDown || std::fabs(velY) > 0.5f || std::fabs(velX) > 0.5f || proc.running; bool got = false;
        if (SDL_WaitEventTimeout(&e, busy ? 8 : 90)) { got = true; do { event(e); } while (SDL_PollEvent(&e)); }
        Uint32 now = SDL_GetTicks(); float dt = std::min(0.05f, std::max(0.001f, (now - last) / 1000.f)); last = now;
        tick(dt);
        if (got || busy || now - lastDraw > 80) { draw(); lastDraw = SDL_GetTicks(); }
    }
}
