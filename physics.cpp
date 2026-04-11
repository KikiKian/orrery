#include "physics.h"
#include <cmath>
#include <algorithm>

void computeGravity(Body& p1, Body& p2, double G, double c) {

    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    double dz = p2.z - p1.z;

    double r = std::sqrt(dx*dx + dy*dy + dz*dz);

    
    double softening = 1e9;
    double r_soft = std::sqrt(r*r + softening*softening);

    
    double F   = G * (p1.mass * p2.mass) / (r_soft * r_soft);
    double Fdr = F / r_soft;  

    p1.ax += Fdr * dx / p1.mass;
    p1.ay += Fdr * dy / p1.mass;
    p1.az += Fdr * dz / p1.mass;

    p2.ax -= Fdr * dx / p2.mass;
    p2.ay -= Fdr * dy / p2.mass;
    p2.az -= Fdr * dz / p2.mass;

    
    
    
    
    
    
    double c2 = c * c;
    double nx = dx / r_soft, ny = dy / r_soft, nz = dz / r_soft;

    double v1sq = p1.vx*p1.vx + p1.vy*p1.vy + p1.vz*p1.vz;
    double v2sq = p2.vx*p2.vx + p2.vy*p2.vy + p2.vz*p2.vz;
    double v1v2 = p1.vx*p2.vx + p1.vy*p2.vy + p1.vz*p2.vz;
    double nv1  = nx*p1.vx + ny*p1.vy + nz*p1.vz;  
    double nv2  = nx*p2.vx + ny*p2.vy + nz*p2.vz;  

    double Gm1_r = G * p1.mass / r_soft;
    double Gm2_r = G * p2.mass / r_soft;

    
    double s1  = v1sq + 2.0*v2sq - 4.0*v1v2 - 1.5*nv2*nv2 + 5.0*Gm1_r + 4.0*Gm2_r;
    double cv1 = 4.0*nv1 - 3.0*nv2;
    double pn1 = G * p2.mass / (r_soft * r_soft * c2);
    p1.ax += pn1 * (nx * s1 + (p1.vx - p2.vx) * cv1);
    p1.ay += pn1 * (ny * s1 + (p1.vy - p2.vy) * cv1);
    p1.az += pn1 * (nz * s1 + (p1.vz - p2.vz) * cv1);

    
    double s2  = v2sq + 2.0*v1sq - 4.0*v1v2 - 1.5*nv1*nv1 + 5.0*Gm2_r + 4.0*Gm1_r;
    double cv2 = -4.0*nv2 + 3.0*nv1;  
    double pn2 = G * p1.mass / (r_soft * r_soft * c2);
    p2.ax += pn2 * (-nx * s2 + (p2.vx - p1.vx) * cv2);
    p2.ay += pn2 * (-ny * s2 + (p2.vy - p1.vy) * cv2);
    p2.az += pn2 * (-nz * s2 + (p2.vz - p1.vz) * cv2);

    
    
    
    
    
    
    
    
    double r5 = r_soft * r_soft * r_soft * r_soft * r_soft;

    
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
        p1.ax -= j2ax * p2.mass / p1.mass;  
        p1.ay -= j2ay * p2.mass / p1.mass;
        p1.az -= j2az * p2.mass / p1.mass;
    }

    
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
        p2.ax -= j2ax * p1.mass / p2.mass;  
        p2.ay -= j2ay * p1.mass / p2.mass;
        p2.az -= j2az * p1.mass / p2.mass;
    }
}

