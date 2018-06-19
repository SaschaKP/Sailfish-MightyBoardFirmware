/*
 *  skew-tilt.c
 *  Perform skew or tilt transformations to move points in planes parallel
 *  to Z=0 to a plane parallel to the plane determined by probing the
 *  build plate at three points, P1, P2, P3.
 *
 *  The skew transformation requires only two multiples and two additions
 *  per point.  It does result in a very, very slight skew.  E.g., if the
 *  out-of-level is at most 0.5 mm across a 200 mm baseline, then at worst
 *  a print will be out of perpendicular by 0.25 mm per 100 mm of build
 *  height.  And that's worst case.  For a plate levelled to within 0.2 mm
 *  across the same 200 mm base line, the deviation would be only 0.1 mm
 *  per 100 mm of build height.
 *
 *  The tilt transformation requires eight additional multiples and five
 *  additions per point.  But it introduces no skew.
 *
 *  Skew transform
 *  --------------
 *  Probe the build platform's Z height at three points, P1, P2, and P3,
 *
 *    P1 = (x1, y1, z1)
 *    P2 = (x2, y2, z2)
 *    P3 = (x3, y3, z3)
 *
 * The three points P1, P2, and P3 define a plane.  Equivalently, the two
 * vectors V1 and V2 given by
 *
 *    V1 = P2 - P1
 *    V2 = P3 - P1
 *
 * define the same plane.  A vector N normal to the plane is then given by
 *
 *    N = (Nx, Ny, Nz) = V1 x V2
 *
 * where "x" is the vector cross product.  We want the upward pointing
 * normal so if the Nz component of N is negative, we can just invert N
 * by negating each of its components, Nx, Ny, and Nz.
 *
 * Now, the equation for the plane is then
 *
 *    x * Nx + y * Ny + z * Nz + d = 0
 *
 * We can determine d by substituting in P1 and solving for d,
 *
 *    d = - ( x1 * Nx + y1 * Ny + z1 * Nz )
 *
 * And from that we have our equation to do the skewing,
 *
 *    z-skew = z - (d + x * Nx + y * Ny ) / Nz
 *
 * Tilt transform
 * --------------
 *  Probe the build platform's Z height at three points, P1, P2, and P3,
 *
 *    P1 = (x1, y1, z1)
 *    P2 = (x2, y2, z2)
 *    P3 = (x3, y3, z3)
 *
 * The three points P1, P2, and P3 define a plane.  Equivalently, the two
 * vectors V1 and V2 given by
 *
 *    V1 = P2 - P1
 *    V2 = P3 - P1
 *
 * define the same plane.  A vector N normal to the plane is then given by
 *
 *    N = (Nx, Ny, Nz) = V1 x V2
 *
 * where "x" is the vector cross product.  We want the upward pointing
 * normal so if the Nz component of N is negative, we can just invert N
 * by negating each of its components, Nx, Ny, and Nz.
 *
 * Next, we wish to determine the angular tilt away from the Z axis as
 * measured in the planes Y=0 and X=0,
 *
 *    Ay = arctan(Nx / Nz)  -- tilt in the plane Y=0
 *    Ax = arctan(Ny / Nz)  -- tilt in the plane X=0
 *
 * To transform a point P from a plane parallel to Z=0 to the point P'
 * in the plane parallel to that defined by P1, P2, and P3, we would
 * just perform the requisite rotations around the Y and then X axes
 * (or around Y and then X),
 *
 *    P' = rotate-X(Ax, rotate-Y(-Ay, P))
 *
 * Inverting the process is then just
 *
 *    P = rotate-Y(Ay, rotate-X(-Ax, P'))
 */

#if defined(AUTO_LEVEL)

#include "Compat.hh"
#include <stdlib.h>
#include <util/atomic.h>
#include <avr/eeprom.h>

#if defined(AUTO_LEVEL_TILT)
#include <math.h>
#endif

#include "Steppers.hh"
#include "EepromMap.hh"
#include "SkewTilt.hh"

// Coefficients for the equation of the plane
//
//   dot-product(P, N) + d = 0
//   N = normal = ( skew_data[0], skew_data[1], skew_data[2] )
//   d = skew_data[3]

static int32_t skew_data[4];

// Maximum difference in Z between the three probing points
// we need to initialize this since it's also used for error
// reporting: for build stats,
//
//   >= 0: auto-level is in use and value gives max z difference
//            between each of the probing points in units of steps
//   == ALEVEL_NOT_ACTIVE: auto-leveling not in use
//   == ALEVEL_BAD_LEVEL: auto-leveling disabled; probe points off by too much
//   == ALEVEL_COLINEAR: auto-leveling disabled; probing points were colinear

