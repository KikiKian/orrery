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

| Library | Purpose |
|---|---|
| [GLFW3](https://www.glfw.org/) | Window creation and input handling |
| [GLM](https://github.com/g-truc/glm) | OpenGL mathematics (vectors, matrices) |
| [GLAD](https://glad.dav1d.de/) | OpenGL function loader (source included) |
| [stb_easy_font](https://github.com/nothings/stb) | Header-only text rendering (included) |

GLAD and stb_easy_font are already bundled in the repo. You only need to install GLFW3 and GLM.

---

### Windows (MinGW / g++)

**Install dependencies via [vcpkg](https://vcpkg.io/):**
```bash
vcpkg install glfw3 glm
```

Or via [MSYS2](https://www.msys2.org/):
```bash
pacman -S mingw-w64-x86_64-glfw mingw-w64-x86_64-glm
```

**Build:**
```bash
g++ main.cpp physics.cpp glad.c -o grav_sim \
  -Iinclude -lglfw3 -lopengl32 -lgdi32 -std=c++17
```

Pre-built Windows executables (`grav_sim.exe`, `grav_sim2.exe`) are included in the repo root.

---

### Linux

**Install dependencies:**

Debian / Ubuntu:
```bash
sudo apt install libglfw3-dev libglm-dev
```

Arch Linux:
```bash
sudo pacman -S glfw glm
```

Fedora:
```bash
sudo dnf install glfw-devel glm-devel
```

**Build:**
```bash
g++ main.cpp physics.cpp glad.c -o grav_sim \
  -Iinclude -lglfw -lGL -ldl -std=c++17
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

To add to checklist open issue!
```
$HIGH PRIORITY:
  [ ] Add GUI
    -> Use c++ module
    -> Different module from different language
    [ ] Make better input prompts

  [ ] Make more realistic
    [ ] Implement planet collisions
    [ ] Switch from Velocity Verlet to another kick

  [ ] Make packages universal for easier build
    -> Use Docker maybe?
    [ ] Figure out if we have to use glad_out

$LOW PRIORITY:
  [ ] Make units more relatable (kg, m, lb, etc.)
  [ ] Make custom planet and distance options for easier use

$CHORES:
  [ ] Quality of life
    [ ] Make grid background better
      [ ] Extend grid further
      [ ] Diff color
    [ ] Better controls
    [ ] Controls help menu
    [ ] Escape menu

  [ ] Dev quality of life
    [ ] Add Dev tests
    [ ] Better comments
    [ ] Refactor so it's easier to understand

  [ ] Documentation (.mds)
    [ ] Add/Upate Screenshots
    [ ] Update build docs
    [ ] Add version docs
 ```