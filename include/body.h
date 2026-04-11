#pragma once

struct Body {
    double mass;
    double radius;


    double x, y, z;


    double vx, vy, vz;


    double ax, ay, az;


    double spin_ax, spin_ay, spin_az;
    double rotation_angle;
    double angular_velocity;


    double J2;
    double R_eq;


    double k2;
    double tidal_Q;


    double atm_scale_height;
    double atm_rho0;
};
