#pragma once
#include "body.h"

void computeGravity(Body& p1, Body& p2, double G, double c);

// Tidal torque and orbital evolution (constant time-lag model).
// Applies tidal orbital velocity kicks directly and updates angular_velocity.
// Call once per full dt step, outside the Yoshida sub-steps.
void computeTides(Body& p1, Body& p2, double G, double dt);