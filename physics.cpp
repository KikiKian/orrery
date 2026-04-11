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

    // J2 oblateness perturbation
    // Derived from the gradient of the J2 gravitational potential:
    //   Φ_J2 = G·M·J2·R² / (2r³) · (3(ẑ·r̂)² − 1)
    // Acceleration on the orbiting body:
    //   a_J2 = (3·G·M·J2·R²) / (2r⁵) · [(5ζ²−1)·r⃗ − 2r·ζ·ẑ]
    // where ζ = ẑ·r̂ (cosine of angle between spin axis and position vector).
    // Effect: nodal regression and apsidal precession for non-equatorial orbits.
    // Scales as (R_eq/r)², so it matters most for gas giants and close orbits.
    double r5 = r_soft * r_soft * r_soft * r_soft * r_soft;

    // p1's oblateness acting on p2  (r⃗ from p1→p2 = (dx,dy,dz))
    if (p1.J2 > 0.0) {
        double K1   = 1.5 * G * p1.mass * p1.J2 * p1.R_eq * p1.R_eq;
        double zeta = (p1.spin_ax*dx + p1.spin_ay*dy + p1.spin_az*dz) / r_soft;
        double fr   = (5.0*zeta*zeta - 1.0) * K1 / r5;
        double fz   = -2.0 * r_soft * zeta  * K1 / r5;
        double j2ax = fr*dx + fz*p1.spin_ax;
        double j2ay = fr*dy + fz*p1.spin_ay;
        double j2az = fr*dz + fz*p1.spin_az;
        p2.ax += j2ax;
        p2.ay += j2ay;
        p2.az += j2az;
        p1.ax -= j2ax * p2.mass / p1.mass;  // Newton's 3rd
        p1.ay -= j2ay * p2.mass / p1.mass;
        p1.az -= j2az * p2.mass / p1.mass;
    }

    // p2's oblateness acting on p1  (r⃗ from p2→p1 = (−dx,−dy,−dz))
    if (p2.J2 > 0.0) {
        double K2   = 1.5 * G * p2.mass * p2.J2 * p2.R_eq * p2.R_eq;
        double zeta = -(p2.spin_ax*dx + p2.spin_ay*dy + p2.spin_az*dz) / r_soft;
        double fr   = (5.0*zeta*zeta - 1.0) * K2 / r5;
        double fz   = -2.0 * r_soft * zeta  * K2 / r5;
        double j2ax = fr*(-dx) + fz*p2.spin_ax;
        double j2ay = fr*(-dy) + fz*p2.spin_ay;
        double j2az = fr*(-dz) + fz*p2.spin_az;
        p1.ax += j2ax;
        p1.ay += j2ay;
        p1.az += j2az;
        p2.ax -= j2ax * p1.mass / p2.mass;  // Newton's 3rd
        p2.ay -= j2ay * p1.mass / p2.mass;
        p2.az -= j2az * p1.mass / p2.mass;
    }
}

