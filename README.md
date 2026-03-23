# orrery
---
## A real-time 3D gravitational simulation written in C++ with OpenGL. Configure 2–8 bodies using real planetary presets or custom masses, then watch Newtonian gravity play out — complete with orbital trails, body mergers, and a live info panel.

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
- **2,000-star background** with a reference 3D grid
 
---
 
## Controls
 
| Input | Action |
|---|---|
| Left mouse drag | Rotate camera |
| Scroll wheel | Zoom in / out |
| `Space` | Pause / resume simulation |
| `R` | Reset to initial conditions |
| `I` | Toggle info panel |
| `↑` / `→` | Increase simulation speed |
| `↓` / `←` | Decrease simulation speed |
| `Escape` | Quit |
 
---

## Building
 
### Windows (MinGW / g++)
 
```bash
g++ main.cpp physics.cpp glad.c -o grav_sim \
  -Iinclude -lglfw3 -lopengl32 -lgdi32 -std=c++17
```

Pre-built Windows executables (`grav_sim.exe`, `grav_sim2.exe`) are included in the repo root.
 
### Linux
 
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
 
