#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <string>
#include <vector>
#include <deque>
#include <random>
#include <sstream>
#include <iomanip>
#define _USE_MATH_DEFINES

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "body.h"
#include "physics.h"

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

//--- Real unit constants ---
const double G            = 6.674e-11;   // gravitational constant (m^3 kg^-1 s^-2)
const double SOLAR_MASS   = 2.0e30;      // kg per solar mass
const double AU           = 1.496e11;    // meters per AU
const double RENDER_SCALE = 5.0 / AU;    // 1 AU = 5 screen units
const double dt           = 86400.0;     // 1 day per physics step
int          STEPS_PER_FRAME = 5;        // physics steps per frame (arrow keys to adjust)
const int    TRAIL_LENGTH    = 300;      // max trail points per body

//--- Camera state ---
float camYaw    = 0.0f;
float camPitch  = 35.0f;
float camDist   = 12.0f;
bool  mouseHeld = false;
double lastMouseX = 0.0, lastMouseY = 0.0;

//--- Simulation state ---
bool paused      = false;
bool showPanel   = true;    // toggle with I key
int  numBodies   = 2;
std::vector<Body>         bodiesInitial;
std::deque<glm::vec3>     trails[8];
std::vector<std::string>  bodyNames;   // stores chosen preset names

//--- Callbacks ---
void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_SPACE)
        paused = !paused;
    if (action == GLFW_PRESS && key == GLFW_KEY_I)
        showPanel = !showPanel;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_UP   || key == GLFW_KEY_RIGHT)
            STEPS_PER_FRAME = glm::clamp(STEPS_PER_FRAME + 1, 1, 100);
        if (key == GLFW_KEY_DOWN || key == GLFW_KEY_LEFT)
            STEPS_PER_FRAME = glm::clamp(STEPS_PER_FRAME - 1, 1, 100);
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            mouseHeld = true;
            glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
        } else {
            mouseHeld = false;
        }
    }
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (!mouseHeld) return;
    float dx = (float)(xpos - lastMouseX);
    float dy = (float)(ypos - lastMouseY);
    camYaw   += dx * 0.3f;
    camPitch += dy * 0.3f;
    camPitch = glm::clamp(camPitch, -89.0f, 89.0f);
    lastMouseX = xpos;
    lastMouseY = ypos;
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    camDist -= (float)yoffset * 1.5f;
    camDist = glm::clamp(camDist, 2.0f, 200.0f);
}

//--- Planet shader — circle shape with soft glow edge ---
const char* planetVertSrc = R"(
#version 460 core
layout (location = 0) in vec3 position;
uniform mat4 MVP;
uniform float pointRadius;
uniform float camDist;
void main() {
    gl_Position = MVP * vec4(position, 1.0);
    gl_PointSize = pointRadius * (20.0 / camDist);
}
)";

const char* planetFragSrc = R"(
#version 460 core
out vec4 fragColor;
uniform vec3 planetColor;
uniform float alpha;
void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    if (dist > 0.5) discard;
    float brightness = 1.0 - smoothstep(0.2, 0.5, dist);
    fragColor = vec4(planetColor * brightness, alpha);
}
)";

//--- Simple shader — used for grid, trails, stars, and UI ---
const char* simpleVertSrc = R"(
#version 460 core
layout (location = 0) in vec3 position;
uniform mat4 MVP;
void main() {
    gl_Position = MVP * vec4(position, 1.0);
}
)";

const char* simpleFragSrc = R"(
#version 460 core
out vec4 fragColor;
uniform vec4 color;
void main() {
    fragColor = color;
}
)";

//--- Compile and link a shader program ---
unsigned int compileShader(const char* vertSrc, const char* fragSrc) {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertSrc, NULL);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragSrc, NULL);
    glCompileShader(fs);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

