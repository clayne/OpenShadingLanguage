// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#include <limits>

#include "oslexec_pvt.h"
#include <OSL/Imathx/Imathx.h>
#include <OSL/dual_vec.h>
#include <OSL/fmt_util.h>
#include <OSL/hashes.h>
#include <OSL/oslnoise.h>

#include <OpenImageIO/fmath.h>

OSL_NAMESPACE_BEGIN
namespace pvt {


#if 0  // only when testing the statistics of perlin noise to normalize the range

#    include <random>

void test_perlin(int d) {
    HashScalar h;
    float noise_min = +std::numeric_limits<float>::max();
    float noise_max = -std::numeric_limits<float>::max();
    float noise_avg = 0;
    float noise_avg2 = 0;
    float noise_stddev;
    std::mt19937 rndgen;
    std::uniform_real_distribution<float> rnd (0.0f, 1.0f);
    print("Running perlin-{} noise test ...\n", d);
    const int n = 100000000;
    const float r = 1024;
    for (int i = 0; i < n; i++) {
        float noise;
        float nx = rnd(rndgen); nx = (2 * nx - 1) * r;
        float ny = rnd(rndgen); ny = (2 * ny - 1) * r;
        float nz = rnd(rndgen); nz = (2 * nz - 1) * r;
        float nw = rnd(rndgen); nw = (2 * nw - 1) * r;
        switch (d) {
            case 1: perlin(noise, h, nx); break;
            case 2: perlin(noise, h, nx, ny); break;
            case 3: perlin(noise, h, nx, ny, nz); break;
            case 4: perlin(noise, h, nx, ny, nz, nw); break;
        }
        if (noise_min > noise) noise_min = noise;
        if (noise_max < noise) noise_max = noise;
        noise_avg += noise;
        noise_avg2 += noise * noise;
    }
    noise_avg /= n;
    noise_stddev = std::sqrt((noise_avg2 - noise_avg * noise_avg * n) / n);
    print("Result: perlin-{} noise stats:\n\tmin: {:.17g}\n\tmax: {:.17g}\n\tavg: {:.17g}\n\tdev: {:.17g}\n",
            d, noise_min, noise_max, noise_avg, noise_stddev);
    print("Normalization: {:.17g}\n", 1.0f / std::max(fabsf(noise_min), fabsf(noise_max)));
}

#endif



/***********************************************************************
 * noise routines callable by the LLVM-generated code.
 */

#if 1
// clang-format off

#define NOISE_IMPL(opname,implname)                                     \
OSL_SHADEOP OSL_HOSTDEVICE float osl_ ##opname## _ff (float x) {        \
    implname impl;                                                      \
    float r;                                                            \
    impl (r, x);                                                        \
    return r;                                                           \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE float osl_ ##opname## _fff (float x, float y) { \
    implname impl;                                                      \
    float r;                                                            \
    impl (r, x, y);                                                     \
    return r;                                                           \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE float osl_ ##opname## _fv (char *x) {        \
    implname impl;                                                      \
    float r;                                                            \
    impl (r, VEC(x));                                                   \
    return r;                                                           \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE float osl_ ##opname## _fvf (char *x, float y) { \
    implname impl;                                                      \
    float r;                                                            \
    impl (r, VEC(x), y);                                                \
    return r;                                                           \
}                                                                       \
                                                                        \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _vf (char *r, float x) { \
    implname impl;                                                      \
    impl (VEC(r), x);                                                   \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _vff (char *r, float x, float y) { \
    implname impl;                                                      \
    impl (VEC(r), x, y);                                                \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _vv (char *r, char *x) { \
    implname impl;                                                      \
    impl (VEC(r), VEC(x));                                              \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _vvf (char *r, char *x, float y) { \
    implname impl;                                                      \
    impl (VEC(r), VEC(x), y);                                           \
}





#define NOISE_IMPL_DERIV(opname,implname)                               \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdf (char *r, char *x) { \
    implname impl;                                                      \
    impl (DFLOAT(r), DFLOAT(x));                                        \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdfdf (char *r, char *x, char *y) { \
    implname impl;                                                      \
    impl (DFLOAT(r), DFLOAT(x), DFLOAT(y));                             \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdff (char *r, char *x, float y) { \
    implname impl;                                                      \
    impl (DFLOAT(r), DFLOAT(x), Dual2<float>(y));                       \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dffdf (char *r, float x, char *y) { \
    implname impl;                                                      \
    impl (DFLOAT(r), Dual2<float>(x), DFLOAT(y));                       \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdv (char *r, char *x) { \
    implname impl;                                                      \
    impl (DFLOAT(r), DVEC(x));                                          \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdvdf (char *r, char *x, char *y) { \
    implname impl;                                                      \
    impl (DFLOAT(r), DVEC(x), DFLOAT(y));                               \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdvf (char *r, char *x, float y) { \
    implname impl;                                                      \
    impl (DFLOAT(r), DVEC(x), Dual2<float>(y));                         \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfvdf (char *r, char *x, char *y) { \
    implname impl;                                                      \
    impl (DFLOAT(r), Dual2<Vec3>(VEC(x)), DFLOAT(y));                   \
}                                                                       \
                                                                        \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdf (char *r, char *x) { \
    implname impl;                                                      \
    impl (DVEC(r), DFLOAT(x));                                          \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdfdf (char *r, char *x, char *y) { \
    implname impl;                                                      \
    impl (DVEC(r), DFLOAT(x), DFLOAT(y));                               \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdff (char *r, char *x, float y) { \
    implname impl;                                                      \
    impl (DVEC(r), DFLOAT(x), Dual2<float>(y));                         \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvfdf (char *r, float x, char *y) { \
    implname impl;                                                      \
    impl (DVEC(r), Dual2<float>(x), DFLOAT(y));                         \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdv (char *r, char *x) { \
    implname impl;                                                      \
    impl (DVEC(r), DVEC(x));                                            \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdvdf (char *r, char *x, char *y) { \
    implname impl;                                                      \
    impl (DVEC(r), DVEC(x), DFLOAT(y));                                 \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdvf (char *r, char *x, float y) { \
    implname impl;                                                      \
    impl (DVEC(r), DVEC(x), Dual2<float>(y));                           \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvvdf (char *r, char *x, char *y) { \
    implname impl;                                                      \
    impl (DVEC(r), Dual2<Vec3>(VEC(x)), DFLOAT(y));                     \
}




#define NOISE_IMPL_DERIV_OPT(opname,implname)                           \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdf (ustringhash_pod  name_, char *r, char *x, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                          \
    impl (name, DFLOAT(r), DFLOAT(x), (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdfdf (ustringhash_pod  name_, char *r, char *x, char *y, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DFLOAT(r), DFLOAT(x), DFLOAT(y), (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdv (ustringhash_pod  name_, char *r, char *x, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DFLOAT(r), DVEC(x), (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdvdf (ustringhash_pod  name_, char *r, char *x, char *y, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DFLOAT(r), DVEC(x), DFLOAT(y), (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdf (ustringhash_pod  name_, char *r, char *x, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DVEC(r), DFLOAT(x), (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdfdf (ustringhash_pod  name_, char *r, char *x, char *y, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DVEC(r), DFLOAT(x), DFLOAT(y), (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdv (ustringhash_pod  name_, char *r, char *x, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DVEC(r), DVEC(x), (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdvdf (ustringhash_pod  name_, char *r, char *x, char *y, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DVEC(r), DVEC(x), DFLOAT(y), (ShaderGlobals *)sg, (NoiseParams *)opt); \
}




NOISE_IMPL (cellnoise, CellNoise)
NOISE_IMPL (hashnoise, HashNoise)
NOISE_IMPL (noise, Noise)
NOISE_IMPL_DERIV (noise, Noise)
NOISE_IMPL (snoise, SNoise)
NOISE_IMPL_DERIV (snoise, SNoise)
NOISE_IMPL (simplexnoise, SimplexNoise)
NOISE_IMPL_DERIV (simplexnoise, SimplexNoise)
NOISE_IMPL (usimplexnoise, USimplexNoise)
NOISE_IMPL_DERIV (usimplexnoise, USimplexNoise)



#define PNOISE_IMPL(opname,implname)                                    \
OSL_SHADEOP OSL_HOSTDEVICE float osl_ ##opname## _fff (float x, float px) { \
    implname impl;                                                      \
    float r;                                                            \
    impl (r, x, px);                                                    \
    return r;                                                           \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE float osl_ ##opname## _fffff (float x, float y, float px, float py) { \
    implname impl;                                                      \
    float r;                                                            \
    impl (r, x, y, px, py);                                             \
    return r;                                                           \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE float osl_ ##opname## _fvv (char *x, char *px) { \
    implname impl;                                                      \
    float r;                                                            \
    impl (r, VEC(x), VEC(px));                                          \
    return r;                                                           \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE float osl_ ##opname## _fvfvf (char *x, float y, char *px, float py) { \
    implname impl;                                                      \
    float r;                                                            \
    impl (r, VEC(x), y, VEC(px), py);                                   \
    return r;                                                           \
}                                                                       \
                                                                        \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _vff (char *r, float x, float px) { \
    implname impl;                                                      \
    impl (VEC(r), x, px);                                               \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _vffff (char *r, float x, float y, float px, float py) { \
    implname impl;                                                      \
    impl (VEC(r), x, y, px, py);                                        \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _vvv (char *r, char *x, char *px) { \
    implname impl;                                                      \
    impl (VEC(r), VEC(x), VEC(px));                                     \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _vvfvf (char *r, char *x, float y, char *px, float py) { \
    implname impl;                                                      \
    impl (VEC(r), VEC(x), y, VEC(px), py);                              \
}





#define PNOISE_IMPL_DERIV(opname,implname)                              \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdff (char *r, char *x, float px) {  \
    implname impl;                                                      \
    impl (DFLOAT(r), DFLOAT(x), px);                                    \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdfdfff (char *r, char *x, char *y, float px, float py) { \
    implname impl;                                                      \
    impl (DFLOAT(r), DFLOAT(x), DFLOAT(y), px, py);                     \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdffff (char *r, char *x, float y, float px, float py) { \
    implname impl;                                                      \
    impl (DFLOAT(r), DFLOAT(x), Dual2<float>(y), px, py);               \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dffdfff (char *r, float x, char *y, float px, float py) { \
    implname impl;                                                      \
    impl (DFLOAT(r), Dual2<float>(x), DFLOAT(y), px, py);               \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdvv (char *r, char *x, char *px) {  \
    implname impl;                                                      \
    impl (DFLOAT(r), DVEC(x), VEC(px));                                 \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdvdfvf (char *r, char *x, char *y, char *px, float py) { \
    implname impl;                                                      \
    impl (DFLOAT(r), DVEC(x), DFLOAT(y), VEC(px), py);                  \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdvfvf (char *r, char *x, float y, char *px, float py) { \
    implname impl;                                                      \
    impl (DFLOAT(r), DVEC(x), Dual2<float>(y), VEC(px), py);            \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfvdfvf (char *r, char *x, char *y, char *px, float py) { \
    implname impl;                                                      \
    impl (DFLOAT(r), Dual2<Vec3>(VEC(x)), DFLOAT(y), VEC(px), py);      \
}                                                                       \
                                                                        \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdff (char *r, char *x, float px) {  \
    implname impl;                                                      \
    impl (DVEC(r), DFLOAT(x), px);                                      \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdfdfff (char *r, char *x, char *y, float px, float py) { \
    implname impl;                                                      \
    impl (DVEC(r), DFLOAT(x), DFLOAT(y), px, py);                       \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdffff (char *r, char *x, float y, float px, float py) { \
    implname impl;                                                      \
    impl (DVEC(r), DFLOAT(x), Dual2<float>(y), px, py);                 \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvfdfff (char *r, float x, char *y, float px, float py) { \
    implname impl;                                                      \
    impl (DVEC(r), Dual2<float>(x), DFLOAT(y), px, py);                 \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdvv (char *r, char *x, char *px) {  \
    implname impl;                                                      \
    impl (DVEC(r), DVEC(x), VEC(px));                                   \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdvdfvf (char *r, char *x, char *y, char *px, float py) { \
    implname impl;                                                      \
    impl (DVEC(r), DVEC(x), DFLOAT(y), VEC(px), py);                    \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdvfvf (char *r, char *x, float y, void *px, float py) { \
    implname impl;                                                      \
    impl (DVEC(r), DVEC(x), Dual2<float>(y), VEC(px), py);              \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvvdfvf (char *r, char *x, char *px, char *y, float py) { \
    implname impl;                                                      \
    impl (DVEC(r), Dual2<Vec3>(VEC(x)), DFLOAT(y), VEC(px), py);        \
}




#define PNOISE_IMPL_DERIV_OPT(opname,implname)                          \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdff (ustringhash_pod  name_, char *r, char *x, float px, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DFLOAT(r), DFLOAT(x), px, (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdfdfff (ustringhash_pod  name_, char *r, char *x, char *y, float px, float py, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DFLOAT(r), DFLOAT(x), DFLOAT(y), px, py, (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdvv (ustringhash_pod  name_, char *r, char *x, char *px, char *sg, char *opt) {  \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DFLOAT(r), DVEC(x), VEC(px), (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dfdvdfvf (ustringhash_pod  name_, char *r, char *x, char *y, char *px, float py, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DFLOAT(r), DVEC(x), DFLOAT(y), VEC(px), py, (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdff (ustringhash_pod  name_, char *r, char *x, float px, char *sg, char *opt) {  \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DVEC(r), DFLOAT(x), px, (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdfdfff (ustringhash_pod  name_, char *r, char *x, char *y, float px, float py, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DVEC(r), DFLOAT(x), DFLOAT(y), px, py, (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdvv (ustringhash_pod  name_, char *r, char *x, char *px, char *sg, char *opt) {  \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DVEC(r), DVEC(x), VEC(px), (ShaderGlobals *)sg, (NoiseParams *)opt); \
}                                                                       \
                                                                        \
OSL_SHADEOP OSL_HOSTDEVICE void osl_ ##opname## _dvdvdfvf (ustringhash_pod  name_, char *r, char *x, char *y, char *px, float py, char *sg, char *opt) { \
    implname impl;                                                      \
    ustringhash name = ustringhash_from(name_);                         \
    impl (name, DVEC(r), DVEC(x), DFLOAT(y), VEC(px), py, (ShaderGlobals *)sg, (NoiseParams *)opt); \
}




PNOISE_IMPL (pcellnoise, PeriodicCellNoise)
PNOISE_IMPL (phashnoise, PeriodicHashNoise)
PNOISE_IMPL (pnoise, PeriodicNoise)
PNOISE_IMPL_DERIV (pnoise, PeriodicNoise)
PNOISE_IMPL (psnoise, PeriodicSNoise)
PNOISE_IMPL_DERIV (psnoise, PeriodicSNoise)

// clang-format off



// NB: We are excluding noise functions that require (u)string arguments
//     in the CUDA case, since strings are not currently well-supported
//     by the PTX backend. We will update this once string support has
//     been improved.

struct GaborNoise {
    OSL_HOSTDEVICE GaborNoise() {}

    // Gabor always uses derivatives, so dual versions only

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<float>& result,
                           const Dual2<float>& x, ShaderGlobals* /*sg*/,
                           const NoiseParams* opt) const
    {
        result = gabor(x, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<float>& result,
                           const Dual2<float>& x, const Dual2<float>& y,
                           ShaderGlobals* /*sg*/, const NoiseParams* opt) const
    {
        result = gabor(x, y, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<float>& result,
                           const Dual2<Vec3>& p, ShaderGlobals* /*sg*/,
                           const NoiseParams* opt) const
    {
        result = gabor(p, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<float>& result,
                           const Dual2<Vec3>& p, const Dual2<float>& /*t*/,
                           ShaderGlobals* /*sg*/, const NoiseParams* opt) const
    {
        // FIXME -- This is very broken, we are ignoring 4D!
        result = gabor(p, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<Vec3>& result,
                           const Dual2<float>& x, ShaderGlobals* /*sg*/,
                           const NoiseParams* opt) const
    {
        result = gabor3(x, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<Vec3>& result,
                           const Dual2<float>& x, const Dual2<float>& y,
                           ShaderGlobals* /*sg*/, const NoiseParams* opt) const
    {
        result = gabor3(x, y, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<Vec3>& result,
                           const Dual2<Vec3>& p, ShaderGlobals* /*sg*/,
                           const NoiseParams* opt) const
    {
        result = gabor3(p, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<Vec3>& result,
                           const Dual2<Vec3>& p, const Dual2<float>& /*t*/,
                           ShaderGlobals* /*sg*/, const NoiseParams* opt) const
    {
        // FIXME -- This is very broken, we are ignoring 4D!
        result = gabor3(p, opt);
    }
};



struct GaborPNoise {
    OSL_HOSTDEVICE GaborPNoise() {}

    // Gabor always uses derivatives, so dual versions only

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<float>& result,
                           const Dual2<float>& x, float px,
                           ShaderGlobals* /*sg*/, const NoiseParams* opt) const
    {
        result = pgabor(x, px, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<float>& result,
                           const Dual2<float>& x, const Dual2<float>& y,
                           float px, float py, ShaderGlobals* /*sg*/,
                           const NoiseParams* opt) const
    {
        result = pgabor(x, y, px, py, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<float>& result,
                           const Dual2<Vec3>& p, const Vec3& pp,
                           ShaderGlobals* /*sg*/, const NoiseParams* opt) const
    {
        result = pgabor(p, pp, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<float>& result,
                           const Dual2<Vec3>& p, const Dual2<float>& /*t*/,
                           const Vec3& pp, float /*tp*/, ShaderGlobals* /*sg*/,
                           const NoiseParams* opt) const
    {
        // FIXME -- This is very broken, we are ignoring 4D!
        result = pgabor(p, pp, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<Vec3>& result,
                           const Dual2<float>& x, float px,
                           ShaderGlobals* /*sg*/, const NoiseParams* opt) const
    {
        result = pgabor3(x, px, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<Vec3>& result,
                           const Dual2<float>& x, const Dual2<float>& y,
                           float px, float py, ShaderGlobals* /*sg*/,
                           const NoiseParams* opt) const
    {
        result = pgabor3(x, y, px, py, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<Vec3>& result,
                           const Dual2<Vec3>& p, const Vec3& pp,
                           ShaderGlobals* /*sg*/, const NoiseParams* opt) const
    {
        result = pgabor3(p, pp, opt);
    }

    OSL_HOSTDEVICE
    inline void operator()(ustringhash /*noisename*/, Dual2<Vec3>& result,
                           const Dual2<Vec3>& p, const Dual2<float>& /*t*/,
                           const Vec3& pp, float /*tp*/, ShaderGlobals* /*sg*/,
                           const NoiseParams* opt) const
    {
        // FIXME -- This is very broken, we are ignoring 4D!
        result = pgabor3(p, pp, opt);
    }
};



NOISE_IMPL_DERIV_OPT(gabornoise, GaborNoise)
PNOISE_IMPL_DERIV_OPT(gaborpnoise, GaborPNoise)


// Turn off warnings about unused params, since the NullNoise methods are stubs.
OSL_PRAGMA_WARNING_PUSH
OSL_GCC_PRAGMA(GCC diagnostic ignored "-Wunused-parameter")


struct NullNoise {
    OSL_HOSTDEVICE NullNoise () { }
    OSL_HOSTDEVICE inline void operator() (float &result, float x) const { result = 0.0f; }
    OSL_HOSTDEVICE inline void operator() (float &result, float x, float y) const { result = 0.0f; }
    OSL_HOSTDEVICE inline void operator() (float &result, const Vec3 &p) const { result = 0.0f; }
    OSL_HOSTDEVICE inline void operator() (float &result, const Vec3 &p, float t) const { result = 0.0f; }
    OSL_HOSTDEVICE inline void operator() (Vec3 &result, float x) const { result = v(); }
    OSL_HOSTDEVICE inline void operator() (Vec3 &result, float x, float y) const { result = v(); }
    OSL_HOSTDEVICE inline void operator() (Vec3 &result, const Vec3 &p) const { result = v(); }
    OSL_HOSTDEVICE inline void operator() (Vec3 &result, const Vec3 &p, float t) const { result = v(); }
    OSL_HOSTDEVICE inline void operator() (Dual2<float> &result, const Dual2<float> &x,
                                           int seed=0) const { result.set (0.0f, 0.0f, 0.0f); }
    OSL_HOSTDEVICE inline void operator() (Dual2<float> &result, const Dual2<float> &x,
                                           const Dual2<float> &y, int seed=0) const { result.set (0.0f, 0.0f, 0.0f); }
    OSL_HOSTDEVICE inline void operator() (Dual2<float> &result, const Dual2<Vec3> &p,
                                           int seed=0) const { result.set (0.0f, 0.0f, 0.0f); }
    OSL_HOSTDEVICE inline void operator() (Dual2<float> &result, const Dual2<Vec3> &p,
                                           const Dual2<float> &t, int seed=0) const { result.set (0.0f, 0.0f, 0.0f); }
    OSL_HOSTDEVICE inline void operator() (Dual2<Vec3> &result, const Dual2<float> &x) const { result.set (v(), v(), v()); }
    OSL_HOSTDEVICE inline void operator() (Dual2<Vec3> &result, const Dual2<float> &x, const Dual2<float> &y) const {  result.set (v(), v(), v()); }
    OSL_HOSTDEVICE inline void operator() (Dual2<Vec3> &result, const Dual2<Vec3> &p) const {  result.set (v(), v(), v()); }
    OSL_HOSTDEVICE inline void operator() (Dual2<Vec3> &result, const Dual2<Vec3> &p, const Dual2<float> &t) const { result.set (v(), v(), v()); }
    OSL_HOSTDEVICE inline Vec3 v () const { return Vec3(0.0f, 0.0f, 0.0f); };
};

struct UNullNoise {
    OSL_HOSTDEVICE UNullNoise () { }
    OSL_HOSTDEVICE inline void operator() (float &result, float x) const { result = 0.5f; }
    OSL_HOSTDEVICE inline void operator() (float &result, float x, float y) const { result = 0.5f; }
    OSL_HOSTDEVICE inline void operator() (float &result, const Vec3 &p) const { result = 0.5f; }
    OSL_HOSTDEVICE inline void operator() (float &result, const Vec3 &p, float t) const { result = 0.5f; }
    OSL_HOSTDEVICE inline void operator() (Vec3 &result, float x) const { result = v(); }
    OSL_HOSTDEVICE inline void operator() (Vec3 &result, float x, float y) const { result = v(); }
    OSL_HOSTDEVICE inline void operator() (Vec3 &result, const Vec3 &p) const { result = v(); }
    OSL_HOSTDEVICE inline void operator() (Vec3 &result, const Vec3 &p, float t) const { result = v(); }
    OSL_HOSTDEVICE inline void operator() (Dual2<float> &result, const Dual2<float> &x,
                                           int seed=0) const { result.set (0.5f, 0.5f, 0.5f); }
    OSL_HOSTDEVICE inline void operator() (Dual2<float> &result, const Dual2<float> &x,
                                           const Dual2<float> &y, int seed=0) const { result.set (0.5f, 0.5f, 0.5f); }
    OSL_HOSTDEVICE inline void operator() (Dual2<float> &result, const Dual2<Vec3> &p,
                                           int seed=0) const { result.set (0.5f, 0.5f, 0.5f); }
    OSL_HOSTDEVICE inline void operator() (Dual2<float> &result, const Dual2<Vec3> &p,
                                           const Dual2<float> &t, int seed=0) const { result.set (0.5f, 0.5f, 0.5f); }
    OSL_HOSTDEVICE inline void operator() (Dual2<Vec3> &result, const Dual2<float> &x) const { result.set (v(), v(), v()); }
    OSL_HOSTDEVICE inline void operator() (Dual2<Vec3> &result, const Dual2<float> &x, const Dual2<float> &y) const {  result.set (v(), v(), v()); }
    OSL_HOSTDEVICE inline void operator() (Dual2<Vec3> &result, const Dual2<Vec3> &p) const {  result.set (v(), v(), v()); }
    OSL_HOSTDEVICE inline void operator() (Dual2<Vec3> &result, const Dual2<Vec3> &p, const Dual2<float> &t) const { result.set (v(), v(), v()); }
    OSL_HOSTDEVICE inline Vec3 v () const { return Vec3(0.5f, 0.5f, 0.5f); };
};

NOISE_IMPL(nullnoise, NullNoise)
NOISE_IMPL_DERIV(nullnoise, NullNoise)
NOISE_IMPL(unullnoise, UNullNoise)
NOISE_IMPL_DERIV(unullnoise, UNullNoise)

OSL_PRAGMA_WARNING_POP



struct GenericNoise {
    OSL_HOSTDEVICE GenericNoise() {}

    // Template on R, S, and T to be either float or Vec3

    // dual versions -- this is always called with derivs

    template<class R, class S>
    OSL_HOSTDEVICE inline void operator()(ustringhash name, Dual2<R>& result,
                                          const Dual2<S>& s, ShaderGlobals* sg,
                                          const NoiseParams* opt) const
    {
        if (name == Hashes::uperlin || name == Hashes::noise) {
            Noise noise;
            noise(result, s);
        } else if (name == Hashes::perlin
                   || name == Hashes::snoise) {
            SNoise snoise;
            snoise(result, s);
        } else if (name == Hashes::simplexnoise
                   || name == Hashes::simplex) {
            SimplexNoise simplexnoise;
            simplexnoise(result, s);
        } else if (name == Hashes::usimplexnoise
                   || name == Hashes::usimplex) {
            USimplexNoise usimplexnoise;
            usimplexnoise(result, s);
        } else if (name == Hashes::cell) {
            CellNoise cellnoise;
            cellnoise(result.val(), s.val());
            result.clear_d();
        } else if (name == Hashes::gabor) {
            GaborNoise gnoise;
            gnoise(name, result, s, sg, opt);
        } else if (name == Hashes::null) {
            NullNoise noise;
            noise(result, s);
        } else if (name == Hashes::unull) {
            UNullNoise noise;
            noise(result, s);
        } else if (name == Hashes::hash) {
            HashNoise hashnoise;
            hashnoise(result.val(), s.val());
            result.clear_d();
        } else {
#    ifndef __CUDA_ARCH__
            OSL::errorfmt(sg, "Unknown noise type \"{}\"", name);
#    else
            // TODO: find a way to signal this error on the GPU
            result.clear_d();
#    endif
        }
    }

    template<class R, class S, class T>
    OSL_HOSTDEVICE inline void operator()(ustringhash name, Dual2<R>& result,
                                          const Dual2<S>& s, const Dual2<T>& t,
                                          ShaderGlobals* sg,
                                          const NoiseParams* opt) const
    {
        if (name == Hashes::uperlin || name == Hashes::noise) {
            Noise noise;
            noise(result, s, t);
        } else if (name == Hashes::perlin
                   || name == Hashes::snoise) {
            SNoise snoise;
            snoise(result, s, t);
        } else if (name == Hashes::simplexnoise
                   || name == Hashes::simplex) {
            SimplexNoise simplexnoise;
            simplexnoise(result, s, t);
        } else if (name == Hashes::usimplexnoise
                   || name == Hashes::usimplex) {
            USimplexNoise usimplexnoise;
            usimplexnoise(result, s, t);
        } else if (name == Hashes::cell) {
            CellNoise cellnoise;
            cellnoise(result.val(), s.val(), t.val());
            result.clear_d();
        } else if (name == Hashes::gabor) {
            GaborNoise gnoise;
            gnoise(name, result, s, t, sg, opt);
        } else if (name == Hashes::null) {
            NullNoise noise;
            noise(result, s, t);
        } else if (name == Hashes::unull) {
            UNullNoise noise;
            noise(result, s, t);
        } else if (name == Hashes::hash) {
            HashNoise hashnoise;
            hashnoise(result.val(), s.val(), t.val());
            result.clear_d();
        } else {
#    ifndef __CUDA_ARCH__
            OSL::errorfmt(sg, "Unknown noise type \"{}\"", name);
#    else
            // TODO: find a way to signal this error on the GPU
            result.clear_d();
#    endif
        }
    }
};


NOISE_IMPL_DERIV_OPT(genericnoise, GenericNoise)


struct GenericPNoise {
    OSL_HOSTDEVICE GenericPNoise() {}

    // Template on R, S, and T to be either float or Vec3

    // dual versions -- this is always called with derivs

    template<class R, class S>
    OSL_HOSTDEVICE inline void
    operator()(ustringhash name, Dual2<R>& result, const Dual2<S>& s,
               const S& sp, ShaderGlobals* sg, const NoiseParams* opt) const
    {
        if (name == Hashes::uperlin || name == Hashes::noise) {
            PeriodicNoise noise;
            noise(result, s, sp);
        } else if (name == Hashes::perlin
                   || name == Hashes::snoise) {
            PeriodicSNoise snoise;
            snoise(result, s, sp);
        } else if (name == Hashes::cell) {
            PeriodicCellNoise cellnoise;
            cellnoise(result.val(), s.val(), sp);
            result.clear_d();
        } else if (name == Hashes::gabor) {
            GaborPNoise gnoise;
            gnoise(name, result, s, sp, sg, opt);
        } else if (name == Hashes::hash) {
            PeriodicHashNoise hashnoise;
            hashnoise(result.val(), s.val(), sp);
            result.clear_d();
        } else {
#    ifndef __CUDA_ARCH__
            OSL::errorfmt(sg, "Unknown noise type \"{}\"", name);
#    else
            // TODO: find a way to signal this error on the GPU
            result.clear_d();
#    endif
        }
    }

    template<class R, class S, class T>
    OSL_HOSTDEVICE inline void
    operator()(ustringhash name, Dual2<R>& result, const Dual2<S>& s,
               const Dual2<T>& t, const S& sp, const T& tp, ShaderGlobals* sg,
               const NoiseParams* opt) const
    {
        if (name == Hashes::uperlin || name == Hashes::noise) {
            PeriodicNoise noise;
            noise(result, s, t, sp, tp);
        } else if (name == Hashes::perlin
                   || name == Hashes::snoise) {
            PeriodicSNoise snoise;
            snoise(result, s, t, sp, tp);
        } else if (name == Hashes::cell) {
            PeriodicCellNoise cellnoise;
            cellnoise(result.val(), s.val(), t.val(), sp, tp);
            result.clear_d();
        } else if (name == Hashes::gabor) {
            GaborPNoise gnoise;
            gnoise(name, result, s, t, sp, tp, sg, opt);
        } else if (name == Hashes::hash) {
            PeriodicHashNoise hashnoise;
            hashnoise(result.val(), s.val(), t.val(), sp, tp);
            result.clear_d();
        } else {
#    ifndef __CUDA_ARCH__
            OSL::errorfmt(sg, "Unknown noise type \"{}\"", name);
#    else
            // TODO: find a way to signal this error on the GPU
            result.clear_d();
#    endif
        }
    }
};


PNOISE_IMPL_DERIV_OPT(genericpnoise, GenericPNoise)


OSL_SHADEOP OSL_HOSTDEVICE void
osl_init_noise_options(void* sg_, void* opt)
{
    new (opt) NoiseParams;
}



OSL_SHADEOP OSL_HOSTDEVICE void
osl_noiseparams_set_anisotropic(void* opt, int a)
{
    ((NoiseParams*)opt)->anisotropic = a;
}



OSL_SHADEOP OSL_HOSTDEVICE void
osl_noiseparams_set_do_filter(void* opt, int a)
{
    ((NoiseParams*)opt)->do_filter = a;
}



OSL_SHADEOP OSL_HOSTDEVICE void
osl_noiseparams_set_direction(void* opt, void* dir)
{
    ((NoiseParams*)opt)->direction = VEC(dir);
}



OSL_SHADEOP OSL_HOSTDEVICE void
osl_noiseparams_set_bandwidth(void* opt, float b)
{
    ((NoiseParams*)opt)->bandwidth = b;
}



OSL_SHADEOP OSL_HOSTDEVICE void
osl_noiseparams_set_impulses(void* opt, float i)
{
    ((NoiseParams*)opt)->impulses = i;
}



OSL_SHADEOP void
osl_count_noise(void* sg_)
{
    ShaderGlobals* sg = (ShaderGlobals*)sg_;
    sg->context->shadingsys().count_noise();
}



OSL_SHADEOP OSL_HOSTDEVICE int
osl_hash_ii(int x)
{
    return inthashi(x);
}

OSL_SHADEOP OSL_HOSTDEVICE int
osl_hash_if(float x)
{
    return inthashf(x);
}

OSL_SHADEOP OSL_HOSTDEVICE int
osl_hash_iff(float x, float y)
{
    return inthashf(x, y);
}


OSL_SHADEOP OSL_HOSTDEVICE int
osl_hash_iv(void* x)
{
    return inthashf(static_cast<float*>(x));
}


OSL_SHADEOP OSL_HOSTDEVICE int
osl_hash_ivf(void* x, float y)
{
    return inthashf(static_cast<float*>(x), y);
}


}  // namespace pvt
OSL_NAMESPACE_END

#endif