static int32_t skew_zdelta = ALEVEL_NOT_ACTIVE;

// skewing activated
bool skew_active = false;

// reference point.  Needed to compute d and needed when coordinate
// space is translated
static int32_t r[3];

static void crossProduct(const int32_t *V1, const int32_t *V2, int32_t *N)
{
     // Scale down to prevent 32bit overflow
     N[0] = (V1[1] * V2[2] - V1[2] * V2[1]) / 512;
     N[1] = (V1[2] * V2[0] - V1[0] * V2[2]) / 512;
     N[2] = (V1[0] * V2[1] - V1[1] * V2[0]) / 512;
}

static void skew_constant(void)
{
     // Determine by solving for d using a point in the plane previously saved
     skew_data[3] = - ( r[0] * skew_data[0] + r[1] * skew_data[1] + r[2] * skew_data[2] );
}

int32_t skew(const int32_t *P)
{
     // z - ( x * Nx + y * Ny ) / Nz
     return ( - ( skew_data[3] + P[0] * skew_data[0] + P[1] * skew_data[1] ) / skew_data[2] );
}

void skew_update(const int32_t *delta)
{
     r[0] += delta[0];
     r[1] += delta[1];
     r[2] += delta[2];

     skew_constant();
}

bool skew_check(int32_t maxz, const int32_t *P1, const int32_t *P2,
				const int32_t *P3)
{
     int32_t V1_Z, V2_Z, ztmp;

     V1_Z = P2[2] - P1[2];
     V2_Z = P3[2] - P1[2];

     skew_zdelta = labs(V1_Z);
     ztmp = labs(V2_Z);
     if ( ztmp > skew_zdelta )
	  skew_zdelta = ztmp;

     // Return result of check.
     return ( skew_zdelta <= maxz );
}

bool skew_init(int32_t maxz, int32_t zoffset,
	       const int32_t *P1, const int32_t *P2, const int32_t *P3)
{
     int32_t probeComps[3], probeOffsets[2], V1[3], V2[3], ztmp;

     skew_deinit();

     // Check for a too far out of level condition
     V1[2] = P2[2] - P1[2];
     V2[2] = P3[2] - P1[2];

     skew_zdelta = labs(V1[2]);
     ztmp = labs(V2[2]);
     if ( ztmp > skew_zdelta )
	  skew_zdelta = ztmp;

     // Make sure the maximal height difference doesn't exceed maxz
     if ( skew_zdelta > maxz )
     {
	  skew_zdelta = ALEVEL_BAD_LEVEL;
	  return false;
     }

     cli();
     eeprom_read_block(probeComps, (void *)eeprom_offsets::ALEVEL_PROBE_COMP_SETTINGS,
		       3*sizeof(int32_t));
#if !defined(ZYYX_3D_PRINTER)
     eeprom_read_block(probeOffsets, (void *)eeprom_offsets::ALEVEL_PROBE_OFFSETS,
		       2*sizeof(int32_t));
#endif
     sei();

#if defined(ZYYX_3D_PRINTER)
     probeOffsets[0] = -stepperAxisMMToSteps(27, X_AXIS);
     probeOffsets[1] = 0;
#endif

     V1[0] = P2[0] - P1[0];
     V1[1] = P2[1] - P1[1];
     V1[2] += probeComps[1] - probeComps[0]; // (P2[2] + probeComps[1]) - (P1[2] + probeComps[0])

     V2[0] = P3[0] - P1[0];
     V2[1] = P3[1] - P1[1];
     V2[2] += probeComps[2] - probeComps[0]; // (P3[2] + probeComps[2]) - (P1[2] + probeComps[0])

     // Compute the normal to the plane and store
     // in skew_data[0], [1], [2]
     crossProduct(V1, V2, skew_data);

     // This should never happen: it indicates that either the
     //   probing points fail to define a plane (are co-linear), or
     //   the plane is parallel to the Z axis!  In that case, the
     //   ztmp > skew_zdata test should have triggered a failure
     if ( skew_data[2] == 0 ) {
	  skew_zdelta = ( skew_data[0] == 0 && skew_data[1] == 0 ) ?
	       ALEVEL_COLINEAR : ALEVEL_BAD_LEVEL;
	  return false;
     }

     // We want the upward pointing normal
     if ( skew_data[2] < 0 )
     {
	  skew_data[0] = -skew_data[0];
	  skew_data[1] = -skew_data[1];
	  skew_data[2] = -skew_data[2];
     }

     // Save P1 as a reference point in case we need
     // to recompute d when the coordinate system is
     // translated.
     //
     // We make
     //
     //   r[z] = Probed[Z] - Zoffset
     //
     // where Zoffset is the distance between the probe tip
     // and the tip of the extruder nozzle.  We only need to
     // account for this once.  If and when we update the skew
     // constant, we do so using the relative displacement and
     // not the absolute.  As such, we do not need to know Zoffset
     // then.

     r[0] = P1[0] + probeOffsets[0];
     r[1] = P1[1] + probeOffsets[1];
     r[2] = P1[2] - zoffset;

     // Calculate the constant d using the reference point
     skew_constant();

     // And we're good to go
     skew_active = true;

     return true;
}