//--- Body presets with real masses and visual sizes ---
struct Preset {
    const char* name;
    double massSolar;       // mass in solar masses
    double visualRadius;    // visual size in pixels
    double orbitalRadiusAU; // real mean orbital radius from the Sun in AU
};
const Preset PRESETS[] = {
    { "Sun",     1.0,      40.0,  0.0   },
    { "Jupiter", 9.54e-4,  28.0,  5.203 },
    { "Saturn",  2.84e-4,  23.0,  9.537 },
    { "Neptune", 5.15e-5,  16.0, 30.069 },
    { "Uranus",  4.37e-5,  15.0, 19.191 },
    { "Earth",   3.00e-6,  12.0,  1.000 },
    { "Venus",   2.45e-6,  11.0,  0.723 },
    { "Mars",    3.21e-7,   9.0,  1.524 },
    { "Mercury", 1.65e-7,   7.0,  0.387 },
    { "Custom",  0.0,       0.0,  0.0   },
};
const int NUM_PRESETS = 10;

//--- Fixed body colors for up to 8 bodies ---
const glm::vec3 BODY_COLORS[8] = {
    glm::vec3(1.0f, 1.0f, 1.0f),   // white
    glm::vec3(1.0f, 0.2f, 0.2f),   // red
    glm::vec3(0.2f, 0.8f, 1.0f),   // cyan
    glm::vec3(1.0f, 0.8f, 0.1f),   // yellow
    glm::vec3(0.2f, 1.0f, 0.3f),   // green
    glm::vec3(1.0f, 0.5f, 0.1f),   // orange
    glm::vec3(0.9f, 0.3f, 1.0f),   // purple
    glm::vec3(0.4f, 0.8f, 1.0f),   // light blue
};

//--- Generate random star field ---
std::vector<float> generateStars(int count) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-300.0f, 300.0f);
    std::vector<float> stars;
    stars.reserve(count * 3);
    for (int i = 0; i < count; i++) {
        stars.push_back(dist(rng));
        stars.push_back(dist(rng));
        stars.push_back(dist(rng));
    }
    return stars;
}

//--- Project a 3D world position to 2D screen pixels ---
glm::vec2 worldToScreen(glm::vec3 worldPos, glm::mat4 MVP, int width, int height) {
    glm::vec4 clip = MVP * glm::vec4(worldPos, 1.0f);
    glm::vec3 ndc  = glm::vec3(clip) / clip.w;
    return glm::vec2(
        (ndc.x + 1.0f) * 0.5f * width,
        (1.0f - ndc.y) * 0.5f * height   // flip Y — screen Y grows downward
    );
}

//--- Format a double to N decimal places as a string ---
std::string fmt(double val, int decimals = 2) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(decimals) << val;
    return ss.str();
}

