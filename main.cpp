// ============================================================================
//  FlowyEditor - cross-platform code editor on SDL2 (+SDL2_ttf, optional SDL2_image)
//  Windows / Linux / macOS / Android / iOS.  Single translation unit, see src/.
// ============================================================================
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "src/lang.hpp"      // utf8, C/C++/Lua/NASM data + highlighters
#include "src/json.hpp"      // JSON parser/writer (settings, plugins, themes)
#include "src/lang_ext.hpp"  // generic data-driven highlighter, JSON language loader, built-in languages & themes
#include "src/doc.hpp"       // text buffer / editing operations
#include "src/gfx.hpp"       // themes, settings, image helpers (SDL_image)
#include "src/proc.hpp"      // run external commands (gcc, python, ...)
#include "src/app.hpp"       // App state
#include "src/app_draw.hpp"
#include "src/app_ops.hpp"
#include "src/app_overlay.hpp"
#include "src/app_input.hpp"

int main(int argc, char** argv) {
    App app;
    if (!app.init(argc, argv)) return 1;
    app.run();
    app.shutdown();
    return 0;
}
