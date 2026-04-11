#pragma once

struct Body {
    double mass;
    double radius;

    // Position
    double x, y, z;

    // Velocity
    double vx, vy, vz;

    // Acceleration
    double ax, ay, az;

    // Rotation
    double spin_ax, spin_ay, spin_az;  // unit spin-axis vector (world space)
    double rotation_angle;              // current rotation phase (radians)
    double angular_velocity;            // rad/s; negative = retrograde

    // Oblateness
    double J2;    // second zonal harmonic coefficient (0 = perfect sphere)
    double R_eq;  // equatorial radius (metres) used in J2 force calculation

    // Tides
    double k2;       // tidal Love number (deformability)
    double tidal_Q;  // tidal quality factor (inverse dissipation; lower = more dissipative)
};