void skew_deinit(void)
{
     skew_data[0] = 0;
     skew_data[1] = 0;
     skew_data[2] = 1;
     skew_data[3] = 0;
     skew_zdelta  = ALEVEL_NOT_ACTIVE;
     skew_active  = false;
}

int32_t skew_status(void)
{
     return skew_zdelta;
}

#if defined(AUTO_LEVEL_TILT)

#define cosAx       0
#define cosAy       1
#define sinAx       2
#define sinAy       3
#define cosAx_cosAy 4
#define cosAx_sinAy 5
#define sinAx_sinAy 6
#define sinAx_cosAy 7

static FPTYPE tilt_data[8];

bool tilt_init(Point &P1, Point &P2, Point &P3)
{
     int32_t N[3], V1[3], V2[3];

     V1[0] = P2[0] - P1[0];
     V1[1] = P2[1] - P1[1];
     V1[2] = P2[2] - P1[2];

     V2[0] = P3[0] - P1[0];
     V2[1] = P3[1] - P1[1];
     V2[2] = P3[2] - P1[2];

     crossProduct(V1, V2, N);

     if ( N[2] == 0 )
	  return false;

     // We want the upward pointing normal
     if ( N[2] < 0 )
     {
	  N[0] = -N[0];
	  N[1] = -N[1];
	  N[2] = -N[2];
     }

     FPTYPE Ax, Ay, Nz;

     Nz = ITOFP(N[2]);
     Ax = atan2(ITOFP(N[1]), Nz);
     Ay = atan2(ITOFP(N[0]), Nz);

     tilt_data[cosAx] = FCOS(Ax);
     tilt_data[cosAy] = FCOS(Ay);
     tilt_data[sinAx] = FSIN(Ax);
     tilt_data[sinAy] = FSIN(Ay);
     tilt_data[cosAx_cosAy] = FPMULT2(tilt_data[cosAx], tilt_data[cosAy]);
     tilt_data[cosAx_sinAy] = FPMULT2(tilt_data[cosAx], tilt_data[sinAy]);
     tilt_data[sinAx_sinAy] = FPMULT2(tilt_data[sinAx], tilt_data[sinAy]);
     tilt_data[sinAx_cosAy] = FPMULT2(tilt_data[sinAx], tilt_data[cosAy]);

     return true;
}

Point tilt(Point &P)
{
     FPTYPE Px, Py, Pz;

     Px = ITOFP(P[0]);
     Py = ITOFP(P[1]);
     Pz = ITOFP(P[2]);

     Point np;

     np[0] = FPTOI(FPMULT2(Px, tilt_data[cosAx]) +
		    FPMULT2(Pz, tilt_data[sinAy]));
     np[1] = FPTOI(FPMULT2(Py, tilt_data[cosAx]) -
		    FPMULT2(Px, tilt_data[sinAx_sinAy]) +
		    FPMULT2(Px, tilt_data[sinAx_cosAy]));
     np[2] = FPTOI(FPMULT2(Pz, tilt_data[cosAx_cosAy]) +
		    FPMULT2(Px, tilt_data[cosAx_sinAy]) -
		    FPMULT2(Py, tilt_data[sinAx]));

     return np;
}

Point tilt_inverse(Point &P)
{
     FPTYPE Px, Py, Pz;

     Px = ITOFP(P[0]);
     Py = ITOFP(P[1]);
     Pz = ITOFP(P[2]);

     Point np;

     np[0] = FPTOI(FPMULT2(Px, tilt_data[cosAy]) +
		    FPMULT2(Pz, tilt_data[sinAx]));
     np[1] = FPTOI(FPMULT2(Py, tilt_data[cosAy]) -
		    FPMULT2(Px, tilt_data[sinAx_sinAy]) +
		    FPMULT2(Px, tilt_data[cosAx_sinAy]));
     np[2] = FPTOI(FPMULT2(Pz, tilt_data[cosAx_cosAy]) +
		    FPMULT2(Px, tilt_data[sinAx_cosAy]) -
		    FPMULT2(Py, tilt_data[sinAy]));

     return np;
}

#endif

#endif
