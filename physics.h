#pragma once
#include "body.h"

void computeGravity(Body& p1, Body& p2, double G);
void updateBody(Body& b, double dt);