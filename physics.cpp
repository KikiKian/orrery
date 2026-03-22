#include "physics.h"
#include <cmath>

void computeGravity(Body& p1, Body& p2, double G) {

    //finding difference between the planets
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    double dz = p2.z - p1.z;

    //calculating distance (3D pythagoras)
    double r = std::sqrt(dx*dx + dy*dy + dz*dz);

    // Softening factor — prevents force going to infinity if bodies get too close
    double softening = 1e9;
    double r_soft = std::sqrt(r*r + softening*softening);

    //calculating the grav force
    double F = G * (p1.mass * p2.mass) / (r_soft * r_soft);

    //calculating force direction for each axis
    double ax = F * (dx / r_soft);
    double ay = F * (dy / r_soft);
    double az = F * (dz / r_soft);

    // Newton's Third Law — equal and opposite forces
    p1.ax += ax / p1.mass;
    p1.ay += ay / p1.mass;
    p1.az += az / p1.mass;

    p2.ax -= ax / p2.mass;
    p2.ay -= ay / p2.mass;
    p2.az -= az / p2.mass;
}

void updateBody(Body& b, double dt) {
    // Symplectic Euler — much more stable than regular Euler for orbital mechanics
    // Update velocity first
    b.vx += b.ax * dt;
    b.vy += b.ay * dt;
    b.vz += b.az * dt;

    // Update position using NEW velocity (key difference from regular Euler)
    b.x += b.vx * dt;
    b.y += b.vy * dt;
    b.z += b.vz * dt;

    // Reset acceleration for next frame
    b.ax = 0.0;
    b.ay = 0.0;
    b.az = 0.0;
}
