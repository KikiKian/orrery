#pragma once
#include "body.h"

void computeGravity(Body& p1, Body& p2, double G, double c);

void computeTides(Body& p1, Body& p2, double G, double dt);

void computeDrag(Body& satellite, const Body& planet, double dt);