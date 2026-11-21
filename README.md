Markdown
# GTA-RUB 4

A 3D open-world action game inspired by **Grand Theft Auto**, written in **C++** using **WinAPI** and **OpenGL** with no external game libraries.

## Repository Structure
- `Code/GTA_C++.cpp` — Full game source code (`GTA-RUB 4`)
- `Code/compilation.txt` — MinGW build command and flags
- `GTA_RUB.exe` & `GTA_RAM.dll` — Compiled executable and runtime library

## Features
- **Open World & Buildings:** Grid city with roads, intersections, streetlights, trees, benches/chairs, roof ladders, minimap, fog, and stencil shadows
- **Weapons & Combat:** Pistol, Assault Rifle, Shotgun, Sniper Rifle (with scope zoom), and RPG with explosions — findable in the world or purchasable at the Gun Shop
- **Vehicles & Climbing:** Driveable car (with handbrake, recall, and jumping onto hood/roof), helicopter with a retractable boarding ladder, and roof access ladders
- **Wanted System, Police & Bank Heist:** 0–5 wanted stars, patrol cars, SWAT vans, police/SWAT/bandits/pedestrians, ruble (`₽`) drops, health shop, and a 6-second bank robbery mechanic

## Build
```bash
"C:\Program Files\CodeBlocks\MinGW\bin\x86_64-w64-mingw32-g++.exe" GTA_C++.cpp -o gta_rub4.exe -O2 -static -mwindows -lopengl32 -lgdi32 -luser32
Controls
WASD — Move / Drive / Fly

Mouse — Camera | LMB — Fire | RMB — Aim / Zoom | 1–5 / Scroll — Select weapon

F — Enter/exit vehicle or climb ladder | R — Call car | E — Heal / Rob bank

Shift — Sprint (or descend in helicopter) | Space — Jump / Handbrake (or ascend in helicopter)

Esc — Quit
