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

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

const double G            = 6.674e-11;   
const double C_LIGHT      = 2.998e8;     
const double SOLAR_MASS   = 2.0e30;      
const double AU           = 1.496e11;    
const double RENDER_SCALE = 5.0 / AU;    
const double dt           = 86400.0;     
int          STEPS_PER_FRAME = 5;        
const int    TRAIL_LENGTH    = 300;      

float camYaw    = 0.0f;
float camPitch  = 35.0f;
float camDist   = 12.0f;
bool  mouseHeld = false;
double lastMouseX = 0.0, lastMouseY = 0.0;

bool paused        = false;
bool showPanel     = true;
bool showForces    = true;
bool showSpin      = true;
bool showLagrange  = true;
bool showCOM       = true;
int  numBodies   = 2;
std::vector<Body>         bodiesInitial;
std::deque<glm::vec3>     trails[8];
std::vector<std::string>  bodyNames;
double E0 = 0.0;
double L0 = 0.0;

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (action == GLFW_PRESS && key == GLFW_KEY_SPACE)
        paused = !paused;
    if (action == GLFW_PRESS && key == GLFW_KEY_I)
        showPanel = !showPanel;
    if (action == GLFW_PRESS && key == GLFW_KEY_F)
        showForces = !showForces;
    if (action == GLFW_PRESS && key == GLFW_KEY_S)
        showSpin = !showSpin;
    if (action == GLFW_PRESS && key == GLFW_KEY_L)
        showLagrange = !showLagrange;
    if (action == GLFW_PRESS && key == GLFW_KEY_B)
        showCOM = !showCOM;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_UP   || key == GLFW_KEY_RIGHT)
            STEPS_PER_FRAME = glm::clamp(STEPS_PER_FRAME + 1, 1, 100);
        if (key == GLFW_KEY_DOWN || key == GLFW_KEY_LEFT)
            STEPS_PER_FRAME = glm::clamp(STEPS_PER_FRAME - 1, 1, 100);
    }
}

void charCallback(GLFWwindow* window, unsigned int codepoint) {
    ImGui_ImplGlfw_CharCallback(window, codepoint);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if (ImGui::GetIO().WantCaptureMouse) return;
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
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    if (ImGui::GetIO().WantCaptureMouse) return;
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
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    if (ImGui::GetIO().WantCaptureMouse) return;
    camDist -= (float)yoffset * 1.5f;
    camDist = glm::clamp(camDist, 2.0f, 200.0f);
}

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

struct Preset {
    const char* name;
    double massSolar;
    double visualRadius;
    double orbitalRadiusAU;
    double axialTiltDeg;
    double spinPeriodDays;
    double J2;
    double R_eq;
    double k2;
    double tidal_Q;
    double atm_rho0;
    double atm_scale_height;
    double eccentricity;
};
const Preset PRESETS[] = {
    //                   name   mass     visR  orbAU   tilt   period      J2        R_eq      k2       Q       rho0   scaleH    ecc
    { "Sun",          1.0,       40.0,   0.0,   7.25,  25.380, 2.20e-7,  6.957e8,  0.028,  1.07e6,  0.0,    0.0,    0.0    },
    { "Jupiter",      9.54e-4,   28.0,   5.203,  3.13,   0.4135,1.474e-2, 7.149e7,  0.379,  3.56e4,  0.0,    0.0,    0.049  },
    { "Saturn",       2.84e-4,   23.0,   9.537, 26.73,   0.4440,1.630e-2, 6.027e7,  0.341,  1.68e4,  0.0,    0.0,    0.057  },
    { "Neptune",      5.15e-5,   16.0,  30.069, 28.32,   0.6713,3.411e-3, 2.476e7,  0.127,  9000.0,  0.0,    0.0,    0.010  },
    { "Uranus",       4.37e-5,   15.0,  19.191, 97.77,  -0.7183,3.343e-3, 2.556e7,  0.104,  8000.0,  0.0,    0.0,    0.046  },
    { "Earth",        3.00e-6,   12.0,   1.000, 23.44,   0.9973,1.083e-3, 6.378e6,  0.299,    12.0,  1.225,  8500.0, 0.017  },
    { "Venus",        2.45e-6,   11.0,   0.723,177.36,-243.025, 4.458e-6, 6.052e6,  0.295,    17.0,  65.0,  15900.0, 0.007  },
    { "Mars",         3.21e-7,    9.0,   1.524, 25.19,   1.026, 1.956e-3, 3.396e6,  0.169,    80.0,  0.0,    0.0,    0.093  },
    { "Mercury",      1.65e-7,    7.0,   0.387,  0.034, 58.646, 6.000e-5, 2.440e6,  0.100,    50.0,  0.0,    0.0,    0.206  },
    { "Neutron Star", 1.4,       14.0,   0.0,    0.0,    0.0233, 0.0,      1.0e4,   0.05,   1.0e13,  0.0,    0.0,    0.0    },
    { "Black Hole",  10.0,       18.0,   0.0,    0.0,    0.0,    0.0,      3.0e5,   0.0,    1.0e15,  0.0,    0.0,    0.0    },
    { "Custom",       0.0,        0.0,   0.0,   23.0,    1.0,   1.000e-3, 6.400e6,  0.300,   100.0,  0.0,    0.0,    0.0    },
};
const int NUM_PRESETS = 12;

const glm::vec3 BODY_COLORS[8] = {
    glm::vec3(1.0f, 1.0f, 1.0f),   
    glm::vec3(1.0f, 0.2f, 0.2f),   
    glm::vec3(0.2f, 0.8f, 1.0f),   
    glm::vec3(1.0f, 0.8f, 0.1f),   
    glm::vec3(0.2f, 1.0f, 0.3f),   
    glm::vec3(1.0f, 0.5f, 0.1f),   
    glm::vec3(0.9f, 0.3f, 1.0f),   
    glm::vec3(0.4f, 0.8f, 1.0f),   
};

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

glm::vec2 worldToScreen(glm::vec3 worldPos, glm::mat4 MVP, int width, int height) {
    glm::vec4 clip = MVP * glm::vec4(worldPos, 1.0f);
    glm::vec3 ndc  = glm::vec3(clip) / clip.w;
    return glm::vec2(
        (ndc.x + 1.0f) * 0.5f * width,
        (1.0f - ndc.y) * 0.5f * height   
    );
}

std::string fmt(double val, int decimals = 2) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(decimals) << val;
    return ss.str();
}

