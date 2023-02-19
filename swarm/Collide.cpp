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
//   risk and I do not accept any liability resulting from incorrect behaviour.
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
#include <math.h>
#include <iostream>
#include <float.h>
#include "MatrixMath.hpp"

#ifndef isnan
#define isnan(x) ((x) != (x))
#endif

#ifdef __linux__
float abs(float f)
{
    return (f < 0) ? -f : f;
}
#endif

void collision3D(float m1, float m2, float r1, float r2,
                 float& rx1, float& ry1,float& rz1,
                 float& rx2, float& ry2, float& rz2,
                 float& rvx1, float& rvy1, float& rvz1,
                 float& rvx2, float& rvy2, float& rvz2,
                 int& error)

{

   float x1 = rx1;
   float y1 = ry1;
   float z1 = rz1;

   float x2 = rx2;
   float y2 = ry2;
   float z2 = rz2;

   float vx1 = rvx1;
   float vy1 = rvy1;
   float vz1 = rvz1;

   float vx2 = rvx2;
   float vy2 = rvy2;
   float vz2 = rvz2;



   float  pi,r12,m21,d,v,theta2,phi2,st,ct,sp,cp,vx1r,vy1r,vz1r,thetav,phiv,
          dr,alpha,beta,sbeta,cbeta,t,a,dvz2,vx2r,vy2r,vz2r,
          x21,y21,z21,vx21,vy21,vz21;

   //     **** initialize some variables ****
   pi=acos(-1.0E0);
   error=0;
   r12=r1+r2;
   m21=m2/m1;
   x21=x2-x1;
   y21=y2-y1;
   z21=z2-z1;
   vx21=vx2-vx1;
   vy21=vy2-vy1;
   vz21=vz2-vz1;

   //     **** calculate relative distance and relative speed ***
   d=sqrt(x21*x21 +y21*y21 +z21*z21);
   v=sqrt(vx21*vx21 +vy21*vy21 +vz21*vz21);

   //     **** return if distance between balls smaller than sum of radii ****
   if (d<r12) {error=2; return;}
   if (d<r12+.001f) {error=4; return;}

   //     **** return if relative speed = 0 ****
   if (abs(v)<0.001) {error=3; return;}


   //     **** shift coordinate system so that ball 1 is at the origin ***
   x2=x21;
   y2=y21;
   z2=z21;

   //     **** boost coordinate system so that ball 2 is resting ***
   vx1=-vx21;
   vy1=-vy21;
   vz1=-vz21;

   //     **** find the polar coordinates of the location of ball 2 ***
   theta2=acos(z2/d);
   if (x2==0 && y2==0) {phi2=0;} else {phi2=atan2(y2,x2);}
   st=sin(theta2);
   ct=cos(theta2);
   sp=sin(phi2);
   cp=cos(phi2);


   //     **** express the velocity vector of ball 1 in a rotated coordinate
   //          system where ball 2 lies on the z-axis ******
   vx1r=ct*cp*vx1+ct*sp*vy1-st*vz1;
   vy1r=cp*vy1-sp*vx1;
   vz1r=st*cp*vx1+st*sp*vy1+ct*vz1;
   thetav=acos(vz1r/v);
   if (vx1r==0 && vy1r==0) {phiv=0;} else {phiv=atan2(vy1r,vx1r);}


   //     **** calculate the normalized impact parameter ***
   dr=d*sin(thetav)/r12;


   //     **** return old positions and velocities if balls do not collide ***
   if (thetav>pi/2 || fabs(dr)>1) {
      x2=x2+x1;
      y2=y2+y1;
      z2=z2+z1;
      vx1=vx1+vx2;
      vy1=vy1+vy2;
      vz1=vz1+vz2;
      error=1;
      return;
   }

   //     **** calculate impact angles if balls do collide ***
   alpha=asin(-dr);
   beta=phiv;
   sbeta=sin(beta);
   cbeta=cos(beta);


   //     **** calculate time to collision ***
   t=(d*cos(thetav) -r12*sqrt(1-dr*dr))/v;


   //     **** update positions and reverse the coordinate shift ***
   x2=x2+vx2*t +x1;
   y2=y2+vy2*t +y1;
   z2=z2+vz2*t +z1;
   x1=(vx1+vx2)*t +x1;
   y1=(vy1+vy2)*t +y1;
   z1=(vz1+vz2)*t +z1;



   //  ***  update velocities ***

   a=tan(thetav+alpha);

   dvz2=2*(vz1r+a*(cbeta*vx1r+sbeta*vy1r))/((1+a*a)*(1+m21));

   vz2r=dvz2;
   vx2r=a*cbeta*dvz2;
   vy2r=a*sbeta*dvz2;
   vz1r=vz1r-m21*vz2r;
   vx1r=vx1r-m21*vx2r;
   vy1r=vy1r-m21*vy2r;


   //  **** rotate the velocity vectors back and add the initial velocity
   //        vector of ball 2 to retrieve the original coordinate system ****

   vx1=ct*cp*vx1r-sp*vy1r+st*cp*vz1r +vx2;
   vy1=ct*sp*vx1r+cp*vy1r+st*sp*vz1r +vy2;
   vz1=ct*vz1r-st*vx1r               +vz2;
   vx2=ct*cp*vx2r-sp*vy2r+st*cp*vz2r +vx2;
   vy2=ct*sp*vx2r+cp*vy2r+st*sp*vz2r +vy2;
   vz2=ct*vz2r-st*vx2r               +vz2;


   if (isnan(vx1)||isnan(vy1)||isnan(vz1))
   {
      return;
   }

   if (isnan(vx2)||isnan(vy2)||isnan(vz2))
   {
      return;
   }

   if (isnan(x1)||isnan(y1)||isnan(z1))
   {
      return;
   }

   rx1 = x1;
   ry1 = y1;
   rz1 = z1;

   rx2 = x2;
   ry2 = y2;
   rz2 = z2;

   rvx1 = vx1;
   rvy1 = vy1;
   rvz1 = vz1;

   rvx2 = vx2;
   rvy2 = vy2;
   rvz2 = vz2;


   //std::cout << "V1," << vx1 << "," << vy1 << "," << vz1 << std::endl;
   //std::cout << "V2," << vx2 << "," << vy2 << "," << vz2 << std::endl;

   return;
}

