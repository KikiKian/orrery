#include "physics.h"
#include <cmath>

void computeGravity(Body& p1, Body& p2, double G, double c) {

    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    double dz = p2.z - p1.z;

    double r = std::sqrt(dx*dx + dy*dy + dz*dz);

    // Softening factor — prevents force going to infinity if bodies get too close
    double softening = 1e9;
    double r_soft = std::sqrt(r*r + softening*softening);

    // Newtonian gravity
    double F   = G * (p1.mass * p2.mass) / (r_soft * r_soft);
    double Fdr = F / r_soft;  // F / r, reused below

    p1.ax += Fdr * dx / p1.mass;
    p1.ay += Fdr * dy / p1.mass;
    p1.az += Fdr * dz / p1.mass;

    p2.ax -= Fdr * dx / p2.mass;
    p2.ay -= Fdr * dy / p2.mass;
    p2.az -= Fdr * dz / p2.mass;

    // 1PN Einstein-Infeld-Hoffmann correction
    // Produces orbital precession (e.g. Mercury's 43"/century) for close/fast orbits.
    // Correction is O(v²/c²) ≈ 1e-8 for Earth, larger near compact or very massive bodies.
    // Reference: Will (2014), EIH equations of motion.
    //
    // n̂ = unit vector from p1 toward p2
    double c2 = c * c;
    double nx = dx / r_soft, ny = dy / r_soft, nz = dz / r_soft;

    double v1sq = p1.vx*p1.vx + p1.vy*p1.vy + p1.vz*p1.vz;
    double v2sq = p2.vx*p2.vx + p2.vy*p2.vy + p2.vz*p2.vz;
    double v1v2 = p1.vx*p2.vx + p1.vy*p2.vy + p1.vz*p2.vz;
    double nv1  = nx*p1.vx + ny*p1.vy + nz*p1.vz;  // n̂ · v1
    double nv2  = nx*p2.vx + ny*p2.vy + nz*p2.vz;  // n̂ · v2

    double Gm1_r = G * p1.mass / r_soft;
    double Gm2_r = G * p2.mass / r_soft;

    // Correction to a1 — n̂ points from p1 toward p2
    double s1  = v1sq + 2.0*v2sq - 4.0*v1v2 - 1.5*nv2*nv2 + 5.0*Gm1_r + 4.0*Gm2_r;
    double cv1 = 4.0*nv1 - 3.0*nv2;
    double pn1 = G * p2.mass / (r_soft * r_soft * c2);
    p1.ax += pn1 * (nx * s1 + (p1.vx - p2.vx) * cv1);
    p1.ay += pn1 * (ny * s1 + (p1.vy - p2.vy) * cv1);
    p1.az += pn1 * (nz * s1 + (p1.vz - p2.vz) * cv1);

    // Correction to a2 — n̂_21 = -n̂, so dot products flip sign
    double s2  = v2sq + 2.0*v1sq - 4.0*v1v2 - 1.5*nv1*nv1 + 5.0*Gm2_r + 4.0*Gm1_r;
    double cv2 = -4.0*nv2 + 3.0*nv1;  // 4*(n̂_21·v2) - 3*(n̂_21·v1)
    double pn2 = G * p1.mass / (r_soft * r_soft * c2);
    p2.ax += pn2 * (-nx * s2 + (p2.vx - p1.vx) * cv2);
    p2.ay += pn2 * (-ny * s2 + (p2.vy - p1.vy) * cv2);
    p2.az += pn2 * (-nz * s2 + (p2.vz - p1.vz) * cv2);
}

