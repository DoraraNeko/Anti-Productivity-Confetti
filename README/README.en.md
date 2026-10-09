# Anti-Productivity Confetti (APC) Manual

## Overview

Efficiency has automated the prose, the code, and probably your calendar. **Anti-Productivity Confetti (APC)** is a tiny detour: colorful confetti drifts in from beyond the top of your screens, and every keypress or click sets off corner crackers across your monitors. Their bursts follow cheerful curves; F1–F3 switch their designs. APC will not stop you working. It cannot promise you will stop watching.

## Requirements

- Windows 10 or Windows 11
- Visual Studio 2022 and the Windows SDK (to build from source)

## Usage

Run `APC.exe`. Mouse input passes through the overlay to the application underneath.

- Press a key or click the left, right, or middle mouse button to launch confetti from both crackers.
- Press **F1**, **F2**, or **F3** to switch cracker designs and compare them; each switch also launches confetti.
- Press **Esc** to exit.

## Settings

Edit `settings.ini` beside the executable. If it is missing, the application creates it with defaults on startup. Settings are read at launch. Out-of-range or non-numeric values fall back to their defaults and show a warning.

| Section | Key | Default | Valid range | Description |
|---|---|---:|---:|---|
| Confetti | RainIntervalMs | 50 | 20–1000 | Interval between falling pieces in milliseconds; lower values produce more confetti |
| Confetti | PiecesPerCracker | 55 | 1–200 | Pieces launched by each cracker per input event |
| Confetti | CrackerVisibleMs | 1000 | 100–2000 | How long the crackers remain visible after input; each input extends the display time |
| Confetti | CrackerStyle | 3 | 1–3 | Initial cracker design: 1=Paper cone, 2=Foil tube, 3=Pop/spring |
| Confetti | MaxConfetti | 10000 | 50–20000 | Maximum number of live pieces |

Cracker concepts: [comparison sheet](../docs/cracker-designs/cracker-design-samples.png) ([paper cone](../docs/cracker-designs/01-paper-cone.png) / [foil tube](../docs/cracker-designs/02-foil-tube.png) / [pop / spring](../docs/cracker-designs/03-pop-spring.png)).

## Build

Visual Studio 2022 with the Desktop development with C++ workload and the Windows SDK is required.

```powershell
MSBuild APC.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64
```

## Privacy and license

The application does not capture screen contents or use the network. It is licensed under the [MIT License](../LICENSE).
