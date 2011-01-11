/*
    Copyright (C) 2010 Jochen Gerhard <gerhard@compeng.uni-frankfurt.de>
    Copyright (C) 2010 Matthias Kretz <kretz@kde.org>

    Permission to use, copy, modify, and distribute this software
    and its documentation for any purpose and without fee is hereby
    granted, provided that the above copyright notice appear in all
    copies and that both that the copyright notice and this
    permission notice and warranty disclaimer appear in supporting
    documentation, and that the name of the author not be used in
    advertising or publicity pertaining to distribution of the
    software without specific, written prior permission.

    The author disclaim all warranties with regard to this
    software, including all implied warranties of merchantability
    and fitness.  In no event shall the author be liable for any
    special, indirect or consequential damages or any damages
    whatsoever resulting from loss of use, data or profits, whether
    in an action of contract, negligence or other tortious action,
    arising out of or in connection with the use or performance of
    this software.

*/

/*!
  Finite difference method example

  We calculate central differences for a given function and
  compare it to the analytical solution.

*/

#include <Vc/Vc>
#include <iostream>
#include <iomanip>
#include <cmath>
#include "../../benchmarks/tsc.h"

#define USE_SCALAR_SINCOS

enum {
  N = 10240000,
  PrintStep = 1000000
};

static const float epsilon = 1e-7;
static const float lower = 0.f;
static const float upper = 40000.f;
static const float h = (upper - lower) / N;

// dfu is the derivative of fu. This is really easy for sine and cosine:
static inline float  fu(float x) { return ( std::sin(x) ); }
static inline float dfu(float x) { return ( std::cos(x) ); }

static inline Vc::float_v fu(Vc::float_v x) {
#ifdef USE_SCALAR_SINCOS
  Vc::float_v r;
  for (size_t i = 0; i < Vc::float_v::Size; ++i) {
    r[i] = std::sin(x[i]);
  }
  return r;
#else
  return Vc::sin(x);
#endif
}

static inline Vc::float_v dfu(Vc::float_v x) {
#ifdef USE_SCALAR_SINCOS
  Vc::float_v r;
  for (size_t i = 0; i < Vc::float_v::Size; ++i) {
    r[i] = std::cos(x[i]);
  }
  return r;
#else
  return Vc::cos(x);
#endif
}

using Vc::float_v;

// It is important for this example that the following variables (especially dy_points) are global
// variables. Else the compiler can optimze all calculations of dy away except for the few places
// where the value is used in printResults.
Vc::Memory<float_v, N> x_points;
Vc::Memory<float_v, N> y_points;
Vc::Memory<float_v, N> dy_points;

void printResults()
{
    std::cout
        << "------------------------------------------------------------\n"
        << std::setw(15) << "fu(x_i)"
        << std::setw(15) << "FD fu'(x_i)"
        << std::setw(15) << "SYM fu'(x)"
        << std::setw(15) << "error %\n";
    for (int i = 0; i < N; i += PrintStep) {
        std::cout
            << std::setw(15) << y_points[i]
            << std::setw(15) << dy_points[i]
            << std::setw(15) << dfu(x_points[i])
            << std::setw(15) << std::abs((dy_points[i] - dfu(x_points[i])) / (dfu(x_points[i] + epsilon)) * 100)
            << "\n";
    }
    std::cout
        << std::setw(15) << y_points[N - 1]
        << std::setw(15) << dy_points[N - 1]
        << std::setw(15) << dfu(x_points[N - 1])
        << std::setw(15) << std::abs((dy_points[N - 1] - dfu(x_points[N - 1])) / (dfu(x_points[N - 1] + epsilon)) * 100)
        << std::endl;
}

