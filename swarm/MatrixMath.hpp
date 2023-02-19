/******************************************************************************
 *
 * Copyright (c) 2007, 2008, 2009
 * University of Rochester
 * Department of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *    * Neither the name of the University of Rochester nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

////////////////////////////////////////////////////////////////////////////
//  MatrixMath.hpp -                                                      //
//    Various helper functions to multiply                                //
//    matrices. Matrices are represented in                               //
//    general as float[16] arrays. If C were a                            //
//    matrix the following is the row ordering.                           //
//                                                                        //
// Matrix indexing (COLUMN-MAJOR matrix):                                 //
//   float C[16];                                                         //
//                                                                        //
//   C[0]   C[4]   C[8]    C[12]                                          //
//   C[1]   C[5]   C[9]    C[13]                                          //
//   C[2]   C[6]   C[10]   C[14]                                          //
//   C[3]   C[7]   C[11]   C[15]                                          //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

#ifndef __MATRIXMATH_H__
#define __MATRIXMATH_H__

#include <math.h>

// quick helper to go from x, y to an index
#define MAT4_IND(x, y) ((y) + ((x) * 4))

typedef float Matrix4[16];
typedef float Vector4[4];
typedef float Vector3[3];

// copy a matrix
inline float* Matrix4Set(Matrix4 result, const Matrix4 input)
{
    for (unsigned int i = 0; i < 16; i++)
    {
        result[i] = input[i];
    }

    return result;
}

// Set a Matrix to the identity matrix
inline void Matrix4Identity(Matrix4 res)
{
    res[0]  = 1.0f; res[1]  = 0.0f; res[2]  = 0.0f; res[3]  = 0.0f;
    res[4]  = 0.0f; res[5]  = 1.0f; res[6]  = 0.0f; res[7]  = 0.0f;
    res[8]  = 0.0f; res[9]  = 0.0f; res[10] = 1.0f; res[11] = 0.0f;
    res[12] = 0.0f; res[13] = 0.0f; res[14] = 0.0f; res[15] = 1.0f;
}

// Set a Matrix to a translation matrix for the translation x, y, z
inline void Matrix4Translation(Matrix4 result, const float x, const float y,
                               const float z)
{
    Matrix4Identity(result);

    result[12] = x;
    result[13] = y;
    result[14] = z;
}

// Set a Matrix to a scale matrix for the scaling sx, sy, sz
inline void Matrix4Scale(Matrix4 result, const float sx, const float sy, const float sz)
{
    Matrix4Identity(result);

    result[0]  = sx;
    result[5]  = sy;
    result[10] = sz;
}

// Set a Matrix to a rotation matrix for the rotation theta radians around the X axis
inline void Matrix4RotationX(Matrix4 result, const float theta)
{
    Matrix4Identity(result);

    result[5] = result[10] = cos(theta);
    result[9] = sin(theta);
    result[6] = -result[9];
}

// Set a Matrix to a rotation matrix for the rotation theta radians around the Y axis
inline void Matrix4RotationY(Matrix4 result, const float theta)
{
    Matrix4Identity(result);

    result[0] = result[10] = cos(theta);
    result[2] = sin(theta);
    result[8] = -result[2];
}

// Set a Matrix to a rotation matrix for the rotation theta radians around the Z axis
inline void Matrix4RotationZ(Matrix4 result, const float theta)
{
    Matrix4Identity(result);

    result[0] = result[5] = cos(theta);
    result[4] = sin(theta);
    result[1] = -result[4];
}

// Multiply two matrices and put their result in result
inline void Matrix4Multiply(Matrix4 result, const Matrix4 A, const Matrix4 B)
{
    Matrix4 tmpResult;

    for (unsigned int i = 0; i < 4; i++)
    {
        for (unsigned int j = 0; j < 4; j++)
        {
            tmpResult[MAT4_IND(i,j)] = A[MAT4_IND(i,0)] * B[MAT4_IND(0,j)] +
                A[MAT4_IND(i,1)] * B[MAT4_IND(1,j)] +
                A[MAT4_IND(i,2)] * B[MAT4_IND(2,j)] +
                A[MAT4_IND(i,3)] * B[MAT4_IND(3,j)];
        }
    }

    for (unsigned int i = 0; i < 16; i++)
    {
        result[i] = tmpResult[i];
    }
}

// Multiply two matrices and put their result in result
//    ** This could probably be optimized **
inline void Matrix4TransformCoordinates(const Matrix4 in, float &xRes, float &yRes, float &zRes)
{

    float res[4];
    for (unsigned int j = 0; j < 4; j++)
    {
        res[j] = xRes * in[MAT4_IND(0,j)] +
            yRes * in[MAT4_IND(1,j)] +
            zRes * in[MAT4_IND(2,j)] +
            in[MAT4_IND(3,j)];
    }

    xRes = res[0];
    yRes = res[1];
    zRes = res[2];
}

inline void Matrix4TransformCoordinates(const Matrix4 in, Vector3 point)
{

    float res[4];
    for (unsigned int j = 0; j < 4; j++)
    {
        res[j] = point[0] * in[MAT4_IND(0,j)] +
            point[1] * in[MAT4_IND(1,j)] +
            point[2] * in[MAT4_IND(2,j)] +
            in[MAT4_IND(3,j)];
    }

    point[0] = res[0];
    point[1] = res[1];
    point[2] = res[2];
}

////////////////////////////////////////////////////////////////////////////
//  Vector Math                                                           //
//    Various vector math functions.                                      //
//                                                                        //
////////////////////////////////////////////////////////////////////////////

inline void Vector3Add(Vector3 result, const Vector3 v1, const Vector3 v2)
{
    float res[3];

    res[0] = v1[0] + v2[0];
    res[1] = v1[1] + v2[2];
    res[2] = v1[2] + v2[2];

    result[0] = res[0];
    result[1] = res[1];
    result[2] = res[2];
}

inline void Vector3Scale(Vector3 result, float scale)
{
   result[0] *= scale;
   result[1] *= scale;
   result[2] *= scale;
}

inline void Vector3Cross(Vector3 result, const Vector3 v1, const Vector3 v2)
{
    float res[3];

    res[0] = v1[1]*v2[2] - v1[2]*v2[1];
    res[1] = v1[2]*v2[0] - v1[0]*v2[2];
    res[2] = v1[0]*v2[1] - v1[1]*v2[0];

    result[0] = res[0];
    result[1] = res[1];
    result[2] = res[2];
}

inline float Vector3Dot(const Vector3 v1, const Vector3 v2)
{
    return(v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2]);
}

inline float Vector3Length(const Vector3 pt)
{
    return sqrt(pt[0] * pt[0] + pt[1] * pt[1] + pt[2] * pt[2]);
}

inline void Vector3Normalize(Vector3 point)
{
    float length = Vector3Length(point);

    if (length == 0)
        return;

    point[0] /= length;
    point[1] /= length;
    point[2] /= length;
}

#endif // __MATRIXMATH_H__
