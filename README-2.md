# FlowyEditor v2 (SDL2)

Редактор кода на C++17 + SDL2 + SDL2_ttf (+ SDL2_image для фото). Windows / Linux / macOS / Android / iOS.
Один translation unit: `main.cpp` подключает `src/*.hpp`.

## Сборка (ПК)
    # Linux: sudo apt install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev cmake g++   (GCC 9+)
    cmake -B build && cmake --build build
    ./build/FlowyEditor [файлы...]
Без SDL2_image собирается тоже, но фон поддерживает только BMP.
Положите моноширинный `font.ttf` рядом с `main.cpp` (копируется к exe), либо выберите шрифт в Settings.

## Android / iOS
Android: `android-project` из SDL2 + SDL2, SDL2_ttf, SDL2_image (+freetype/libpng/libjpeg из `external/`), `main.cpp` и `src/` в `app/jni/src`,
`font.ttf` в `assets/`, в манифесте `android:windowSoftInputMode="adjustResize"`, для выбора фото из галереи нужен READ_MEDIA_IMAGES (Android 13+) / READ_EXTERNAL_STORAGE.
iOS: Xcode-шаблон SDL2, + SDL2_ttf/SDL2_image, `font.ttf` в Copy Bundle Resources. Запуск программ на iOS невозможен (ограничение системы).
Встроенная клавиатура включена по умолчанию на мобильных (Settings > Keyboard & touch).

## Возможности
- Языки: C, C++, NASM, Lua + Python, JavaScript, TypeScript, Java, C#, Go, Rust, Kotlin, PHP, Bash, SQL, JSON, HTML/XML, CSS, Markdown, YAML, TOML/INI, Makefile; свои — через JSON-плагины.
- Автокомплит (функции с сигнатурами, ключевые слова, сниппеты, слова из файла), сниппеты с `${1:default}`.
- Запуск (F5): команда по языку, плейсхолдеры `{file} {dir} {name} {stem} {ext} {out} {args}`; вывод в нижней панели, клик по `file:line:col` открывает ошибку, stdin (POSIX), Stop (Shift+F5). Команды правятся в Settings > Run & build.
- Файловый менеджер внутри приложения: места (Home/Documents/...), Open/Save As/выбор папки/шрифта, выбор фото с миниатюрами и превью.
- Фон: любое фото (PNG/JPG/WebP/...), режимы Cover/Fit/Stretch/Tile/Center, прозрачность, затемнение, размытие, цветной оттенок, прозрачность панелей, медленный «дрейф».
- Кастомизация: 7 тем + редактор каждого из 29 цветов, шрифт/размер/межстрочный интервал, радиус углов, анимации и их скорость, стиль каретки, направляющие отступов, пробелы, ширина Tab, и т.д. Всё в `settings.json`.
- Анимации: плавный скролл, скользящая каретка и подсветка строки, выезд клавиатуры/панелей/оверлеев, ripple на кнопках, тосты, затухание скроллбара.
- Встроенная клавиатура: буквы / цифры / символы, Shift (двойной тап = Caps), стрелки, Tab, подсказки автокомплита в верхнем ряду, повтор при удержании.

## Горячие клавиши (Cmd = Ctrl на macOS)
F1 / Ctrl+Shift+P меню · Ctrl+, настройки · Ctrl+O / S / Shift+S / N / W · Ctrl+F / H / G · F3 далее · Ctrl+Space автодополнение
F5 запуск · Shift+F5 стоп · Ctrl+F5 запуск с аргументами · Ctrl+/ комментарий · Ctrl+D дубль · Ctrl+Shift+K удалить строку · Alt+↑/↓ строка
F4 клавиатура · F2 панель кнопок · F6 язык · F11 фуллскрин · Ctrl+T тема · Ctrl+колесо / +/- масштаб

## Плагины (JSON)
Файлы `*.json` из `<папка данных>/plugins/` (путь показан в Settings > Plugins), `plugins/` рядом с exe и в текущей папке.
Settings > Plugins > «Create sample plugin» создаёт готовый пример. Формат:

    {
      "name": "My plugin",
      "languages": [{
        "name": "Zig", "extensions": ["zig"], "caseInsensitive": false,
        "run": "zig run \"{file}\" -- {args}",
        "keywords": [...], "types": [...], "constants": [...],
        "functions": ["name|signature", ...],
        "comments": {"line": ["//"], "block": [["/*", "*/"]]},
        "strings": ["\"", "'", {"start": "\"\"\"", "end": "\"\"\"", "multiline": true, "escape": "\\"}],
        "indentAfter": ["{"], "dedent": "}", "pairs": "()[]{}\"\"''", "wordChars": ".",
        "annotation": "@", "preprocessor": "#", "variable": "$", "keyColon": false, "hashNumbers": false, "markup": false,
        "identifierChars": "-", "callParens": true,
        "lineStart": [{"prefix": "#", "token": "keyword"}],
        "snippets": [{"prefix": "main", "description": "...", "body": "fn main() {\n\t$0\n}"}]
      }],
      "themes":   [{"name": "Sunset", "base": "Dracula", "colors": {"bg": "#2b1d2e", "accent": "#ff7a59"}}],
      "snippets": [{"lang": "*", "prefix": "todo", "body": "// TODO: $0"}],
      "tasks":    [{"name": "Git status", "cmd": "git status", "lang": "c++"}]
    }
Если язык с таким именем уже есть (например `"name": "C"`), плагин ДОПОЛНЯЕТ его (ключевые слова, функции, сниппеты...).
Названия токенов для `lineStart`: text keyword type function call number string comment preproc register label operator directive.
Цвета темы: bg fg gutter gutterFg curLine sel cursor bar barFg accent popup match btn good warn bad + 13 токенов (см. Settings > Colors).