// Tidal forces using the constant time-lag (CTL) model.
//
// For each body i deformed by body j, the tidal torque is:
//   T_i = -C_i * (Ω_i - n_orb)
//   C_i = (3/2) * G * mj² * k2_i * R_eq_i⁵ / (Q_i * r⁶)
//
// This torque:
//   1. Changes body i's spin rate:  dΩ_i = T_i / I_i * dt  (I = 0.4·m·R_eq²)
//   2. Reacts on the orbit as a tangential velocity kick (angular momentum conservation):
//      Δv_tang_j = -T_i * dt / (r · mj)   (prograde direction for body j)
//      Δv_tang_i = +T_i * dt / (r · mi)   (opposite, by Newton's 3rd)
//
// Physical example: Earth spins faster than the Moon orbits →
// T < 0 decelerates Earth's spin; orbit gains angular momentum; Moon recedes.
void computeTides(Body& p1, Body& p2, double G, double dt) {
    if (p1.R_eq <= 0.0 && p2.R_eq <= 0.0) return;

    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    double dz = p2.z - p1.z;
    double r2 = dx*dx + dy*dy + dz*dz;
    double r  = std::sqrt(r2);
    if (r < 1e6) return;

    double r3 = r2 * r;
    double r6 = r3 * r3;

    double n_orb = std::sqrt(G * (p1.mass + p2.mass) / r3);

    // Relative velocity (p2 − p1)
    double dvx = p2.vx - p1.vx, dvy = p2.vy - p1.vy, dvz = p2.vz - p1.vz;

    // Orbital angular momentum direction L̂ = normalize((p2−p1) × (v2−v1))
    double Lx = dy*dvz - dz*dvy;
    double Ly = dz*dvx - dx*dvz;
    double Lz = dx*dvy - dy*dvx;
    double Llen = std::sqrt(Lx*Lx + Ly*Ly + Lz*Lz);
    if (Llen < 1e-20) return;
    Lx /= Llen; Ly /= Llen; Lz /= Llen;

    // r̂ = unit vector from p1 to p2
    double rx = dx/r, ry = dy/r, rz = dz/r;

    // t̂ = L̂ × r̂  (prograde tangential direction for p2)
    double tx = Ly*rz - Lz*ry;
    double ty = Lz*rx - Lx*rz;
    double tz = Lx*ry - Ly*rx;

    // ---- Tides raised on p1 by p2 ----
    if (p1.k2 > 0.0 && p1.tidal_Q > 0.0 && p1.R_eq > 0.0) {
        // Spin component of p1 along the orbital axis
        double cos1  = p1.spin_ax*Lx + p1.spin_ay*Ly + p1.spin_az*Lz;
        double spin1 = p1.angular_velocity * cos1;  // effective spin rate along L̂

        double R5_1 = p1.R_eq * p1.R_eq * p1.R_eq * p1.R_eq * p1.R_eq;
        double C1   = 1.5 * G * p2.mass * p2.mass * p1.k2 * R5_1 / (p1.tidal_Q * r6);
        double T1   = -C1 * (spin1 - n_orb);

        // Spin update: dΩ = T / I  where I = 0.4·m·R²
        double I1 = 0.4 * p1.mass * p1.R_eq * p1.R_eq;
        if (std::abs(cos1) > 1e-6)
            p1.angular_velocity += (T1 / I1 * dt) / cos1;

        // Orbital velocity kick (tangential, conserving total angular momentum)
        double dv1 = T1 * dt / r;
        p2.vx -= (dv1 / p2.mass) * tx;
        p2.vy -= (dv1 / p2.mass) * ty;
        p2.vz -= (dv1 / p2.mass) * tz;
        p1.vx += (dv1 / p1.mass) * tx;
        p1.vy += (dv1 / p1.mass) * ty;
        p1.vz += (dv1 / p1.mass) * tz;
    }

    // ---- Tides raised on p2 by p1 ----
    if (p2.k2 > 0.0 && p2.tidal_Q > 0.0 && p2.R_eq > 0.0) {
        double cos2  = p2.spin_ax*Lx + p2.spin_ay*Ly + p2.spin_az*Lz;
        double spin2 = p2.angular_velocity * cos2;

        double R5_2 = p2.R_eq * p2.R_eq * p2.R_eq * p2.R_eq * p2.R_eq;
        double C2   = 1.5 * G * p1.mass * p1.mass * p2.k2 * R5_2 / (p2.tidal_Q * r6);
        double T2   = -C2 * (spin2 - n_orb);

        double I2 = 0.4 * p2.mass * p2.R_eq * p2.R_eq;
        if (std::abs(cos2) > 1e-6)
            p2.angular_velocity += (T2 / I2 * dt) / cos2;

        // p1 is the perturber here; tangential direction for p1 is -t̂
        double dv2 = T2 * dt / r;
        p1.vx -= (dv2 / p1.mass) * tx;
        p1.vy -= (dv2 / p1.mass) * ty;
        p1.vz -= (dv2 / p1.mass) * tz;
        p2.vx += (dv2 / p2.mass) * tx;
        p2.vy += (dv2 / p2.mass) * ty;
        p2.vz += (dv2 / p2.mass) * tz;
    }
}

