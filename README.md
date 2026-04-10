# Orrery
 A real-time 3D gravitational simulation written in C++ with OpenGL. Configure 2–8 bodies using real planetary presets or custom masses, then watch Newtonian gravity play out — complete with orbital trails, body mergers, and a live info panel.

##
Orerry (noun.) — a mechanical, usually clockwork-driven model of the solar system that illustrates the relative positions and motions of planets and moons, typically with a sun at the center.
---
 
![Alt text](/screenshots/screenshot1.png "Screenshot of Orrery")

---
## Features
 
- **Real physics units** — gravitational constant G, solar masses, AU distances, and 1-day time steps
- **Velocity Verlet integration** — symplectic integrator for stable long-term orbital energy conservation
- **2–8 configurable bodies** — choose from Sun, Jupiter, Saturn, Neptune, Uranus, Earth, Venus, Mars, Mercury, or a custom mass
- **Body mergers** — when two bodies come within 0.005 AU, they combine conserving mass, momentum, and volume
- **Orbital trails** — each body leaves a color-coded trail (up to 300 points)
- **Live info panel** — real-time speed (km/s), distance (AU / million km), and mass (solar masses) per body
- **Real-time controls panel** — pause, reset, adjust simulation speed, and edit body masses live via ImGui
- **Interactive 3D camera** — rotate, zoom, and pan freely
 
---
 
## Controls
 
| Input | Action |
|---|---|
| Left mouse drag | Rotate camera |
| Scroll wheel | Zoom in / out |
| `Space` | Pause / resume simulation |
| `R` | Reset to initial conditions |
| `I` | Toggle info panel |
| `F` | Toggle force arrows |
| `↑` / `→` | Increase simulation speed |
| `↓` / `←` | Decrease simulation speed |
| `Escape` | Quit |
 
---

## Building

### Dependencies

| Library | Purpose | Bundled? |
|---|---|---|
| [GLFW3](https://www.glfw.org/) | Window creation and input handling | Headers only — library must be installed |
| [GLM](https://github.com/g-truc/glm) | OpenGL mathematics (vectors, matrices) | Yes (header-only) |
| [GLAD](https://glad.dav1d.de/) | OpenGL function loader | Yes (`glad.c`, `include/glad/`) |
| [stb_easy_font](https://github.com/nothings/stb) | Text rendering | Yes (`include/stb_easy_font.h`) |
| [Dear ImGui](https://github.com/ocornut/imgui) | GUI / parameter panel | Yes (`include/imgui/`) |

Only the **GLFW3 library** needs to be installed — everything else is already in the repo.

---

### Windows (MinGW / g++)

**Install GLFW3 via [vcpkg](https://vcpkg.io/):**
```bash
vcpkg install glfw3
```

Or via [MSYS2](https://www.msys2.org/):
```bash
pacman -S mingw-w64-x86_64-glfw
```

**Build:**
```bash
g++ main.cpp physics.cpp glad.c \
  include/imgui/imgui.cpp include/imgui/imgui_draw.cpp \
  include/imgui/imgui_tables.cpp include/imgui/imgui_widgets.cpp \
  include/imgui/imgui_impl_glfw.cpp include/imgui/imgui_impl_opengl3.cpp \
  -o grav_sim -Iinclude -Iinclude/imgui -lglfw3 -lopengl32 -lgdi32 -std=c++17
```

Pre-built Windows executables (`grav_sim.exe`, `grav_sim2.exe`) are included in the repo root.

---

### Linux

**Install GLFW3:**

Debian / Ubuntu:
```bash
sudo apt install libglfw3-dev
```

Arch Linux:
```bash
sudo pacman -S glfw
```

Fedora:
```bash
sudo dnf install glfw-devel
```

**Build:**
```bash
g++ main.cpp physics.cpp glad.c \
  include/imgui/imgui.cpp include/imgui/imgui_draw.cpp \
  include/imgui/imgui_tables.cpp include/imgui/imgui_widgets.cpp \
  include/imgui/imgui_impl_glfw.cpp include/imgui/imgui_impl_opengl3.cpp \
  -o grav_sim -Iinclude -Iinclude/imgui -lglfw -lGL -ldl -std=c++17
```
 
---
## Physics
 
See [FORMULAS.md](FORMULAS.md) for the full derivations. The core loop uses **Velocity Verlet**:
 
1. Half-kick: `v += 0.5 * a * dt`
2. Drift: `x += v * dt`
3. Recompute gravitational accelerations at new positions
4. Second half-kick: `v += 0.5 * a * dt`
 
Gravitational force between each pair of bodies is computed as:
 
```
F = G * m1 * m2 / r²
```
 
with acceleration applied in the direction of the unit vector between them.

---
# Checklist / Tickets
## Version: 1.0

> To add to the checklist, open an issue!

### High Priority

- ~~[x] Add GUI
  - [x] Evaluate C++ GUI options (ImGui, Qt, wxWidgets)
  - [x] Make better input prompts (body count, mass, velocity)
  - [x] Add real-time parameter editing while simulation runs~~
- [ ] Make physics more realistic
  - [ ] Implement proper planet collision / fragmentation
  - [ ] Evaluate replacing Velocity Verlet (RK4, Leapfrog)
  - [ ] Add relativistic corrections for very close orbits
  - [ ] Add axial tilt and rotation for each body
  - [ ] Model non-spherical bodies (oblateness / J2 perturbations)
  - [ ] Add tidal forces between close bodies
  - [ ] Implement Lagrange points and show them visually
  - [ ] Support multi-star systems (barycenter tracking)
  - [ ] Add atmospheric drag for low-orbit scenarios
  - [ ] Variable time step — shrink dt automatically during close approaches
  - [ ] Energy and angular momentum readout to measure integrator drift
- [ ] Make build more portable
  - [ ] Bundle or auto-fetch GLFW (CMake FetchContent or vcpkg manifest)
  - [ ] Resolve `glad_out` vs `glad.c` — pick one and remove the other
  - [ ] Add a `CMakeLists.txt` so users don't have to hand-write the compile command

### Low Priority

- [ ] Make units more relatable (kg, m/s, lb toggle)
- [ ] Add custom planet name, mass, radius, and starting velocity options
- [ ] Add preset solar system configurations (inner solar system, gas giants, etc.)
- [ ] Time display (simulated years / days elapsed)
- [ ] Export trail data to CSV for external analysis

### Chore

- [ ] Quality of life
  - [ ] Make grid background better
    - [ ] Extend grid further out
    - [ ] Different / configurable grid color
  - [ ] Better mouse sensitivity / control tuning
  - [ ] In-sim controls help overlay (`H` key)
  - [ ] Pause / escape menu with resume option
  - [ ] Fullscreen toggle
- [ ] Dev quality of life
  - [ ] Add unit tests for physics (verlet integrator, merger logic)
  - [ ] Add integration test: known orbit stays stable over N steps
  - [ ] Better inline comments on non-obvious physics code
  - [ ] Refactor `main.cpp` — split rendering, UI, and sim-loop into separate files
  - [ ] Set up CI (GitHub Actions) to build on push
- [ ] Documentation
  - [ ] Add / update screenshots
  - [ ] Update build docs once CMake is added
  - [ ] Add version changelog