int main()
{
    {
      float_v x_i(float_v::IndexType::IndexesFromZero());
      for ( unsigned int i = 0; i < x_points.vectorsCount(); ++i, x_i += float_v::Size ) {
        const float_v x = x_i * h;
        x_points.vector(i) = x;
        y_points.vector(i) = fu(x);
      }
    }

    double speedup;
    TimeStampCounter timer;

    { ///////// ignore this part - it only wakes up the CPU ////////////////////////////
        const float oneOver2h = 0.5f / h;

        // set borders explicit as up- or downdifferential
        dy_points[0] = (y_points[1] - y_points[0]) / h;
        // GCC auto-vectorizes the following loop. It is interesting to see that both Vc::Scalar and
        // Vc::SSE are faster, though.
        for ( int i = 1; i < N - 1; ++i) {
            dy_points[i] = (y_points[i + 1] - y_points[i - 1]) * oneOver2h;
        }
        dy_points[N - 1] = (y_points[N - 1] - y_points[N - 2]) / h;
    } //////////////////////////////////////////////////////////////////////////////////

    {
        std::cout << "\n" << std::setw(60) << "Classical finite difference method" << std::endl;
        timer.Start();

        const float oneOver2h = 0.5f / h;

        // set borders explicit as up- or downdifferential
        dy_points[0] = (y_points[1] - y_points[0]) / h;
        // GCC auto-vectorizes the following loop. It is interesting to see that both Vc::Scalar and
        // Vc::SSE are faster, though.
        for ( int i = 1; i < N - 1; ++i) {
            dy_points[i] = (y_points[i + 1] - y_points[i - 1]) * oneOver2h;
        }
        dy_points[N - 1] = (y_points[N - 1] - y_points[N - 2]) / h;

        timer.Stop();
        printResults();
        std::cout << "cycle count: " << timer.Cycles() << "\n";
    }

    speedup = timer.Cycles();
    {
        std::cout << std::setw(60) << "Vectorized finite difference method" << std::endl;
        timer.Start();

        // All the differentials require to calculate (r - l) / 2h, where we calculate 1/2h as a
        // constant before the loop to avoid unnecessary calculations. Note that a good compiler can
        // already do this for you.
        const float_v oneOver2h = 0.5f / h;

        // Calculate the left border
        dy_points[0] = (y_points[1] - y_points[0]) / h;

        // Calculate the differentials streaming through the y and dy memory. The picture below
        // should give an idea of what values in y get read and what values are written to dy in
        // each iteration:
        //
        // y  [...................................]
        //     00001111222233334444555566667777
        //       00001111222233334444555566667777
        // dy [...................................]
        //      00001111222233334444555566667777
        //
        // The loop is manually unrolled four times to improve instruction level parallelism and
        // prefetching on architectures where four vectors fill one cache line. (Note that this
        // unrolling breaks auto-vectorization of the Vc::Scalar implementation when compiling with
        // GCC.)
        for (unsigned int i = 0; i < (y_points.entriesCount() - 2) / float_v::Size; i += 4) {
            // Prefetches make sure the data which is going to be used in 24/4 iterations is already
            // in the L1 cache. The prefetchForOneRead additionally instructs the CPU to not evict
            // these cache lines to L2/L3. The prefetchForModify is used as a small optimization on
            // AMD CPUs to mark cache lines as modified already before the store is executed.
            Vc::prefetchForOneRead(&y_points[(i + 24) * float_v::Size]);
            Vc::prefetchForModify(&dy_points[(i + 24) * float_v::Size]);

            const float_v diff0 = y_points.vector(i + 0, 2) - y_points.vector(i + 0);
            const float_v diff1 = y_points.vector(i + 1, 2) - y_points.vector(i + 1);
            const float_v diff2 = y_points.vector(i + 2, 2) - y_points.vector(i + 2);
            const float_v diff3 = y_points.vector(i + 3, 2) - y_points.vector(i + 3);

            dy_points.vector(i    , 1) = diff0 * oneOver2h;
            dy_points.vector(i + 1, 1) = diff1 * oneOver2h;
            dy_points.vector(i + 2, 1) = diff2 * oneOver2h;
            dy_points.vector(i + 3, 1) = diff3 * oneOver2h;
        }

        // Process the last vector. Note that this works for any N because Vc::Memory adds padding
        // to y_points and dy_points such that the last scalar value is somewhere inside lastVector.
        // The correct right border value for dy_points is overwritten in the last step unless N is
        // a multiple of float_v::Size + 2.
        // y  [...................................]
        //                                  8888
        //                                    8888
        // dy [...................................]
        //                                   8888
        {
            const unsigned int i = y_points.vectorsCount() - 1;
            const float_v left = y_points.vector(i, -2);
            const float_v right = y_points.lastVector();
            dy_points.vector(i, -1) = (right - left) * oneOver2h;
        }

        // ... and finally the right border
        dy_points[N - 1] = (y_points[N - 1] - y_points[N - 2]) / h;

        timer.Stop();
        printResults();
        std::cout << "cycle count: " << timer.Cycles() << "\n";
    }
    speedup /= timer.Cycles();
    std::cout << "Speedup: " << speedup << "\n";
    return 0;
}