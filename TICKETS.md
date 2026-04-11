# Tickets
## Version: 1.2

> To add to the tickets, open an issue! To help the project, fork this repo, do/add to the tickets, and make a pull request.


### High Priority
- [x] ~~Add GUI~~
  - [x] ~~Evaluate C++ GUI options (ImGui, Qt, wxWidgets)~~
  - [x] ~~Make better input prompts (body count, mass, velocity)~~
  - [x] ~~Add real-time parameter editing while simulation runs~~
- [ ] Make physics more realistic
  - [x] ~~Implement proper planet collision / fragmentation~~
  - [x] ~~Evaluate replacing Velocity Verlet (Yoshida)~~
        -> Decided to go with Yoshida
  - [x] ~~Add relativistic corrections for very close orbits~~
  - [x] ~~Add axial tilt and rotation for each body~~
  - [x] ~~Model non-spherical bodies (oblateness / J2 perturbations)~~
  - [x] ~~Add tidal forces between close bodies~~
  - [ ] Implement Lagrange points and show them visually
  - [ ] Support multi-star systems (barycenter tracking)
  - [ ] Add atmospheric drag for low-orbit scenarios
  - [ ] Add planet textures
  - [ ] Variable time step — shrink dt automatically during close approaches
  - [ ] Energy and angular momentum readout to measure integrator drift
  - [ ] Add Spaceships (artemis II, Voyager, etc.) -- requested by kenlinkin2
    - [ ] Abillity to control spaceship
    - [ ] Affect of planets gravity on shpaceships
    - [ ] Shape/texture of the spaceship
- [ ] Make build more portable
  - [ ] Bundle or auto-fetch GLFW (CMake FetchContent or vcpkg manifest)
  - [x] ~~Resolve `glad_out` vs `glad.c` — pick one and remove the other~~
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
  - [x] ~~Add version changelog~~

