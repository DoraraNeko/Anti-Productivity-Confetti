# Anti-Productivity Confetti (APC) Specification

## 1. Purpose

Continuously render colorful confetti over the Windows desktop and launch pieces from crackers at both bottom corners when a key is pressed or the mouse is clicked.

## 2. Platform and display

- Windows 10 / Windows 11.
- A topmost, non-activating, click-through layered window covers the virtual desktop.
- Use physical virtual-screen coordinates and exclude the overlay from screen capture.
- Enumerate each monitor's display bounds and use its individual bounds for cracker placement and launch height.

## 3. Functional behavior

### 3.1 Confetti

- Pieces spawn above the top edge and fall through the screen under gravity.
- Pieces have varied colors, dimensions, rotation and sway.
- Low-level keyboard and mouse hooks detect key presses and left, right or middle clicks. They never suppress input and post events to the message queue with an 80 ms rate limit.
- Draw one cracker at each bottom corner of every monitor only after input. Hide crackers one second after the most recent input by default; further input extends the display time.
- Each input event launches pieces radially from every cracker, with an apex about 35–105% of that monitor's height. Initial velocity and gravity form ballistic arcs.
- Crackers have a tapered tube, diagonal colored bands, and a visible open mouth so their launch point is recognizable.
- The configured maximum bounds the live particle count. Particles outside the screen or past their lifetime are removed.
- Esc exits the application.
- Press F1 / F2 / F3 to switch between and compare the three cracker designs at runtime. Switching designs also launches confetti.

### 3.2 Configuration

Read `settings.ini` beside the executable at startup, creating it with defaults if absent. Missing and invalid values use defaults; invalid values trigger a warning.

| Section | Key | Default | Range |
|---|---|---:|---:|
| Confetti | RainIntervalMs | 50 | 20–1000 milliseconds |
| Confetti | PiecesPerCracker | 55 | 1–200 pieces |
| Confetti | CrackerVisibleMs | 1000 | 100–2000 milliseconds |
| Confetti | CrackerStyle | PopSpring | PaperCone / FoilTube / PopSpring |
| Confetti | MaxConfetti | 10000 | 50–20000 pieces |

`CrackerStyle` accepts `PaperCone`, `FoilTube`, or `PopSpring`. For compatibility with older settings files, `1`, `2`, and `3` are also accepted in the same order.

### 3.3 Implementation

- C++17 / Win32 APIs. Visual Studio 2022 v143 toolset.
- Draw into a 32-bit BGRA DIB using premultiplied alpha and present with `UpdateLayeredWindow`.
- Update on a window timer at approximately 16 ms intervals.
- Input hooks pass events on through `CallNextHookEx` and do not block input.
- The application does not capture or save screen contents and makes no network connections.

## 4. Build and distribution

- `MSBuild APC.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64`
- `APC.exe` and the settings file are output to `bin\x64\Release\`.
- `powershell -ExecutionPolicy Bypass -File scripts\package.ps1` creates a release zip and SHA-256 checksum in `dist\`.

## 5. Project layout

| Path | Contents |
|---|---|
| `APC.vcxproj` | x64 Debug/Release Visual C++ project |
| `src\Main.cpp` | Confetti simulation, rendering, input hooks, and settings |
| `src\APC.rc` / `src\APC.ico` | Executable version information and icon |
| `settings.ini` | Runtime settings |
| `README.md` / `README\` | Project overview and Japanese/English manuals |
| `SPECIFICATION\` | Japanese and English specifications |
| `docs\cracker-designs\` | PNG cracker design concepts |
| `LICENSE` / `CHANGELOG.md` | License and release history |
| `scripts\package.ps1` | Release build, archive, and SHA-256 packaging |