std::string fmtSci(double val) {
    if (val <= 0.0) return "0.00e+0";
    int exp = (int)std::floor(std::log10(val));
    double mantissa = val / std::pow(10.0, exp);
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << mantissa
       << "e" << (exp >= 0 ? "+" : "") << exp;
    return ss.str();
}

int main() {
    
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

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD\n";
        glfwTerminate();
        return -1;
    }

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    
    unsigned int planetShader = compileShader(planetVertSrc, planetFragSrc);
    unsigned int simpleShader = compileShader(simpleVertSrc, simpleFragSrc);

    int pMVP     = glGetUniformLocation(planetShader, "MVP");
    int pRadius  = glGetUniformLocation(planetShader, "pointRadius");
    int pCamDist = glGetUniformLocation(planetShader, "camDist");
    int pColor   = glGetUniformLocation(planetShader, "planetColor");
    int pAlpha   = glGetUniformLocation(planetShader, "alpha");
    int sMVP     = glGetUniformLocation(simpleShader, "MVP");
    int sColor   = glGetUniformLocation(simpleShader, "color");

    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.4f;
    ImGui::StyleColorsDark();
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 8.0f;
    style.FrameRounding     = 4.0f;
    style.ItemSpacing       = ImVec2(10, 8);
    style.WindowPadding     = ImVec2(18, 16);
    ImGui_ImplGlfw_InitForOpenGL(window, false);  
    ImGui_ImplOpenGL3_Init("#version 460");

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);

    
    int  setupNumBodies = 2;
    int  selectedPreset[8] = {0, 5, 0, 0, 0, 0, 0, 0};  
    double customMass[8]   = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    bool launched = false;

    
    const char* presetNames[NUM_PRESETS];
    for (int i = 0; i < NUM_PRESETS; i++) presetNames[i] = PRESETS[i].name;

    
    while (!glfwWindowShouldClose(window) && !launched) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        float winW = 520.0f;
        ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(winW, 0), ImGuiCond_Always);
        ImGui::Begin("Gravity Simulation", nullptr,
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove   |
                     ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoCollapse);

        ImGui::TextDisabled("Configure bodies, then launch.");
        ImGui::Spacing();
        ImGui::SliderInt("Number of Bodies", &setupNumBodies, 2, 8);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (int i = 0; i < setupNumBodies; i++) {
            ImGui::PushID(i);
            glm::vec3 c = BODY_COLORS[i];
            ImGui::ColorButton("##col",
                ImVec4(c.r, c.g, c.b, 1.0f),
                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                ImVec2(16, 16));
            ImGui::SameLine();

            char label[32];
            snprintf(label, sizeof(label), "Body %d", i + 1);
            ImGui::SetNextItemWidth(160);
            ImGui::Combo(label, &selectedPreset[i], presetNames, NUM_PRESETS);
            ImGui::SameLine();

            if (selectedPreset[i] == NUM_PRESETS - 1) {
                ImGui::SetNextItemWidth(130);
                ImGui::InputDouble("M\xe2\x98\x89##custom", &customMass[i], 0.0, 0.0, "%.5f");
                if (customMass[i] < 1e-8) customMass[i] = 1e-8;
                if (customMass[i] > 100.0) customMass[i] = 100.0;
            } else {
                ImGui::TextDisabled("%.5f M\xe2\x98\x89  %.3f AU",
                    PRESETS[selectedPreset[i]].massSolar,
                    PRESETS[selectedPreset[i]].orbitalRadiusAU);
            }
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float btnW = 220.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btnW) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.42f, 0.78f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.55f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.12f, 0.32f, 0.60f, 1.0f));
        if (ImGui::Button("Launch Simulation", ImVec2(btnW, 0)))
            launched = true;
        ImGui::PopStyleColor(3);

        ImGui::End();
        ImGui::Render();

        glClearColor(0.01f, 0.01f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    if (glfwWindowShouldClose(window)) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
        return 0;
    }

    
    numBodies = setupNumBodies;
    std::vector<double> massSolars(numBodies);
    std::vector<double> radii(numBodies);
    std::vector<double> orbitalRadii(numBodies);
    bodyNames.resize(numBodies);

    for (int i = 0; i < numBodies; i++) {
        int choice = selectedPreset[i];
        if (choice < NUM_PRESETS - 1) {
            massSolars[i]   = PRESETS[choice].massSolar;
            radii[i]        = PRESETS[choice].visualRadius;
            orbitalRadii[i] = PRESETS[choice].orbitalRadiusAU * AU;
            bodyNames[i]    = PRESETS[choice].name;
        } else {
            massSolars[i]   = customMass[i];
            radii[i]        = glm::clamp(20.0 * std::pow(customMass[i], 0.8), 8.0, 70.0);
            orbitalRadii[i] = 1.0 * AU;
            bodyNames[i]    = "Custom";
        }
    }

    std::vector<Body> bodies(numBodies);
    for (int i = 0; i < numBodies; i++) {
        bodies[i].mass   = massSolars[i] * SOLAR_MASS;
        bodies[i].radius = radii[i];
        bodies[i].ax = bodies[i].ay = bodies[i].az = 0.0;
        bodies[i].vx = bodies[i].vy = bodies[i].vz = 0.0;

        
        
        int    choice      = selectedPreset[i];
        double tiltDeg     = PRESETS[choice].axialTiltDeg;
        double periodDays  = PRESETS[choice].spinPeriodDays;
        double tiltRad     = tiltDeg * 3.14159265358979323846 / 180.0;
        double azimuth     = 2.0 * 3.14159265358979323846 * i / numBodies;
        bodies[i].spin_ax  = std::sin(tiltRad) * std::cos(azimuth);
        bodies[i].spin_ay  = std::cos(tiltRad);
        bodies[i].spin_az  = std::sin(tiltRad) * std::sin(azimuth);
        bodies[i].rotation_angle   = 0.0;
        bodies[i].angular_velocity = (periodDays != 0.0)
            ? 2.0 * 3.14159265358979323846 / (periodDays * 86400.0) : 0.0;

        bodies[i].J2               = PRESETS[choice].J2;
        bodies[i].R_eq             = PRESETS[choice].R_eq;
        bodies[i].k2               = PRESETS[choice].k2;
        bodies[i].tidal_Q          = PRESETS[choice].tidal_Q;
        bodies[i].atm_rho0         = PRESETS[choice].atm_rho0;
        bodies[i].atm_scale_height = PRESETS[choice].atm_scale_height;

        if (choice == NUM_PRESETS - 1) {
            bodies[i].R_eq = 6.378e6 * std::cbrt(massSolars[i] / 3.00e-6);
        }
    }

    double total_mass = 0.0;
    for (auto& b : bodies) total_mass += b.mass;

    
    const double PI   = 3.14159265358979323846;
    const double tilt = PI / 6.0;

    double centralMass = total_mass;
    {
        double best = 0.0;
        for (int i = 0; i < numBodies; i++) {
            if (orbitalRadii[i] < 0.001 * AU && bodies[i].mass > best) {
                best = bodies[i].mass;
                centralMass = bodies[i].mass;
            }
        }
    }

    for (int i = 0; i < numBodies; i++) {
        double angle = 2.0 * PI * i / numBodies;
        double ecc   = PRESETS[selectedPreset[i]].eccentricity;
        double a     = orbitalRadii[i];
        double r_peri = a * (1.0 - ecc);
        bodies[i].x = r_peri * std::cos(angle);
        bodies[i].y = r_peri * std::sin(angle) * std::cos(tilt);
        bodies[i].z = r_peri * std::sin(angle) * std::sin(tilt);
    }

    double comX = 0.0, comY = 0.0, comZ = 0.0;
    for (auto& b : bodies) {
        comX += b.mass * b.x;
        comY += b.mass * b.y;
        comZ += b.mass * b.z;
    }
    comX /= total_mass; comY /= total_mass; comZ /= total_mass;
    for (auto& b : bodies) { b.x -= comX; b.y -= comY; b.z -= comZ; }

    for (int i = 0; i < numBodies; i++) {
        if (orbitalRadii[i] < 0.001 * AU) continue;
        double angle = 2.0 * PI * i / numBodies;
        double ecc   = PRESETS[selectedPreset[i]].eccentricity;
        double a     = orbitalRadii[i];
        double r_peri = a * (1.0 - ecc);
        double v_peri = std::sqrt(G * centralMass * (1.0 + ecc) / r_peri);
        bodies[i].vx = -v_peri * std::sin(angle);
        bodies[i].vy =  v_peri * std::cos(angle) * std::cos(tilt);
        bodies[i].vz =  v_peri * std::cos(angle) * std::sin(tilt);
    }

    
    double pvx = 0.0, pvy = 0.0, pvz = 0.0;
    for (auto& b : bodies) { pvx += b.mass * b.vx; pvy += b.mass * b.vy; pvz += b.mass * b.vz; }
    for (auto& b : bodies) { b.vx -= pvx/total_mass; b.vy -= pvy/total_mass; b.vz -= pvz/total_mass; }

    
    for (int i = 0; i < numBodies; i++)
        for (int j = i+1; j < numBodies; j++)
            computeGravity(bodies[i], bodies[j], G, C_LIGHT);

    bodiesInitial = bodies;
    std::vector<double>      massSolarsInitial = massSolars;
    std::vector<double>      radiiInitial      = radii;
    std::vector<std::string> bodyNamesInitial  = bodyNames;

    {
        double E_init = 0.0;
        for (int i = 0; i < (int)bodies.size(); i++) {
            double vsq = bodies[i].vx*bodies[i].vx + bodies[i].vy*bodies[i].vy + bodies[i].vz*bodies[i].vz;
            E_init += 0.5 * bodies[i].mass * vsq;
        }
        for (int i = 0; i < (int)bodies.size(); i++) {
            for (int j = i+1; j < (int)bodies.size(); j++) {
                double ddx = bodies[j].x - bodies[i].x;
                double ddy = bodies[j].y - bodies[i].y;
                double ddz = bodies[j].z - bodies[i].z;
                double rr  = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
                if (rr > 0.0) E_init -= G * bodies[i].mass * bodies[j].mass / rr;
            }
        }
        E0 = E_init;

        double Lx = 0.0, Ly = 0.0, Lz = 0.0;
        for (int i = 0; i < (int)bodies.size(); i++) {
            Lx += bodies[i].mass * (bodies[i].y * bodies[i].vz - bodies[i].z * bodies[i].vy);
            Ly += bodies[i].mass * (bodies[i].z * bodies[i].vx - bodies[i].x * bodies[i].vz);
            Lz += bodies[i].mass * (bodies[i].x * bodies[i].vy - bodies[i].y * bodies[i].vx);
        }
        L0 = std::sqrt(Lx*Lx + Ly*Ly + Lz*Lz);
    }

    
    unsigned int planetVAO, planetVBO;
    glGenVertexArrays(1, &planetVAO);
    glGenBuffers(1, &planetVBO);
    glBindVertexArray(planetVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planetVBO);
    glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    
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

    
    auto starData = generateStars(2000);
    unsigned int starVAO, starVBO;
    glGenVertexArrays(1, &starVAO); glGenBuffers(1, &starVBO);
    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferData(GL_ARRAY_BUFFER, starData.size() * sizeof(float), starData.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    
    std::vector<float> gridVertices;
    int   gridSize   = 60;
    float gridStep   = 1.0f;
    float gridExtent = gridSize * gridStep;
    for (int i = -gridSize; i <= gridSize; i++) {
        float pos  = i * gridStep;
        
        gridVertices.insert(gridVertices.end(), { pos, 0.0f, -gridExtent });
        gridVertices.insert(gridVertices.end(), { pos, 0.0f,  gridExtent });
        gridVertices.insert(gridVertices.end(), { -gridExtent, 0.0f, pos });
        gridVertices.insert(gridVertices.end(), {  gridExtent, 0.0f, pos });
        
        gridVertices.insert(gridVertices.end(), { pos, -gridExtent, -gridExtent });
        gridVertices.insert(gridVertices.end(), { pos,  gridExtent, -gridExtent });
        
        float ypos = i * gridStep;
        gridVertices.insert(gridVertices.end(), { -gridExtent, ypos, -gridExtent });
        gridVertices.insert(gridVertices.end(), {  gridExtent, ypos, -gridExtent });
        
        gridVertices.insert(gridVertices.end(), { -gridExtent, -gridExtent, pos });
        gridVertices.insert(gridVertices.end(), { -gridExtent,  gridExtent, pos });
        
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

    
    
    unsigned int forceVAO, forceVBO;
    glGenVertexArrays(1, &forceVAO); glGenBuffers(1, &forceVBO);
    glBindVertexArray(forceVAO);
    glBindBuffer(GL_ARRAY_BUFFER, forceVBO);
    glBufferData(GL_ARRAY_BUFFER, 56 * 6 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    
    unsigned int spinVAO, spinVBO;
    glGenVertexArrays(1, &spinVAO); glGenBuffers(1, &spinVBO);
    glBindVertexArray(spinVAO);
    glBindBuffer(GL_ARRAY_BUFFER, spinVBO);
    glBufferData(GL_ARRAY_BUFFER, 52 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    
    
    unsigned int lagrangeVAO, lagrangeVBO;
    glGenVertexArrays(1, &lagrangeVAO); glGenBuffers(1, &lagrangeVBO);
    glBindVertexArray(lagrangeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lagrangeVBO);
    glBufferData(GL_ARRAY_BUFFER, 1120 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    unsigned int comVAO, comVBO;
    glGenVertexArrays(1, &comVAO); glGenBuffers(1, &comVBO);
    glBindVertexArray(comVAO);
    glBindBuffer(GL_ARRAY_BUFFER, comVBO);
    glBufferData(GL_ARRAY_BUFFER, 18 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    unsigned int uiVAO, uiVBO;
    glGenVertexArrays(1, &uiVAO); glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, 99999 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    
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

    
    auto drawText = [&](float x, float y, float scale, const std::string& text, glm::vec4 col) {
        static char buf[99999];
        int nq = stb_easy_font_print(0, 0, (char*)text.c_str(), nullptr, buf, sizeof(buf));

        
        struct SVertex { float x, y, z; unsigned char r, g, b, a; };
        SVertex* sv = (SVertex*)buf;

        
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

    
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        (float)screenW / (float)screenH,
        0.1f, 1000.0f
    );

    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        processInput(window);

        if (!ImGui::GetIO().WantCaptureKeyboard &&
            glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            bodies     = bodiesInitial;
            massSolars = massSolarsInitial;
            radii      = radiiInitial;
            bodyNames  = bodyNamesInitial;
            numBodies  = (int)bodies.size();
            for (int i = 0; i < 8; i++) trails[i].clear();
        }

        
        ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.80f);
        ImGui::Begin("Controls", nullptr,
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoCollapse);

        
        if (ImGui::Button(paused ? "Resume" : " Pause ", ImVec2(80, 0)))
            paused = !paused;
        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(60, 0))) {
            bodies     = bodiesInitial;
            massSolars = massSolarsInitial;
            radii      = radiiInitial;
            bodyNames  = bodyNamesInitial;
            numBodies  = (int)bodies.size();
            for (int i = 0; i < 8; i++) trails[i].clear();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(R to reset)");

        
        ImGui::SetNextItemWidth(170.0f);
        ImGui::SliderInt("##spf", &STEPS_PER_FRAME, 1, 100);
        ImGui::SameLine(0, 8);
        ImGui::Text("Speed  %d\xc3\x97", STEPS_PER_FRAME);

        ImGui::Checkbox("Forces (F)",   &showForces);   ImGui::SameLine(152);
        ImGui::Checkbox("Info (I)",     &showPanel);
        ImGui::Checkbox("Spin (S)",     &showSpin);     ImGui::SameLine(152);
        ImGui::Checkbox("Lagrange (L)", &showLagrange);
        ImGui::Checkbox("COM (B)",      &showCOM);

        ImGui::Separator();
        ImGui::TextDisabled("Body masses (solar masses)");
        ImGui::Spacing();

        for (int i = 0; i < numBodies; i++) {
            ImGui::PushID(i);
            glm::vec3 c = BODY_COLORS[i];
            ImGui::ColorButton("##col",
                ImVec4(c.r, c.g, c.b, 1.0f),
                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                ImVec2(14, 14));
            ImGui::SameLine();
            ImGui::Text("%s", bodyNames[i].c_str());

            float massF = (float)massSolars[i];
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::DragFloat("##mass", &massF,
                    massF * 0.005f, 1e-8f, 100.0f,
                    "%.6f M\xe2\x98\x89",
                    ImGuiSliderFlags_Logarithmic)) {
                massF = glm::clamp(massF, 1e-8f, 100.0f);
                massSolars[i]  = (double)massF;
                bodies[i].mass = massSolars[i] * SOLAR_MASS;
            }
            ImGui::PopID();
        }

        ImGui::End();

        
        
        if (!paused) {
            
            static const double w1 = 1.0 / (2.0 - std::cbrt(2.0));
            static const double w0 = 1.0 - 2.0 * w1;
            static const double c1 = w1 / 2.0;         
            static const double c2 = (w0 + w1) / 2.0;  
            static const double d1 = w1;                
            static const double d2 = w0;
            for (int step = 0; step < STEPS_PER_FRAME; step++) {
                double dt_step = dt;
                {
                    double min_scale = 1.0;
                    for (int i = 0; i < (int)bodies.size(); i++) {
                        for (int j = i+1; j < (int)bodies.size(); j++) {
                            double ddx = bodies[j].x - bodies[i].x;
                            double ddy = bodies[j].y - bodies[i].y;
                            double ddz = bodies[j].z - bodies[i].z;
                            double rr  = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
                            double s   = rr / (0.01 * AU);
                            if (s < min_scale) min_scale = s;
                        }
                    }
                    double scale_factor = (min_scale < 1.0) ? (1.0 / std::max(min_scale, 0.001)) : 1.0;
                    dt_step = dt / std::max(1.0, scale_factor);
                    if (dt_step < dt / 1000.0) dt_step = dt / 1000.0;
                    if (dt_step > dt)           dt_step = dt;
                }

                auto recomputeForces = [&]() {
                    for (auto& b : bodies) { b.ax = b.ay = b.az = 0.0; }
                    for (int i = 0; i < (int)bodies.size(); i++)
                        for (int j = i+1; j < (int)bodies.size(); j++) {
                            computeGravity(bodies[i], bodies[j], G, C_LIGHT);
                            computeDrag(bodies[i], bodies[j], dt_step);
                            computeDrag(bodies[j], bodies[i], dt_step);
                        }
                };

                for (auto& b : bodies) { b.x += b.vx*c1*dt_step; b.y += b.vy*c1*dt_step; b.z += b.vz*c1*dt_step; }
                recomputeForces();
                for (auto& b : bodies) { b.vx += b.ax*d1*dt_step; b.vy += b.ay*d1*dt_step; b.vz += b.az*d1*dt_step; }

                for (auto& b : bodies) { b.x += b.vx*c2*dt_step; b.y += b.vy*c2*dt_step; b.z += b.vz*c2*dt_step; }
                recomputeForces();
                for (auto& b : bodies) { b.vx += b.ax*d2*dt_step; b.vy += b.ay*d2*dt_step; b.vz += b.az*d2*dt_step; }

                for (auto& b : bodies) { b.x += b.vx*c2*dt_step; b.y += b.vy*c2*dt_step; b.z += b.vz*c2*dt_step; }
                recomputeForces();
                for (auto& b : bodies) { b.vx += b.ax*d1*dt_step; b.vy += b.ay*d1*dt_step; b.vz += b.az*d1*dt_step; }

                for (auto& b : bodies) { b.x += b.vx*c1*dt_step; b.y += b.vy*c1*dt_step; b.z += b.vz*c1*dt_step; }

                
                
                
                
                
                
                for (int i = 0; i < (int)bodies.size(); i++) {
                    for (int j = i+1; j < (int)bodies.size(); j++) {
                        double rx = bodies[i].x - bodies[j].x;
                        double ry = bodies[i].y - bodies[j].y;
                        double rz = bodies[i].z - bodies[j].z;
                        double dist = std::sqrt(rx*rx + ry*ry + rz*rz);
                        if (dist > 0.005 * AU) continue;

                        double mi = bodies[i].mass, mj = bodies[j].mass;
                        double mTot = mi + mj;

                        
                        double dvx = bodies[i].vx - bodies[j].vx;
                        double dvy = bodies[i].vy - bodies[j].vy;
                        double dvz = bodies[i].vz - bodies[j].vz;
                        double vRel = std::sqrt(dvx*dvx + dvy*dvy + dvz*dvz);

                        
                        double vEsc = std::sqrt(2.0 * G * mTot / std::max(dist, 1e6));

                        
                        
                        double cpx = ry*dvz - rz*dvy;
                        double cpy = rz*dvx - rx*dvz;
                        double cpz = rx*dvy - ry*dvx;
                        double bParam = (vRel > 0.0)
                            ? std::sqrt(cpx*cpx + cpy*cpy + cpz*cpz) / vRel : 0.0;
                        double xi = std::min(bParam / dist, 1.0);

                        
                        double comX  = (mi*bodies[i].x  + mj*bodies[j].x)  / mTot;
                        double comY  = (mi*bodies[i].y  + mj*bodies[j].y)  / mTot;
                        double comZ  = (mi*bodies[i].z  + mj*bodies[j].z)  / mTot;
                        double comVX = (mi*bodies[i].vx + mj*bodies[j].vx) / mTot;
                        double comVY = (mi*bodies[i].vy + mj*bodies[j].vy) / mTot;
                        double comVZ = (mi*bodies[i].vz + mj*bodies[j].vz) / mTot;

                        
                        double nx = rx / dist, ny = ry / dist, nz = rz / dist;

                        
                        bool hitAndRun    = (xi > 0.65 && vRel < 2.5 * vEsc);
                        bool catastrophic = (vRel >= 3.0 * vEsc);
                        bool partial      = (!hitAndRun && !catastrophic && vRel >= 1.5 * vEsc);
                        

                        
                        if (hitAndRun) {
                            double vDotN = dvx*nx + dvy*ny + dvz*nz;
                            if (vDotN < 0.0) {  
                                double e = glm::clamp(0.3 + 0.5 * xi, 0.0, 0.85);
                                double J = -(1.0 + e) * vDotN / (1.0/mi + 1.0/mj);
                                bodies[i].vx += J/mi * nx;
                                bodies[i].vy += J/mi * ny;
                                bodies[i].vz += J/mi * nz;
                                bodies[j].vx -= J/mj * nx;
                                bodies[j].vy -= J/mj * ny;
                                bodies[j].vz -= J/mj * nz;
                            }
                            continue;  
                        }

                        
                        double r3Total   = std::pow(radii[i], 3.0) + std::pow(radii[j], 3.0);
                        bool   spawnFrag = (numBodies < 8) && (catastrophic || partial);

                        double frag1Frac = 1.0;
                        if (catastrophic && spawnFrag) {
                            frag1Frac = 0.50 + 0.08 * xi;      
                        } else if (partial && spawnFrag) {
                            double er = glm::clamp(vRel / vEsc, 1.5, 3.0);
                            frag1Frac = 1.0 - 0.20 * (er - 1.5) / 1.5;  
                        }
                        double frag1Mass = mTot * frag1Frac;
                        double frag2Mass = mTot * (1.0 - frag1Frac);

                        
                        
                        
                        double vEject = 0.0;
                        if (spawnFrag) {
                            vEject = catastrophic
                                ? std::max(0.25 * vRel, 1.1 * vEsc)
                                : std::max(0.10 * vRel, 0.5 * vEsc);
                        }
                        double v1x = comVX + (frag2Mass/mTot) * vEject * nx;
                        double v1y = comVY + (frag2Mass/mTot) * vEject * ny;
                        double v1z = comVZ + (frag2Mass/mTot) * vEject * nz;
                        double v2x = comVX - (frag1Mass/mTot) * vEject * nx;
                        double v2y = comVY - (frag1Mass/mTot) * vEject * ny;
                        double v2z = comVZ - (frag1Mass/mTot) * vEject * nz;

                        
                        double r1new = std::cbrt(r3Total * frag1Frac);
                        double r2new = std::cbrt(r3Total * (1.0 - frag1Frac));

                        
                        const double FRAG_SEP = 0.003 * AU;

                        
                        std::string baseName = bodyNames[i] + "+" + bodyNames[j];
                        bodies[i].mass   = frag1Mass;
                        bodies[i].radius = r1new;
                        bodies[i].x  = spawnFrag ? comX + nx * FRAG_SEP : comX;
                        bodies[i].y  = spawnFrag ? comY + ny * FRAG_SEP : comY;
                        bodies[i].z  = spawnFrag ? comZ + nz * FRAG_SEP : comZ;
                        bodies[i].vx = v1x; bodies[i].vy = v1y; bodies[i].vz = v1z;
                        bodies[i].ax = bodies[i].ay = bodies[i].az = 0.0;
                        massSolars[i] = frag1Mass / SOLAR_MASS;
                        radii[i]      = r1new;
                        bodyNames[i]  = baseName;

                        
                        bodies.erase(bodies.begin() + j);
                        for (int k = j; k < numBodies - 1; k++) trails[k] = std::move(trails[k+1]);
                        trails[numBodies - 1].clear();
                        massSolars.erase(massSolars.begin() + j);
                        radii.erase(radii.begin() + j);
                        bodyNames.erase(bodyNames.begin() + j);
                        numBodies--;

                        
                        if (spawnFrag) {
                            Body frag{};
                            frag.mass   = frag2Mass;
                            frag.radius = r2new;
                            frag.x  = comX - nx * FRAG_SEP;
                            frag.y  = comY - ny * FRAG_SEP;
                            frag.z  = comZ - nz * FRAG_SEP;
                            frag.vx = v2x; frag.vy = v2y; frag.vz = v2z;

                            bodies.insert(bodies.begin() + j, frag);
                            
                            for (int k = numBodies; k > j; k--) trails[k] = std::move(trails[k-1]);
                            trails[j].clear();
                            massSolars.insert(massSolars.begin() + j, frag2Mass / SOLAR_MASS);
                            radii.insert(radii.begin() + j, r2new);
                            bodyNames.insert(bodyNames.begin() + j,
                                catastrophic ? "Debris" : "Ejecta");
                            numBodies++;
                            
                        } else {
                            j--;  
                        }
                    }
                }

                for (int i = 0; i < (int)bodies.size(); i++)
                    for (int j = i+1; j < (int)bodies.size(); j++)
                        computeTides(bodies[i], bodies[j], G, dt_step);

                for (int i = 0; i < numBodies; i++)
                    bodies[i].rotation_angle += bodies[i].angular_velocity * dt_step;
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

        glUniform4f(sColor, 0.05f, 0.25f, 0.45f, 0.6f);  
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
            glUniform1f(pRadius, (float)radii[i] * 3.0f);  
            glUniform1f(pAlpha, 0.12f);
            glDrawArrays(GL_POINTS, 0, 1);
            glUniform1f(pRadius, (float)radii[i]);          
            glUniform1f(pAlpha, 1.0f);
            glDrawArrays(GL_POINTS, 0, 1);
        }

        
        if (showSpin) {
            glUseProgram(simpleShader);
            glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(MVP));
            glBindVertexArray(spinVAO);
            glBindBuffer(GL_ARRAY_BUFFER, spinVBO);

            const int N_RING = 48;

            for (int i = 0; i < numBodies; i++) {
                glm::vec3 col  = BODY_COLORS[i];
                float cx = (float)(bodies[i].x * RENDER_SCALE);
                float cy = (float)(bodies[i].y * RENDER_SCALE);
                float cz = (float)(bodies[i].z * RENDER_SCALE);

                
                float halfLen  = (float)radii[i] * 0.005f * camDist;
                float ringR    = halfLen * 0.75f;

                glm::vec3 axis = glm::normalize(glm::vec3(
                    (float)bodies[i].spin_ax,
                    (float)bodies[i].spin_ay,
                    (float)bodies[i].spin_az));

                
                float axPts[6] = {
                    cx - axis.x*halfLen, cy - axis.y*halfLen, cz - axis.z*halfLen,
                    cx + axis.x*halfLen, cy + axis.y*halfLen, cz + axis.z*halfLen,
                };
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(axPts), axPts);
                glUniform4f(sColor, col.r, col.g, col.b, 0.9f);
                glDrawArrays(GL_LINES, 0, 2);

                
                
                glm::vec3 tmp = (std::abs(axis.y) < 0.9f) ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
                glm::vec3 u   = glm::normalize(glm::cross(axis, tmp));
                glm::vec3 v   = glm::cross(axis, u);

                
                float phi = (float)bodies[i].rotation_angle;
                glm::vec3 uR =  std::cos(phi)*u + std::sin(phi)*v;
                glm::vec3 vR = -std::sin(phi)*u + std::cos(phi)*v;

                std::vector<float> ring;
                ring.reserve((N_RING + 1) * 3);
                for (int k = 0; k <= N_RING; k++) {
                    float theta = 2.0f * 3.14159265f * k / N_RING;
                    glm::vec3 pt = glm::vec3(cx,cy,cz)
                        + ringR * (std::cos(theta)*uR + std::sin(theta)*vR);
                    ring.push_back(pt.x); ring.push_back(pt.y); ring.push_back(pt.z);
                }
                glBufferSubData(GL_ARRAY_BUFFER, 0, ring.size()*sizeof(float), ring.data());
                glUniform4f(sColor, col.r, col.g, col.b, 0.45f);
                glDrawArrays(GL_LINE_STRIP, 0, N_RING + 1);
            }
        }

        
        if (showForces && numBodies >= 2) {
            glUseProgram(simpleShader);
            glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(MVP));

            
            double maxF = 0.0;
            for (int i = 0; i < numBodies; i++)
                for (int j = i+1; j < numBodies; j++) {
                    double dx = bodies[j].x - bodies[i].x;
                    double dy = bodies[j].y - bodies[i].y;
                    double dz = bodies[j].z - bodies[i].z;
                    double r2 = dx*dx + dy*dy + dz*dz + 1e18;
                    double F  = G * bodies[i].mass * bodies[j].mass / r2;
                    if (F > maxF) maxF = F;
                }
            if (maxF <= 0.0) maxF = 1.0;

            const float maxArrowLen = 2.5f;
            const float headLen     = 0.22f;
            const float headWidth   = 0.10f;
            float s = (float)RENDER_SCALE;

            
            auto drawArrow3D = [&](glm::vec3 base, glm::vec3 dir, float len, glm::vec3 col) {
                glm::vec3 tip = base + dir * len;

                
                glm::vec3 up   = (std::abs(dir.y) < 0.9f) ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
                glm::vec3 perp = glm::normalize(glm::cross(dir, up));

                glm::vec3 hBase = tip - dir * headLen;
                glm::vec3 h1    = hBase + perp * headWidth;
                glm::vec3 h2    = hBase - perp * headWidth;

                float v[] = {
                    base.x, base.y, base.z,  tip.x, tip.y, tip.z,   
                    tip.x,  tip.y,  tip.z,   h1.x,  h1.y,  h1.z,   
                    tip.x,  tip.y,  tip.z,   h2.x,  h2.y,  h2.z,   
                };
                glBindVertexArray(forceVAO);
                glBindBuffer(GL_ARRAY_BUFFER, forceVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
                glUniform4f(sColor, col.r, col.g, col.b, 0.9f);
                glDrawArrays(GL_LINES, 0, 6);
            };

            for (int i = 0; i < numBodies; i++) {
                for (int j = i+1; j < numBodies; j++) {
                    double dx = bodies[j].x - bodies[i].x;
                    double dy = bodies[j].y - bodies[i].y;
                    double dz = bodies[j].z - bodies[i].z;
                    double r2 = dx*dx + dy*dy + dz*dz + 1e18;
                    double F  = G * bodies[i].mass * bodies[j].mass / r2;

                    float arrowLen = maxArrowLen * (float)std::sqrt(F / maxF);

                    glm::vec3 posI = { (float)(bodies[i].x*s), (float)(bodies[i].y*s), (float)(bodies[i].z*s) };
                    glm::vec3 posJ = { (float)(bodies[j].x*s), (float)(bodies[j].y*s), (float)(bodies[j].z*s) };
                    glm::vec3 dirIJ = glm::normalize(posJ - posI);

                    
                    drawArrow3D(posI,  dirIJ, arrowLen, BODY_COLORS[i]);
                    
                    drawArrow3D(posJ, -dirIJ, arrowLen, BODY_COLORS[j]);
                }
            }
        }

        
        if (showLagrange && numBodies >= 2) {
            
            struct LPoint { glm::vec3 pos; glm::vec4 color; const char* label; };
            std::vector<LPoint> lpoints;

            float s = (float)RENDER_SCALE;

            for (int i = 0; i < numBodies; i++) {
                for (int j = i+1; j < numBodies; j++) {
                    
                    
                    int pi = (bodies[i].mass >= bodies[j].mass) ? i : j;
                    int si = (bodies[i].mass >= bodies[j].mass) ? j : i;

                    double m1 = bodies[pi].mass, m2 = bodies[si].mass;
                    double mTot = m1 + m2;
                    double q = m2 / mTot;   

                    
                    double dx = bodies[si].x - bodies[pi].x;
                    double dy = bodies[si].y - bodies[pi].y;
                    double dz = bodies[si].z - bodies[pi].z;
                    double d  = std::sqrt(dx*dx + dy*dy + dz*dz);
                    if (d < 1e6) continue;  

                    
                    glm::dvec3 rHat(dx/d, dy/d, dz/d);

                    
                    double rHill = d * std::cbrt(q / 3.0);

                    
                    glm::vec3 pPos((float)(bodies[pi].x * s),
                                   (float)(bodies[pi].y * s),
                                   (float)(bodies[pi].z * s));

                    glm::vec3 rHatF((float)rHat.x, (float)rHat.y, (float)rHat.z);
                    float df = (float)(d * s);
                    float rHillF = (float)(rHill * s);

                    
                    glm::vec3 posL1 = pPos + rHatF * (df - rHillF);
                    
                    glm::vec3 posL2 = pPos + rHatF * (df + rHillF);
                    
                    float L3dist = (float)(d * (1.0 + 5.0*q/12.0) * s);
                    glm::vec3 posL3 = pPos - rHatF * L3dist;

                    
                    
                    
                    double rvx = bodies[si].vx - bodies[pi].vx;
                    double rvy = bodies[si].vy - bodies[pi].vy;
                    double rvz = bodies[si].vz - bodies[pi].vz;
                    glm::dvec3 rVel(rvx, rvy, rvz);
                    glm::dvec3 orbitNorm = glm::cross(rHat, rVel);
                    double oNormLen = glm::length(orbitNorm);

                    
                    glm::vec3 pairCol = glm::mix(BODY_COLORS[pi], BODY_COLORS[si], 0.5f);
                    glm::vec4 col4(pairCol.r, pairCol.g, pairCol.b, 0.85f);

                    lpoints.push_back({ posL1, col4, "L1" });
                    lpoints.push_back({ posL2, col4, "L2" });
                    lpoints.push_back({ posL3, col4, "L3" });

                    if (oNormLen > 1e-30) {
                        glm::dvec3 nHat = orbitNorm / oNormLen;
                        
                        glm::dvec3 perpHat = glm::cross(nHat, rHat);

                        
                        
                        
                        double sin60 = std::sqrt(3.0) / 2.0;
                        glm::dvec3 L4off = rHat * 0.5 + perpHat * sin60;
                        glm::dvec3 L5off = rHat * 0.5 - perpHat * sin60;

                        glm::vec3 posL4(
                            (float)(bodies[pi].x * s + L4off.x * d * s),
                            (float)(bodies[pi].y * s + L4off.y * d * s),
                            (float)(bodies[pi].z * s + L4off.z * d * s));
                        glm::vec3 posL5(
                            (float)(bodies[pi].x * s + L5off.x * d * s),
                            (float)(bodies[pi].y * s + L5off.y * d * s),
                            (float)(bodies[pi].z * s + L5off.z * d * s));

                        lpoints.push_back({ posL4, col4, "L4" });
                        lpoints.push_back({ posL5, col4, "L5" });
                    }
                }
            }

            
            glUseProgram(simpleShader);
            glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(MVP));
            glBindVertexArray(lagrangeVAO);
            glBindBuffer(GL_ARRAY_BUFFER, lagrangeVBO);

            float crossHalf = 0.12f * (camDist / 12.0f);  

            for (auto& lp : lpoints) {
                float v[] = {
                    lp.pos.x - crossHalf, lp.pos.y,             lp.pos.z,
                    lp.pos.x + crossHalf, lp.pos.y,             lp.pos.z,
                    lp.pos.x,             lp.pos.y - crossHalf, lp.pos.z,
                    lp.pos.x,             lp.pos.y + crossHalf, lp.pos.z,
                    lp.pos.x,             lp.pos.y,             lp.pos.z - crossHalf,
                    lp.pos.x,             lp.pos.y,             lp.pos.z + crossHalf,
                };
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
                glUniform4f(sColor, lp.color.r, lp.color.g, lp.color.b, lp.color.a);
                glDrawArrays(GL_LINES, 0, 6);
            }

            
            glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
            glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(ortho));

            for (auto& lp : lpoints) {
                glm::vec2 sp = worldToScreen(lp.pos, MVP, screenW, screenH);
                
                if (sp.x < -100 || sp.x > screenW + 100 || sp.y < -100 || sp.y > screenH + 100) continue;
                drawText(sp.x + 6.0f, sp.y - 8.0f, 1.5f, lp.label,
                    glm::vec4(lp.color.r, lp.color.g, lp.color.b, 0.9f));
            }

            
            glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(MVP));
        }

        
        if (showPanel) {
            glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
            glUseProgram(simpleShader);
            glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(ortho));

            const float panelW    = 280.0f;
            const float panelH    = 130.0f;
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

                float px = panelX;
                float py = 18.0f + i * (panelH + panelGap);
                glm::vec3 c = BODY_COLORS[i];

                drawRect(px, py, panelW, panelH, glm::vec4(0.04f, 0.04f, 0.10f, 0.88f));
                drawRect(px, py, 4.0f, panelH, glm::vec4(c.r, c.g, c.b, 1.0f));

                
                double speed_kms = std::sqrt(
                    bodies[i].vx*bodies[i].vx +
                    bodies[i].vy*bodies[i].vy +
                    bodies[i].vz*bodies[i].vz) / 1000.0;

                
                int other = (i == 0) ? 1 : 0;
                double ddx = bodies[i].x - bodies[other].x;
                double ddy = bodies[i].y - bodies[other].y;
                double ddz = bodies[i].z - bodies[other].z;
                double dist_AU = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz) / AU;
                double dist_Mkm = dist_AU * AU / 1e9;  

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
                ty += lineH;

                double Fmag = bodies[i].mass * std::sqrt(
                    bodies[i].ax*bodies[i].ax +
                    bodies[i].ay*bodies[i].ay +
                    bodies[i].az*bodies[i].az);
                drawText(tx, ty, textScale,
                    "Force : " + fmtSci(Fmag) + " N",
                    glm::vec4(0.8f, 0.65f, 1.0f, 1.0f));
            }
        }

        if (showPanel) {
            double E_cur = 0.0;
            for (int i = 0; i < (int)bodies.size(); i++) {
                double vsq = bodies[i].vx*bodies[i].vx + bodies[i].vy*bodies[i].vy + bodies[i].vz*bodies[i].vz;
                E_cur += 0.5 * bodies[i].mass * vsq;
            }
            for (int i = 0; i < (int)bodies.size(); i++)
                for (int j = i+1; j < (int)bodies.size(); j++) {
                    double ddx = bodies[j].x - bodies[i].x;
                    double ddy = bodies[j].y - bodies[i].y;
                    double ddz = bodies[j].z - bodies[i].z;
                    double rr  = std::sqrt(ddx*ddx + ddy*ddy + ddz*ddz);
                    if (rr > 0.0) E_cur -= G * bodies[i].mass * bodies[j].mass / rr;
                }
            double Lx_c = 0.0, Ly_c = 0.0, Lz_c = 0.0;
            for (int i = 0; i < (int)bodies.size(); i++) {
                Lx_c += bodies[i].mass * (bodies[i].y * bodies[i].vz - bodies[i].z * bodies[i].vy);
                Ly_c += bodies[i].mass * (bodies[i].z * bodies[i].vx - bodies[i].x * bodies[i].vz);
                Lz_c += bodies[i].mass * (bodies[i].x * bodies[i].vy - bodies[i].y * bodies[i].vx);
            }
            double L_cur   = std::sqrt(Lx_c*Lx_c + Ly_c*Ly_c + Lz_c*Lz_c);
            double E_drift = (E0 != 0.0) ? (E_cur - E0) / std::abs(E0) * 100.0 : 0.0;
            double L_drift = (L0 != 0.0) ? (L_cur - L0) / std::abs(L0) * 100.0 : 0.0;

            const float eH     = 90.0f;
            const float panelW = 280.0f;
            float panelX = (float)screenW - panelW - 18.0f;
            float epy    = (float)screenH - eH - 18.0f;

            glm::mat4 ortho2 = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
            glUseProgram(simpleShader);
            glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(ortho2));

            drawRect(panelX, epy, panelW, eH, glm::vec4(0.04f, 0.04f, 0.10f, 0.88f));
            drawRect(panelX, epy, 4.0f, eH, glm::vec4(0.6f, 0.6f, 1.0f, 1.0f));

            const float ts = 2.0f, lh = 18.0f;
            float tx = panelX + 10.0f;
            float ty = epy + 8.0f;
            drawText(tx, ty, ts, "E:  " + fmtSci(std::abs(E_cur)) + " J",
                glm::vec4(0.8f, 0.8f, 1.0f, 1.0f));
            ty += lh;
            drawText(tx, ty, ts, "    drift " + fmt(E_drift, 5) + "%",
                glm::vec4(0.65f, 0.65f, 0.9f, 1.0f));
            ty += lh + 2.0f;
            drawText(tx, ty, ts, "|L|: " + fmtSci(L_cur) + " kg m2/s",
                glm::vec4(0.8f, 1.0f, 0.8f, 1.0f));
            ty += lh;
            drawText(tx, ty, ts, "     drift " + fmt(L_drift, 5) + "%",
                glm::vec4(0.6f, 0.85f, 0.6f, 1.0f));
        }

        if (showCOM && numBodies >= 2) {
            double tm = 0.0;
            for (auto& b : bodies) tm += b.mass;
            double cx = 0.0, cy_c = 0.0, cz = 0.0;
            for (auto& b : bodies) {
                cx   += b.mass * b.x;
                cy_c += b.mass * b.y;
                cz   += b.mass * b.z;
            }
            cx /= tm; cy_c /= tm; cz /= tm;

            float s = (float)RENDER_SCALE;
            float cpx = (float)(cx * s);
            float cpy = (float)(cy_c * s);
            float cpz = (float)(cz * s);
            float crossH = 0.14f * (camDist / 12.0f);

            glUseProgram(simpleShader);
            glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(MVP));
            glBindVertexArray(comVAO);
            glBindBuffer(GL_ARRAY_BUFFER, comVBO);

            float cv[] = {
                cpx - crossH, cpy,          cpz,
                cpx + crossH, cpy,          cpz,
                cpx,          cpy - crossH, cpz,
                cpx,          cpy + crossH, cpz,
                cpx,          cpy,          cpz - crossH,
                cpx,          cpy,          cpz + crossH,
            };
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cv), cv);
            glUniform4f(sColor, 1.0f, 1.0f, 0.3f, 0.9f);
            glDrawArrays(GL_LINES, 0, 6);

            glm::mat4 ortho3 = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
            glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(ortho3));
            glm::vec2 comSP = worldToScreen(glm::vec3(cpx, cpy, cpz), MVP, screenW, screenH);
            drawText(comSP.x + 6.0f, comSP.y - 8.0f, 1.5f, "COM",
                glm::vec4(1.0f, 1.0f, 0.3f, 0.9f));

            glUniformMatrix4fv(sMVP, 1, GL_FALSE, glm::value_ptr(MVP));
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
