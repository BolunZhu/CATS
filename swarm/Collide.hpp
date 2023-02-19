//*****************************************************************************
//   This program is a 'remote' 3D-collision detector for two balls on linear
//   trajectories and returns, if applicable, the location of the collision for
//   both balls as well as the new velocity vectors (assuming a fully elastic
//   collision).
//   All variables apart from 'error' are of float Precision Floating Point type.
//
//   The Parameters are:
//
//    m1    (mass of ball 1)
//    m2    (mass of ball 2)
//    r1    (radius of ball 1)
//    r2    (radius of ball 2)
//  & x1    (x-coordinate of ball 1)
//  & y1    (y-coordinate of ball 1)
//  & z1    (z-coordinate of ball 1)
//  & x2    (x-coordinate of ball 2)
//  & y2    (y-coordinate of ball 2)
//  & z2    (z-coordinate of ball 2)
//  & vx1   (velocity x-component of ball 1)
//  & vy1   (velocity y-component of ball 1)
//  & vz1   (velocity z-component of ball 1)
//  & vx2   (velocity x-component of ball 2)
//  & vy2   (velocity y-component of ball 2)
//  & vz2   (velocity z-component of ball 2)
//  & error (int)     (0: no error
//                     1: balls do not collide
//                     2: initial positions impossible (balls overlap))
//
//   Note that the parameters with an ampersand (&) are passed by reference,
//   i.e. the corresponding arguments in the calling program will be updated
//   (the positions and velocities however only if 'error'=0).
//   All variables should have the same data types in the calling program
//   and all should be initialized before calling the function.
//
//   This program is free to use for everybody. However, you use it at your own
//   risk and I do not accept any liability resulting from incorrect behavior.
//   I have tested the program for numerous cases and I could not see anything
//   wrong with it but I can not guarantee that it is bug-free under any
//   circumstances.
//
//   I would appreciate if you could report any problems to me
//   (for contact details see  http://www.plasmaphysics.org.uk/feedback.htm ).
//
//   Thomas Smid   February 2004
//                 December 2005 (a few minor changes to improve speed)
//******************************************************************************


void collision3D(float m1, float m2, float r1, float r2,
                 float& x1, float& y1,float& z1,
                 float& x2, float& y2, float& z2,
                 float& vx1, float& vy1, float& vz1,
                 float& vx2, float& vy2, float& vz2,
                 int& error);
