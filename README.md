# Orrery
 A real-time 3D gravitational simulation written in C++ with OpenGL. Configure 2–8 bodies using real planetary presets or custom masses, then watch Newtonian gravity play out — complete with orbital trails, physically-classified collisions, and a live info panel.

##
Orerry (noun.) — a mechanical, usually clockwork-driven model of the solar system that illustrates the relative positions and motions of planets and moons, typically with a sun at the center.
---
 
![Alt text](/screenshots/screenshot1.png "Screenshot of Orrery")

---
## Features
 
- **Real physics units** — gravitational constant G, solar masses, AU distances, and 1-day time steps
- **Yoshida 4th-order symplectic integration** — 3-stage leapfrog composition for superior long-term orbital energy conservation
- **1PN relativistic corrections** — Einstein-Infeld-Hoffmann equations applied per body pair, producing orbital precession (e.g. Mercury's 43″/century) for close or fast orbits
- **2–8 configurable bodies** — choose from Sun, Jupiter, Saturn, Neptune, Uranus, Earth, Venus, Mars, Mercury, or a custom mass
- **Physically-classified collisions** — close encounters (< 0.005 AU) are resolved into one of four regimes based on relative speed vs. mutual escape velocity and impact parameter:
  - **Hit-and-run** — grazing encounter; both bodies survive with a partially elastic impulse
  - **Perfect merge** — slow or head-on; bodies combine conserving mass, momentum, and volume
  - **Partial accretion** — medium energy; large remnant (60–80 % of total mass) plus a smaller ejecta fragment
  - **Catastrophic disruption** — high energy; two comparable fragments (~50/50 split) fly apart
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
 
See [FORMULAS.md](FORMULAS.md) for the full derivations. The core loop uses the **Yoshida (1990) 4th-order ABA symplectic integrator** — a 3-stage composition of leapfrog steps that conserves energy far better than Velocity Verlet over long simulations:

1. Drift `c₁·dt` → recompute forces → kick `d₁·dt`
2. Drift `c₂·dt` → recompute forces → kick `d₂·dt`
3. Drift `c₂·dt` → recompute forces → kick `d₁·dt`
4. Final drift `c₁·dt`

Gravitational force between each pair of bodies is computed as:

```
F = G * m1 * m2 / (r² + ε²)
```

with a softening term `ε = 1×10⁹ m` to prevent singularities at very close range.

### Relativistic corrections

Each body pair also receives a **1PN Einstein-Infeld-Hoffmann** correction on top of the Newtonian force:

```
a_PN = (G·m₂ / r²c²) · { n̂ · [v₁² + 2v₂² − 4(v₁·v₂) − ³⁄₂(n̂·v₂)² + 5Gm₁/r + 4Gm₂/r]
                         + (v₁ − v₂) · [4(n̂·v₁) − 3(n̂·v₂)] }
```

The correction is ~10⁻⁸ of the Newtonian force at typical orbital speeds (v²/c² ≈ 10⁻⁸), but accumulates over time to produce observable precession. It becomes significant for very close orbits or near-relativistic bodies.

### Collision physics

When two bodies come within 0.005 AU, the collision is classified using:

- **`v_esc`** — mutual escape velocity at collision distance: `sqrt(2G(m₁+m₂)/r)`
- **`ξ`** — impact parameter ratio `b/r` (0 = head-on, 1 = grazing)

| Condition | Regime |
|---|---|
| `ξ > 0.65` and `v_rel < 2.5·v_esc` | Hit-and-run |
| `v_rel < 1.5·v_esc` | Perfect merge |
| `1.5·v_esc ≤ v_rel < 3·v_esc` | Partial accretion |
| `v_rel ≥ 3·v_esc` | Catastrophic disruption |

Fragment velocities are computed in the centre-of-mass frame and transformed back to the lab frame, guaranteeing momentum conservation in all regimes. Fragment radii follow the cube-root volume rule.

---

## Tickets

See [TICKETS.md](TICKETS.md) for the full issue tracker.