int main() {
    std::cout << "=== Gravity Simulation ===\n";
    std::cout << "Controls: Left drag = rotate | Scroll = zoom | Space = pause | R = reset | I = info panel\n\n";

    std::cout << "How many bodies? (2-8): ";
    std::cin >> numBodies;
    numBodies = glm::clamp(numBodies, 2, 8);
    std::cout << "\n";

    // returns {massSolar, visualRadius, orbitalRadiusAU, name}
    auto pickBody = [&](const char* label) -> std::tuple<double, double, double, std::string> {
        std::cout << "Choose " << label << ":\n";
        for (int i = 0; i < NUM_PRESETS; i++) {
            if (i < NUM_PRESETS - 1)
                std::cout << "  " << i+1 << ". " << PRESETS[i].name
                          << " (" << PRESETS[i].massSolar << " M_sun"
                          << ", orbit " << PRESETS[i].orbitalRadiusAU << " AU)\n";
            else
                std::cout << "  " << i+1 << ". Custom\n";
        }
        std::cout << "Enter choice (1-" << NUM_PRESETS << "): ";
        int choice; std::cin >> choice;
        choice = glm::clamp(choice, 1, NUM_PRESETS) - 1;
        if (choice < NUM_PRESETS - 1) {
            std::cout << "Selected: " << PRESETS[choice].name << "\n\n";
            return { PRESETS[choice].massSolar, PRESETS[choice].visualRadius,
                     PRESETS[choice].orbitalRadiusAU, PRESETS[choice].name };
        } else {
            double mass;
            std::cout << "Enter mass (solar masses): "; std::cin >> mass;
            double rad = glm::clamp(20.0 * std::pow(mass, 0.8), 8.0, 70.0);
            std::cout << "\n";
            return { mass, rad, 1.0, "Custom" };  // default 1 AU
        }
    };

    std::vector<double> massSolars(numBodies);
    std::vector<double> radii(numBodies);
    std::vector<double> orbitalRadii(numBodies);
    bodyNames.resize(numBodies);
    for (int i = 0; i < numBodies; i++) {
        std::string label = "Body " + std::to_string(i+1);
        auto [m, r, orbitAU, name] = pickBody(label.c_str());
        massSolars[i]   = m;
        radii[i]        = r;
        orbitalRadii[i] = orbitAU * AU;
        bodyNames[i]    = name;
    }

    std::vector<Body> bodies(numBodies);
    for (int i = 0; i < numBodies; i++) {
        bodies[i].mass   = massSolars[i] * SOLAR_MASS;
        bodies[i].radius = radii[i];
        bodies[i].ax = bodies[i].ay = bodies[i].az = 0.0;
        bodies[i].vx = bodies[i].vy = bodies[i].vz = 0.0;
    }

    double total_mass = 0.0;
    for (auto& b : bodies) total_mass += b.mass;

    // Place each body at its own orbital radius, evenly spaced in angle on a tilted plane
    const double PI   = 3.14159265358979323846;
    const double tilt = PI / 6.0;  // 30-degree tilt so 3/4 bodies have visible z-axis motion
    for (int i = 0; i < numBodies; i++) {
        double angle = 2.0 * PI * i / numBodies;
        bodies[i].x = orbitalRadii[i] * std::cos(angle);
        bodies[i].y = orbitalRadii[i] * std::sin(angle) * std::cos(tilt);
        bodies[i].z = orbitalRadii[i] * std::sin(angle) * std::sin(tilt);
    }

    // Shift positions so center of mass is at origin
    double comX = 0.0, comY = 0.0, comZ = 0.0;
    for (auto& b : bodies) {
        comX += b.mass * b.x;
        comY += b.mass * b.y;
        comZ += b.mass * b.z;
    }
    comX /= total_mass; comY /= total_mass; comZ /= total_mass;
    for (auto& b : bodies) { b.x -= comX; b.y -= comY; b.z -= comZ; }

    // Give each body a circular orbit velocity based on its actual distance from COM
    for (int i = 0; i < numBodies; i++) {
        if (orbitalRadii[i] < 0.001 * AU) continue;  // skip bodies placed at origin (e.g. Sun)
        double r = std::sqrt(bodies[i].x*bodies[i].x + bodies[i].y*bodies[i].y + bodies[i].z*bodies[i].z);
        double speed = std::sqrt(G * total_mass / r);
        double angle = 2.0 * PI * i / numBodies;
        bodies[i].vx = -speed * std::sin(angle);
        bodies[i].vy =  speed * std::cos(angle) * std::cos(tilt);
        bodies[i].vz =  speed * std::cos(angle) * std::sin(tilt);
    }

    // Correct velocities so total momentum = 0 (COM stays fixed at origin)
    double pvx = 0.0, pvy = 0.0, pvz = 0.0;
    for (auto& b : bodies) { pvx += b.mass * b.vx; pvy += b.mass * b.vy; pvz += b.mass * b.vz; }
    for (auto& b : bodies) { b.vx -= pvx/total_mass; b.vy -= pvy/total_mass; b.vz -= pvz/total_mass; }

    // Compute initial forces so Velocity Verlet starts with correct accelerations
    for (int i = 0; i < numBodies; i++)
        for (int j = i+1; j < numBodies; j++)
            computeGravity(bodies[i], bodies[j], G);

    bodiesInitial = bodies;

    //--- Init GLFW ---
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    int screenW = mode->width, screenH = mode->height;
    GLFWwindow* window = glfwCreateWindow(screenW, screenH, "Gravity Simulation", monitor, NULL);
    if (!window) {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    glEnable(GL_POINT_SPRITE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //--- Compile shaders ---
    unsigned int planetShader = compileShader(planetVertSrc, planetFragSrc);
    unsigned int simpleShader = compileShader(simpleVertSrc, simpleFragSrc);

    int pMVP     = glGetUniformLocation(planetShader, "MVP");
    int pRadius  = glGetUniformLocation(planetShader, "pointRadius");
    int pCamDist = glGetUniformLocation(planetShader, "camDist");
    int pColor   = glGetUniformLocation(planetShader, "planetColor");
    int pAlpha   = glGetUniformLocation(planetShader, "alpha");
    int sMVP     = glGetUniformLocation(simpleShader, "MVP");
    int sColor   = glGetUniformLocation(simpleShader, "color");

    //--- Planet VAO/VBO ---
    unsigned int planetVAO, planetVBO;
    glGenVertexArrays(1, &planetVAO);
    glGenBuffers(1, &planetVBO);
    glBindVertexArray(planetVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planetVBO);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //--- Trail VAOs/VBOs — one set per body (up to 8) ---
    unsigned int trailVAO[8], trailVBO[8];
    glGenVertexArrays(8, trailVAO);
    glGenBuffers(8, trailVBO);
    for (int i = 0; i < 8; i++) {
        glBindVertexArray(trailVAO[i]);
        glBindBuffer(GL_ARRAY_BUFFER, trailVBO[i]);
        glBufferData(GL_ARRAY_BUFFER, TRAIL_LENGTH * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }

    //--- Stars VAO/VBO ---
    auto starData = generateStars(2000);
    unsigned int starVAO, starVBO;
    glGenVertexArrays(1, &starVAO); glGenBuffers(1, &starVBO);
    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferData(GL_ARRAY_BUFFER, starData.size() * sizeof(float), starData.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //--- Grid VAO/VBO ---
    std::vector<float> gridVertices;
    int   gridSize   = 60;
    float gridStep   = 1.0f;
    float gridExtent = gridSize * gridStep;
    for (int i = -gridSize; i <= gridSize; i++) {
        float pos  = i * gridStep;
        // Floor grid (XZ plane at y=0)
        gridVertices.insert(gridVertices.end(), { pos, 0.0f, -gridExtent });
        gridVertices.insert(gridVertices.end(), { pos, 0.0f,  gridExtent });
        gridVertices.insert(gridVertices.end(), { -gridExtent, 0.0f, pos });
        gridVertices.insert(gridVertices.end(), {  gridExtent, 0.0f, pos });
        // Back wall (XY plane at z=-gridExtent) — vertical lines go above AND below
        gridVertices.insert(gridVertices.end(), { pos, -gridExtent, -gridExtent });
        gridVertices.insert(gridVertices.end(), { pos,  gridExtent, -gridExtent });
        // Back wall — horizontal lines
        float ypos = i * gridStep;
        gridVertices.insert(gridVertices.end(), { -gridExtent, ypos, -gridExtent });
        gridVertices.insert(gridVertices.end(), {  gridExtent, ypos, -gridExtent });
        // Left wall (YZ plane at x=-gridExtent) — vertical lines go above AND below
        gridVertices.insert(gridVertices.end(), { -gridExtent, -gridExtent, pos });
        gridVertices.insert(gridVertices.end(), { -gridExtent,  gridExtent, pos });
        // Left wall — horizontal lines
        gridVertices.insert(gridVertices.end(), { -gridExtent, ypos, -gridExtent });
        gridVertices.insert(gridVertices.end(), { -gridExtent, ypos,  gridExtent });
    }

    unsigned int gridVAO, gridVBO;
    glGenVertexArrays(1, &gridVAO); glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //--- UI VAO/VBO — reused each frame for panel backgrounds, stems, and text quads ---
    unsigned int uiVAO, uiVBO;
    glGenVertexArrays(1, &uiVAO); glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, 99999 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //--- UI helper lambdas ---
    auto drawRect = [&](float x, float y, float w, float h, glm::vec4 col) {
        float v[] = {
            x,   y,   0,  x+w, y,   0,  x+w, y+h, 0,
            x,   y,   0,  x+w, y+h, 0,  x,   y+h, 0
        };
        glBindVertexArray(uiVAO);
        glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
        glUniform4f(sColor, col.r, col.g, col.b, col.a);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };

    auto drawLine2D = [&](float x1, float y1, float x2, float y2, glm::vec4 col) {
        float v[] = { x1, y1, 0, x2, y2, 0 };
        glBindVertexArray(uiVAO);
        glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
        glUniform4f(sColor, col.r, col.g, col.b, col.a);
        glDrawArrays(GL_LINES, 0, 2);
    };

    // Draw text using stb_easy_font — converts string to filled quads
    auto drawText = [&](float x, float y, float scale, const std::string& text, glm::vec4 col) {
        static char buf[99999];
        int nq = stb_easy_font_print(0, 0, (char*)text.c_str(), nullptr, buf, sizeof(buf));

        // stb_easy_font vertex layout: float x, float y, float z, 4 bytes color (16 bytes total)
        struct SVertex { float x, y, z; unsigned char r, g, b, a; };
        SVertex* sv = (SVertex*)buf;

        // Convert quads → triangles and apply position + scale
        std::vector<float> pts;
        pts.reserve(nq * 18);
        for (int q = 0; q < nq; q++) {
            int b = q * 4;
            for (int idx : {0,1,2, 0,2,3}) {
                pts.push_back(x + sv[b+idx].x * scale);
                pts.push_back(y + sv[b+idx].y * scale);
                pts.push_back(0.0f);
            }
        }

        glBindVertexArray(uiVAO);
        glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, pts.size() * sizeof(float), pts.data());
        glUniform4f(sColor, col.r, col.g, col.b, col.a);
        glDrawArrays(GL_TRIANGLES, 0, nq * 6);
    };

    //--- Fixed projection matrix ---
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        (float)screenW / (float)screenH,
        0.1f, 1000.0f
    );

    //--- Render loop ---
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            bodies    = bodiesInitial;
            numBodies = (int)bodies.size();
            massSolars.resize(numBodies); radii.resize(numBodies); bodyNames.resize(numBodies);
            for (int i = 0; i < 8; i++) trails[i].clear();
        }

        // Velocity Verlet: half-kick → drift → recompute forces → half-kick
        if (!paused) {
            for (int step = 0; step < STEPS_PER_FRAME; step++) {
                // Half-kick: v += 0.5 * a * dt  (using forces from previous step)
                for (auto& b : bodies) {
                    b.vx += 0.5 * b.ax * dt;
                    b.vy += 0.5 * b.ay * dt;
                    b.vz += 0.5 * b.az * dt;
                }
                // Drift: x += v * dt
                for (auto& b : bodies) {
                    b.x += b.vx * dt;
                    b.y += b.vy * dt;
                    b.z += b.vz * dt;
                }
                // Recompute forces at new positions
                for (auto& b : bodies) { b.ax = b.ay = b.az = 0.0; }
                for (int i = 0; i < (int)bodies.size(); i++)
                    for (int j = i+1; j < (int)bodies.size(); j++)
                        computeGravity(bodies[i], bodies[j], G);
                // Second half-kick: v += 0.5 * a * dt  (now uses new forces)
                for (auto& b : bodies) {
                    b.vx += 0.5 * b.ax * dt;
                    b.vy += 0.5 * b.ay * dt;
                    b.vz += 0.5 * b.az * dt;
                }

                // Collision/merger — if two bodies get within 0.005 AU they merge
                for (int i = 0; i < (int)bodies.size(); i++) {
                    for (int j = i+1; j < (int)bodies.size(); j++) {
                        double ddx = bodies[i].x - bodies[j].x;
                        double ddy = bodies[i].y - bodies[j].y;
                        double ddz = bodies[i].z - bodies[j].z;
                        if (std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz) > 0.005 * AU) continue;
                        // Merge j into i — conserve mass, momentum, and volume
                        double mTot = bodies[i].mass + bodies[j].mass;
                        bodies[i].x  = (bodies[i].mass*bodies[i].x  + bodies[j].mass*bodies[j].x)  / mTot;
                        bodies[i].y  = (bodies[i].mass*bodies[i].y  + bodies[j].mass*bodies[j].y)  / mTot;
                        bodies[i].z  = (bodies[i].mass*bodies[i].z  + bodies[j].mass*bodies[j].z)  / mTot;
                        bodies[i].vx = (bodies[i].mass*bodies[i].vx + bodies[j].mass*bodies[j].vx) / mTot;
                        bodies[i].vy = (bodies[i].mass*bodies[i].vy + bodies[j].mass*bodies[j].vy) / mTot;
                        bodies[i].vz = (bodies[i].mass*bodies[i].vz + bodies[j].mass*bodies[j].vz) / mTot;
                        bodies[i].mass    = mTot;
                        massSolars[i]    += massSolars[j];
                        // Radius grows with cube root of combined volume
                        bodies[i].radius  = std::cbrt(std::pow(radii[i],3.0) + std::pow(radii[j],3.0));
                        radii[i]          = bodies[i].radius;
                        bodyNames[i]      = bodyNames[i] + "+" + bodyNames[j];
                        bodies.erase(bodies.begin() + j);
                        trails[j].clear();
                        massSolars.erase(massSolars.begin() + j);
                        radii.erase(radii.begin() + j);
                        bodyNames.erase(bodyNames.begin() + j);
                        numBodies--;
                        j--;
                    }
                }
            }
            float s = (float)RENDER_SCALE;
            for (int i = 0; i < numBodies; i++) {
                trails[i].push_back({ (float)bodies[i].x * s, (float)bodies[i].y * s, (float)bodies[i].z * s });
                if ((int)trails[i].size() > TRAIL_LENGTH) trails[i].pop_front();
            }
        }

        float yawRad   = glm::radians(camYaw);
        float pitchRad = glm::radians(camPitch);
        glm::vec3 camPos(
            camDist * cos(pitchRad) * sin(yawRad),
            camDist * sin(pitchRad),
            camDist * cos(pitchRad) * cos(yawRad)
        );
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 MVP  = projection * view * glm::mat4(1.0f);

        glClearColor(0.01f, 0.01f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(simpleShader);
        glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(MVP));
        glUniform4f(sColor, 1.0f, 1.0f, 1.0f, 0.6f);
        glPointSize(1.5f);
        glBindVertexArray(starVAO);
        glDrawArrays(GL_POINTS, 0, (int)starData.size() / 3);

        glUniform4f(sColor, 0.05f, 0.25f, 0.45f, 0.6f);  // steel blue grid
        glBindVertexArray(gridVAO);
        glDrawArrays(GL_LINES, 0, (int)gridVertices.size() / 3);

        for (int i = 0; i < numBodies; i++) {
            if (trails[i].size() < 2) continue;
            std::vector<float> pts;
            pts.reserve(trails[i].size() * 3);
            for (auto& p : trails[i]) { pts.push_back(p.x); pts.push_back(p.y); pts.push_back(p.z); }
            glBindVertexArray(trailVAO[i]);
            glBindBuffer(GL_ARRAY_BUFFER, trailVBO[i]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, pts.size() * sizeof(float), pts.data());
            glm::vec3 col = BODY_COLORS[i];
            glUniform4f(sColor, col.r, col.g, col.b, 0.5f);
            glDrawArrays(GL_LINE_STRIP, 0, (int)trails[i].size());
        }

        // Draw planets (glow pass then solid pass) for each body
        glUseProgram(planetShader);
        glUniformMatrix4fv(pMVP, 1, GL_FALSE, glm::value_ptr(MVP));
        glUniform1f(pCamDist, camDist);
        glBindVertexArray(planetVAO);

        for (int i = 0; i < numBodies; i++) {
            float pos[3] = {
                (float)(bodies[i].x * RENDER_SCALE),
                (float)(bodies[i].y * RENDER_SCALE),
                (float)(bodies[i].z * RENDER_SCALE)
            };
            glBindBuffer(GL_ARRAY_BUFFER, planetVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(pos), pos);
            glm::vec3 col = BODY_COLORS[i];
            glUniform3f(pColor, col.r, col.g, col.b);
            glUniform1f(pRadius, (float)radii[i] * 3.0f);  // glow pass
            glUniform1f(pAlpha, 0.12f);
            glDrawArrays(GL_POINTS, 0, 1);
            glUniform1f(pRadius, (float)radii[i]);          // solid pass
            glUniform1f(pAlpha, 1.0f);
            glDrawArrays(GL_POINTS, 0, 1);
        }

        //--- Info panel overlay (drawn in 2D screen space) ---
        if (showPanel) {
            glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
            glUseProgram(simpleShader);
            glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(ortho));

            const float panelW    = 280.0f;
            const float panelH    = 112.0f;
            const float panelGap  = 6.0f;
            const float textScale = 2.0f;
            const float lineH     = 18.0f;

            float panelX = (float)screenW - panelW - 18.0f;

            for (int i = 0; i < numBodies; i++) {
                glm::vec3 worldPos = {
                    (float)(bodies[i].x * RENDER_SCALE),
                    (float)(bodies[i].y * RENDER_SCALE),
                    (float)(bodies[i].z * RENDER_SCALE)
                };

                glm::vec2 sp = worldToScreen(worldPos, MVP, screenW, screenH);
                float px = panelX;
                float py = 18.0f + i * (panelH + panelGap);
                glm::vec3 c = BODY_COLORS[i];

                drawRect(px, py, panelW, panelH, glm::vec4(0.04f, 0.04f, 0.10f, 0.88f));
                drawRect(px, py, 4.0f, panelH, glm::vec4(c.r, c.g, c.b, 1.0f));

                // --- Real-life stats ---
                double speed_kms = std::sqrt(
                    bodies[i].vx*bodies[i].vx +
                    bodies[i].vy*bodies[i].vy +
                    bodies[i].vz*bodies[i].vz) / 1000.0;

                // Distance to the nearest other body
                int other = (i == 0) ? 1 : 0;
                double ddx = bodies[i].x - bodies[other].x;
                double ddy = bodies[i].y - bodies[other].y;
                double ddz = bodies[i].z - bodies[other].z;
                double dist_AU = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz) / AU;
                double dist_Mkm = dist_AU * AU / 1e9;  // million km

                float tx = px + 10.0f;
                float ty = py + 10.0f;
                drawText(tx, ty, textScale,
                    bodyNames[i] + "  (Body " + std::to_string(i+1) + ")",
                    glm::vec4(c.r, c.g, c.b, 1.0f));
                ty += lineH + 2.0f;

                drawText(tx, ty, textScale,
                    "Speed : " + fmt(speed_kms, 2) + " km/s",
                    glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
                ty += lineH;

                drawText(tx, ty, textScale,
                    "Dist  : " + fmt(dist_AU, 3) + " AU",
                    glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
                ty += lineH;

                drawText(tx, ty, textScale,
                    "        " + fmt(dist_Mkm, 1) + " M km",
                    glm::vec4(0.75f, 0.75f, 0.75f, 1.0f));
                ty += lineH;

                drawText(tx, ty, textScale,
                    "Mass  : " + fmt(massSolars[i], 5) + " M_sun",
                    glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