void computeDrag(Body& satellite, const Body& planet, double dt) {
    if (planet.atm_rho0 <= 0.0 || planet.atm_scale_height <= 0.0 || planet.R_eq <= 0.0) return;

    double dx = satellite.x - planet.x;
    double dy = satellite.y - planet.y;
    double dz = satellite.z - planet.z;
    double r  = std::sqrt(dx*dx + dy*dy + dz*dz);

    double h = r - planet.R_eq;
    if (h < 0.0) h = 0.0;

    double max_h = 10.0 * planet.atm_scale_height;
    if (h > max_h) return;

    double rho = planet.atm_rho0 * std::exp(-h / planet.atm_scale_height);

    double vrx = satellite.vx - planet.vx;
    double vry = satellite.vy - planet.vy;
    double vrz = satellite.vz - planet.vz;
    double vrel = std::sqrt(vrx*vrx + vry*vry + vrz*vrz);
    if (vrel < 1e-10) return;

    const double Cd = 2.2;
    const double AmRatio = 0.01;

    double a_drag = 0.5 * Cd * AmRatio * rho * vrel * vrel;

    satellite.ax -= a_drag * (vrx / vrel);
    satellite.ay -= a_drag * (vry / vrel);
    satellite.az -= a_drag * (vrz / vrel);
}

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

    
    double dvx = p2.vx - p1.vx, dvy = p2.vy - p1.vy, dvz = p2.vz - p1.vz;

    
    double Lx = dy*dvz - dz*dvy;
    double Ly = dz*dvx - dx*dvz;
    double Lz = dx*dvy - dy*dvx;
    double Llen = std::sqrt(Lx*Lx + Ly*Ly + Lz*Lz);
    if (Llen < 1e-20) return;
    Lx /= Llen; Ly /= Llen; Lz /= Llen;

    
    double rx = dx/r, ry = dy/r, rz = dz/r;

    
    double tx = Ly*rz - Lz*ry;
    double ty = Lz*rx - Lx*rz;
    double tz = Lx*ry - Ly*rx;

    
    if (p1.k2 > 0.0 && p1.tidal_Q > 0.0 && p1.R_eq > 0.0) {
        
        double cos1  = p1.spin_ax*Lx + p1.spin_ay*Ly + p1.spin_az*Lz;
        double spin1 = p1.angular_velocity * cos1;  

        double R5_1 = p1.R_eq * p1.R_eq * p1.R_eq * p1.R_eq * p1.R_eq;
        double C1   = 1.5 * G * p2.mass * p2.mass * p1.k2 * R5_1 / (p1.tidal_Q * r6);
        double T1   = -C1 * (spin1 - n_orb);

        
        double I1 = 0.4 * p1.mass * p1.R_eq * p1.R_eq;
        if (std::abs(cos1) > 1e-6)
            p1.angular_velocity += (T1 / I1 * dt) / cos1;

        
        double dv1 = T1 * dt / r;
        p2.vx -= (dv1 / p2.mass) * tx;
        p2.vy -= (dv1 / p2.mass) * ty;
        p2.vz -= (dv1 / p2.mass) * tz;
        p1.vx += (dv1 / p1.mass) * tx;
        p1.vy += (dv1 / p1.mass) * ty;
        p1.vz += (dv1 / p1.mass) * tz;
    }

    
    if (p2.k2 > 0.0 && p2.tidal_Q > 0.0 && p2.R_eq > 0.0) {
        double cos2  = p2.spin_ax*Lx + p2.spin_ay*Ly + p2.spin_az*Lz;
        double spin2 = p2.angular_velocity * cos2;

        double R5_2 = p2.R_eq * p2.R_eq * p2.R_eq * p2.R_eq * p2.R_eq;
        double C2   = 1.5 * G * p1.mass * p1.mass * p2.k2 * R5_2 / (p2.tidal_Q * r6);
        double T2   = -C2 * (spin2 - n_orb);

        double I2 = 0.4 * p2.mass * p2.R_eq * p2.R_eq;
        if (std::abs(cos2) > 1e-6)
            p2.angular_velocity += (T2 / I2 * dt) / cos2;

        
        double dv2 = T2 * dt / r;
        p2.vx -= (dv2 / p2.mass) * tx;
        p2.vy -= (dv2 / p2.mass) * ty;
        p2.vz -= (dv2 / p2.mass) * tz;
        p1.vx += (dv2 / p1.mass) * tx;
        p1.vy += (dv2 / p1.mass) * ty;
        p1.vz += (dv2 / p1.mass) * tz;
    }
}

