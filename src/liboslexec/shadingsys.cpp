// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "oslexec_pvt.h"
#include <OSL/fmt_util.h>
#include <OSL/genclosure.h>
#include "backendllvm.h"
#if OSL_USE_BATCHED
#    include "batched_backendllvm.h"
#    include <OSL/wide.h>
#endif
#include <OSL/oslquery.h>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/fmath.h>
#include <OpenImageIO/optparser.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>
#include <OpenImageIO/timer.h>

#include "opcolor.h"

using namespace OSL;
using namespace OSL::pvt;

#include <Imath/ImathConfig.h>  // Just for IMATH_VERSION_STRING

// avoid naming conflicts with MSVC macros
#ifdef _MSC_VER
#    undef RGB
// We use some of the iso646.h macro names later on in this file. For
// some compilers (MSVS, I'm looking at you) this is trouble. I don't know
// how or why that header would have been included here, but it did for at
// least one person, so shut off those macros so they don't cause trouble.
#    undef and
#    undef or
#    undef xor
#    undef compl
#    undef bitand
#    undef bitor
#endif


#ifdef OSL_LLVM_CUDA_BITCODE
extern int shadeops_cuda_ptx_compiled_ops_size;
extern unsigned char shadeops_cuda_ptx_compiled_ops_block[];
#endif


OSL_NAMESPACE_BEGIN



ShadingSystem::ShadingSystem(RendererServices* renderer,
                             TextureSystem* texturesystem, ErrorHandler* err)
    : m_impl(NULL)
{
    if (!err) {
        err = &ErrorHandler::default_handler();
    }
    m_impl = new ShadingSystemImpl(renderer, texturesystem, err);
#ifndef NDEBUG
    err->infofmt("creating new ShadingSystem {:p}", (void*)this);
#endif
}



ShadingSystem::~ShadingSystem()
{
    delete m_impl;
}



bool
ShadingSystem::attribute(string_view name, TypeDesc type, const void* val)
{
    return m_impl->attribute(name, type, val);
}



bool
ShadingSystem::attribute(ShaderGroup* group, string_view name, TypeDesc type,
                         const void* val)
{
    return m_impl->attribute(group, name, type, val);
}



bool
ShadingSystem::getattribute(string_view name, TypeDesc type, void* val)
{
    return m_impl->getattribute(name, type, val);
}



bool
ShadingSystem::getattribute(ShaderGroup* group, string_view name, TypeDesc type,
                            void* val)
{
    return m_impl->getattribute(group, name, type, val);
}



bool
ShadingSystem::LoadMemoryCompiledShader(string_view shadername,
                                        string_view buffer)
{
    return m_impl->LoadMemoryCompiledShader(shadername, buffer);
}



ShaderGroupRef
ShadingSystem::ShaderGroupBegin(string_view groupname)
{
    return m_impl->ShaderGroupBegin(groupname);
}



ShaderGroupRef
ShadingSystem::ShaderGroupBegin(string_view groupname, string_view usage,
                                string_view groupspec)
{
    return m_impl->ShaderGroupBegin(groupname, usage, groupspec);
}



bool
ShadingSystem::ShaderGroupEnd(ShaderGroup& group)
{
    return m_impl->ShaderGroupEnd(group);
}


bool
ShadingSystem::ShaderGroupEnd(void)
{
    return m_impl->ShaderGroupEnd();
}



bool
ShadingSystem::Parameter(ShaderGroup& group, string_view name, TypeDesc t,
                         const void* val, ParamHints hints)
{
    return m_impl->Parameter(group, name, t, val, hints);
}



bool
ShadingSystem::Parameter(string_view name, TypeDesc t, const void* val,
                         ParamHints hints)
{
    return m_impl->Parameter(name, t, val, hints);
}



bool
ShadingSystem::Shader(ShaderGroup& group, string_view shaderusage,
                      string_view shadername, string_view layername)
{
    return m_impl->Shader(group, shaderusage, shadername, layername);
}



bool
ShadingSystem::Shader(string_view shaderusage, string_view shadername,
                      string_view layername)
{
    return m_impl->Shader(shaderusage, shadername, layername);
}



bool
ShadingSystem::ConnectShaders(ShaderGroup& group, string_view srclayer,
                              string_view srcparam, string_view dstlayer,
                              string_view dstparam)
{
    return m_impl->ConnectShaders(group, srclayer, srcparam, dstlayer,
                                  dstparam);
}



bool
ShadingSystem::ConnectShaders(string_view srclayer, string_view srcparam,
                              string_view dstlayer, string_view dstparam)
{
    return m_impl->ConnectShaders(srclayer, srcparam, dstlayer, dstparam);
}



bool
ShadingSystem::ReParameter(ShaderGroup& group, string_view layername,
                           string_view paramname, TypeDesc type,
                           const void* val)
{
    return m_impl->ReParameter(group, layername, paramname, type, val);
}



PerThreadInfo*
ShadingSystem::create_thread_info()
{
    return m_impl->create_thread_info();
}



void
ShadingSystem::destroy_thread_info(PerThreadInfo* threadinfo)
{
    return m_impl->destroy_thread_info(threadinfo);
}



ShadingContext*
ShadingSystem::get_context(PerThreadInfo* threadinfo,
                           TextureSystem::Perthread* texture_threadinfo)
{
    return m_impl->get_context(threadinfo, texture_threadinfo);
}



void
ShadingSystem::release_context(ShadingContext* ctx)
{
    return m_impl->release_context(ctx);
}



bool
ShadingSystem::execute(ShadingContext& ctx, ShaderGroup& group,
                       int thread_index, int shade_index,
                       ShaderGlobals& globals, void* userdata_base_ptr,
                       void* output_base_ptr, bool run)
{
    return m_impl->execute(ctx, group, thread_index, shade_index, globals,
                           userdata_base_ptr, output_base_ptr, run);
}



bool
ShadingSystem::execute_init(ShadingContext& ctx, ShaderGroup& group,
                            int thread_index, int shade_index,
                            ShaderGlobals& globals, void* userdata_base_ptr,
                            void* output_base_ptr, bool run)
{
    return ctx.execute_init(group, thread_index, shade_index, globals,
                            userdata_base_ptr, output_base_ptr, run);
}



bool
ShadingSystem::execute_layer(ShadingContext& ctx, int thread_index,
                             int shade_index, ShaderGlobals& globals,
                             void* userdata_base_ptr, void* output_base_ptr,
                             int layernumber)
{
    return ctx.execute_layer(thread_index, shade_index, globals,
                             userdata_base_ptr, output_base_ptr, layernumber);
}



bool
ShadingSystem::execute_layer(ShadingContext& ctx, int thread_index,
                             int shade_index, ShaderGlobals& globals,
                             void* userdata_base_ptr, void* output_base_ptr,
                             ustring layername)
{
    int layernumber = find_layer(*ctx.group(), layername);
    return layernumber >= 0 ? ctx.execute_layer(thread_index, shade_index,
                                                globals, userdata_base_ptr,
                                                output_base_ptr, layernumber)
                            : false;
}



bool
ShadingSystem::execute_layer(ShadingContext& ctx, int thread_index,
                             int shade_index, ShaderGlobals& globals,
                             void* userdata_base_ptr, void* output_base_ptr,
                             const ShaderSymbol* symbol)
{
    if (!symbol)
        return false;
    const Symbol* sym = reinterpret_cast<const Symbol*>(symbol);
    int layernumber   = sym->layer();
    return layernumber >= 0 ? ctx.execute_layer(thread_index, shade_index,
                                                globals, userdata_base_ptr,
                                                output_base_ptr, layernumber)
                            : false;
}

#if OSL_USE_BATCHED
template<int WidthT>
bool
ShadingSystem::BatchedExecutor<WidthT>::execute(
    ShadingContext& ctx, ShaderGroup& group, int batch_size,
    Wide<const int, WidthT> wide_shadeindex,
    BatchedShaderGlobals<WidthT>& globals_batch, void* userdata_base_ptr,
    void* output_base_ptr, bool run)
{
    return ctx.batched<WidthT>().execute(group, batch_size, wide_shadeindex,
                                         globals_batch, userdata_base_ptr,
                                         output_base_ptr, run);
}

template<int WidthT>
bool
ShadingSystem::BatchedExecutor<WidthT>::execute_init(
    ShadingContext& ctx, ShaderGroup& group, int batch_size,
    Wide<const int, WidthT> wide_shadeindex,
    BatchedShaderGlobals<WidthT>& globals_batch, void* userdata_base_ptr,
    void* output_base_ptr, bool run)
{
    return ctx.batched<WidthT>().execute_init(group, batch_size,
                                              wide_shadeindex, globals_batch,
                                              userdata_base_ptr,
                                              output_base_ptr, run);
}


template<int WidthT>
bool
ShadingSystem::BatchedExecutor<WidthT>::execute_layer(
    ShadingContext& ctx, int batch_size,
    Wide<const int, WidthT> wide_shadeindex,
    BatchedShaderGlobals<WidthT>& globals_batch, void* userdata_base_ptr,
    void* output_base_ptr, int layernumber)
{
    return ctx.batched<WidthT>().execute_layer(batch_size, wide_shadeindex,
                                               globals_batch, userdata_base_ptr,
                                               output_base_ptr, layernumber);
}

template<int WidthT>
bool
ShadingSystem::BatchedExecutor<WidthT>::execute_layer(
    ShadingContext& ctx, int batch_size,
    Wide<const int, WidthT> wide_shadeindex,
    BatchedShaderGlobals<WidthT>& globals_batch, void* userdata_base_ptr,
    void* output_base_ptr, ustring layername)
{
    int layernumber = m_shading_system.find_layer(*ctx.group(), layername);
    return (layernumber >= 0) ? ctx.batched<WidthT>().execute_layer(
               batch_size, wide_shadeindex, globals_batch, userdata_base_ptr,
               output_base_ptr, layernumber)
                              : false;
}

template<int WidthT>
bool
ShadingSystem::BatchedExecutor<WidthT>::execute_layer(
    ShadingContext& ctx, int batch_size,
    Wide<const int, WidthT> wide_shadeindex,
    BatchedShaderGlobals<WidthT>& globals_batch, void* userdata_base_ptr,
    void* output_base_ptr, const ShaderSymbol* symbol)
{
    OSL_ASSERT(symbol);
    const Symbol* sym = reinterpret_cast<const Symbol*>(symbol);
    int layernumber   = sym->layer();
    return (layernumber >= 0) ? ctx.batched<WidthT>().execute_layer(
               batch_size, wide_shadeindex, globals_batch, userdata_base_ptr,
               output_base_ptr, layernumber)
                              : false;
}
#endif

bool
ShadingSystem::execute_cleanup(ShadingContext& ctx)
{
    return ctx.execute_cleanup();
}



int
ShadingSystem::find_layer(const ShaderGroup& group, ustring layername) const
{
    return group.find_layer(layername);
}



const void*
ShadingSystem::get_symbol(const ShadingContext& ctx, ustring layername,
                          ustring symbolname, TypeDesc& type) const
{
    const ShaderSymbol* sym = find_symbol(*ctx.group(), layername, symbolname);
    if (sym) {
        type = symbol_typedesc(sym);
        return symbol_address(ctx, sym);
    }
    return NULL;
}



const void*
ShadingSystem::get_symbol(const ShadingContext& ctx, ustring symbolname,
                          TypeDesc& type) const
{
    ustring layername;
    size_t dot = symbolname.find('.');
    if (dot != ustring::npos) {
        // If the name contains a dot, it's intended to be layer.symbol
        layername  = ustring(symbolname, 0, dot);
        symbolname = ustring(symbolname, dot + 1);
    }
    return get_symbol(ctx, layername, symbolname, type);
}



const ShaderSymbol*
ShadingSystem::find_symbol(const ShaderGroup& group, ustring layername,
                           ustring symbolname) const
{
    if (!group.optimized())
        return NULL;  // has to be post-optimized
    return (const ShaderSymbol*)group.find_symbol(layername, symbolname);
}



const ShaderSymbol*
ShadingSystem::find_symbol(const ShaderGroup& group, ustring symbolname) const
{
    ustring layername;
    size_t dot = symbolname.find('.');
    if (dot != ustring::npos) {
        // If the name contains a dot, it's intended to be layer.symbol
        layername  = ustring(symbolname, 0, dot);
        symbolname = ustring(symbolname, dot + 1);
    }
    return find_symbol(group, layername, symbolname);
}



TypeDesc
ShadingSystem::symbol_typedesc(const ShaderSymbol* sym) const
{
    return sym ? ((const Symbol*)sym)->typespec().simpletype() : TypeDesc();
}



const void*
ShadingSystem::symbol_address(const ShadingContext& ctx,
                              const ShaderSymbol* sym) const
{
    OSL_DASSERT(sym != nullptr);
    return ctx.symbol_data(*(const Symbol*)sym);
}

#if OSL_USE_BATCHED
bool
ShadingSystem::configure_batch_execution_at(int width)
{
    auto requestedISA = LLVM_Util::lookup_isa_by_name(
        m_impl->llvm_jit_target());
    OSL_MAYBE_UNUSED bool target_requested = (requestedISA
                                              != TargetISA::UNKNOWN);
    OSL_MAYBE_UNUSED bool jit_fma          = m_impl->llvm_jit_fma();

    // Build defines preprocessor MACROS to identify which
    // target specific ISA's it is building OSL library functions for.
    // This could be changed to possibly search
    // a library path for existence of target specific libraries.
    switch (width) {
    case 16:
        switch (requestedISA) {
        case TargetISA::UNKNOWN:
            // fallthrough
        case TargetISA::AVX512:
            if (jit_fma) {
#    ifdef __OSL_SUPPORTS_b16_AVX512
                if (LLVM_Util::supports_isa(TargetISA::AVX512)) {
                    if (!target_requested)
                        m_impl->attribute("llvm_jit_target",
                                          LLVM_Util::target_isa_name(
                                              TargetISA::AVX512));
                    return true;
                }
#    endif
                if (target_requested) {
                    break;
                }
            }
            // fallthrough
        case TargetISA::AVX512_noFMA:
#    ifdef __OSL_SUPPORTS_b16_AVX512_noFMA
            if (LLVM_Util::supports_isa(TargetISA::AVX512_noFMA)) {
                if (!target_requested) {
                    m_impl->attribute("llvm_jit_target",
                                      LLVM_Util::target_isa_name(
                                          TargetISA::AVX512_noFMA));
                }
                m_impl->attribute("llvm_jit_fma", 0);
                return true;
            }
#    endif
            if (target_requested) {
                break;
            }
            // fallthrough
        default: return false;
        };
        return false;
    case 8:
        switch (requestedISA) {
        case TargetISA::UNKNOWN:
            // fallthrough
        case TargetISA::AVX512:
            if (jit_fma) {
#    ifdef __OSL_SUPPORTS_b8_AVX512
                if (LLVM_Util::supports_isa(TargetISA::AVX512)) {
                    if (!target_requested)
                        m_impl->attribute("llvm_jit_target",
                                          LLVM_Util::target_isa_name(
                                              TargetISA::AVX512));
                    return true;
                }
#    endif
                if (target_requested) {
                    break;
                }
            }
            // fallthrough
        case TargetISA::AVX512_noFMA:
#    ifdef __OSL_SUPPORTS_b8_AVX512_noFMA
            if (LLVM_Util::supports_isa(TargetISA::AVX512_noFMA)) {
                if (!target_requested) {
                    m_impl->attribute("llvm_jit_target",
                                      LLVM_Util::target_isa_name(
                                          TargetISA::AVX512_noFMA));
                }
                m_impl->attribute("llvm_jit_fma", 0);
                return true;
            }
#    endif
            if (target_requested) {
                break;
            }
            // fallthrough
        case TargetISA::AVX2:
            if (jit_fma) {
#    ifdef __OSL_SUPPORTS_b8_AVX2
                if (LLVM_Util::supports_isa(TargetISA::AVX2)) {
                    if (!target_requested)
                        m_impl->attribute("llvm_jit_target",
                                          LLVM_Util::target_isa_name(
                                              TargetISA::AVX2));
                    return true;
                }
#    endif
                if (target_requested) {
                    break;
                }
            }
            // fallthrough
        case TargetISA::AVX2_noFMA:
#    ifdef __OSL_SUPPORTS_b8_AVX2_noFMA
            if (LLVM_Util::supports_isa(TargetISA::AVX2_noFMA)) {
                if (!target_requested)
                    m_impl->attribute("llvm_jit_target",
                                      LLVM_Util::target_isa_name(
                                          TargetISA::AVX2_noFMA));
                m_impl->attribute("llvm_jit_fma", 0);
                return true;
            }
#    endif
            if (target_requested) {
                break;
            }
            // fallthrough
        case TargetISA::AVX:
#    ifdef __OSL_SUPPORTS_b8_AVX
            if (LLVM_Util::supports_isa(TargetISA::AVX)) {
                if (!target_requested)
                    m_impl->attribute("llvm_jit_target",
                                      LLVM_Util::target_isa_name(
                                          TargetISA::AVX));
                // AVX doesn't support FMA
                m_impl->attribute("llvm_jit_fma", 0);
                return true;
            }
#    endif
            if (target_requested) {
                break;
            }
            // fallthrough
        default: return false;
        };
        return false;
    case 4:
        switch (requestedISA) {
        case TargetISA::UNKNOWN:
            // fallthrough
        case TargetISA::x64:
#    ifdef __OSL_SUPPORTS_b4_SSE2
            if (LLVM_Util::supports_isa(TargetISA::x64)) {
                if (!target_requested)
                    m_impl->attribute("llvm_jit_target",
                                      LLVM_Util::target_isa_name(
                                          TargetISA::x64));
                // SSE2 doesn't support FMA
                m_impl->attribute("llvm_jit_fma", 0);
                return true;
            }
#    endif
            if (target_requested) {
                break;
            }
            // fallthrough
        default: return false;
        };
        return false;
    default: return false;
    }
}
#endif

std::string
ShadingSystem::getstats(int level) const
{
    return m_impl->getstats(level);
}



void
ShadingSystem::register_closure(string_view name, int id,
                                const ClosureParam* params,
                                PrepareClosureFunc prepare,
                                SetupClosureFunc setup)
{
    return m_impl->register_closure(name, id, params, prepare, setup);
}



bool
ShadingSystem::query_closure(const char** name, int* id,
                             const ClosureParam** params)
{
    return m_impl->query_closure(name, id, params);
}



static cspan<std::pair<ustring, SGBits>>
sgbit_table()
{
    // clang-format off
    static const std::pair<ustring,SGBits> table[] = {
        { ustring("P"),       SGBits::P },
        { ustring("I"),       SGBits::I },
        { ustring("N"),       SGBits::N },
        { ustring("Ng"),      SGBits::Ng },
        { ustring("u"),       SGBits::u },
        { ustring("v"),       SGBits::v },
        { ustring("dPdu"),    SGBits::dPdu },
        { ustring("dPdv"),    SGBits::dPdv },
        { ustring("time"),    SGBits::time },
        { ustring("dtime"),   SGBits::dtime },
        { ustring("dPdtime"), SGBits::dPdtime },
        { ustring("Ps"),      SGBits::Ps },
        { ustring("Ci"),      SGBits::Ci }
    };
    // clang-format on
    return cspan<std::pair<ustring, SGBits>>(table);
}



SGBits
ShadingSystem::globals_bit(ustring name)
{
    for (auto t : sgbit_table()) {
        if (name == t.first)
            return t.second;
    }
    return SGBits::None;
}



ustring
ShadingSystem::globals_name(SGBits bit)
{
    for (auto t : sgbit_table()) {
        if (bit == t.second)
            return t.first;
    }
    return ustring();
}



int
ShadingSystem::raytype_bit(ustring name)
{
    return m_impl->raytype_bit(name);
}



void
ShadingSystem::optimize_all_groups(int nthreads, bool do_jit)
{
    return m_impl->optimize_all_groups(nthreads, 0 /*mythread*/,
                                       1 /*totalthreads*/, do_jit);
}



TextureSystem*
ShadingSystem::texturesys() const
{
    return m_impl->texturesys();
}



RendererServices*
ShadingSystem::renderer() const
{
    return m_impl->renderer();
}



bool
ShadingSystem::archive_shadergroup(ShaderGroup* group, string_view filename)
{
    if (!group) {
        m_impl->error("archive_shadergroup: passed nullptr as group");
        return false;
    }
    return m_impl->archive_shadergroup(*group, filename);
}


bool
ShadingSystem::archive_shadergroup(ShaderGroup& group, string_view filename)
{
    return m_impl->archive_shadergroup(group, filename);
}


void
ShadingSystem::set_raytypes(ShaderGroup* group, int raytypes_on,
                            int raytypes_off)
{
    if (group)
        group->set_raytypes(raytypes_on, raytypes_off);
}


void
ShadingSystem::clear_symlocs()
{
    m_impl->clear_symlocs();
}



void
ShadingSystem::clear_symlocs(ShaderGroup* group)
{
    if (group)
        group->clear_symlocs();
    else
        clear_symlocs();  // no group specified, make it global
}



void
ShadingSystem::add_symlocs(cspan<SymLocationDesc> symlocs)
{
    m_impl->add_symlocs(symlocs);
}



void
ShadingSystem::add_symlocs(ShaderGroup* group, cspan<SymLocationDesc> symlocs)
{
    if (group)
        group->add_symlocs(symlocs);
    else
        add_symlocs(symlocs);  // no group specified, make it global
}



const SymLocationDesc*
ShadingSystem::find_symloc(ustring name) const
{
    return m_impl->find_symloc(name);
}



const SymLocationDesc*
ShadingSystem::find_symloc(const ShaderGroup* group, ustring name) const
{
    if (group)
        return group->find_symloc(name);
    else
        return m_impl->find_symloc(name);
}



const SymLocationDesc*
ShadingSystem::find_symloc(ustring name, SymArena arena) const
{
    return m_impl->find_symloc(name, arena);
}



const SymLocationDesc*
ShadingSystem::find_symloc(const ShaderGroup* group, ustring name,
                           SymArena arena) const
{
    if (group)
        return group->find_symloc(name, arena);
    else
        return m_impl->find_symloc(name, arena);
}



void
ShadingSystem::optimize_group(ShaderGroup* group, ShadingContext* ctx,
                              bool do_jit)
{
    if (group)
        m_impl->optimize_group(*group, ctx, do_jit);
}



void
ShadingSystem::optimize_group(ShaderGroup* group, int raytypes_on,
                              int raytypes_off, ShadingContext* ctx,
                              bool do_jit)
{
    // convenience function for backwards compatibility
    set_raytypes(group, raytypes_on, raytypes_off);
    optimize_group(group, ctx, do_jit);
}

#if OSL_USE_BATCHED
template<int WidthT>
void
ShadingSystem::BatchedExecutor<WidthT>::jit_group(ShaderGroup* group,
                                                  ShadingContext* ctx)
{
    OSL_ASSERT(group);
    m_shading_system.m_impl->batched<WidthT>().jit_group(*group, ctx);
}

template<int WidthT>
void
ShadingSystem::BatchedExecutor<WidthT>::jit_all_groups(int nthreads)
{
    OSL_ASSERT(0 && "To Be Implemented");
}

// Explicitly instantiate
template class ShadingSystem::BatchedExecutor<16>;
template class ShadingSystem::BatchedExecutor<8>;
template class ShadingSystem::BatchedExecutor<4>;
#endif


static TypeDesc TypeFloatArray2(TypeDesc::FLOAT, 2);
static TypeDesc TypeFloatArray3(TypeDesc::FLOAT, 3);
static TypeDesc TypeFloatArray4(TypeDesc::FLOAT, 4);



bool
ShadingSystem::convert_value(void* dst, TypeDesc dsttype, const void* src,
                             TypeDesc srctype)
{
    int tmp_int;
    if (srctype == TypeDesc::UINT8) {
        // uint8 src: Up-convert the source to int
        if (src) {
            tmp_int = *(const unsigned char*)src;
            src     = &tmp_int;
        }
        srctype = TypeInt;
    }

    float tmp_float;
    if (srctype == TypeInt && dsttype.basetype == TypeDesc::FLOAT) {
        // int -> float-based : up-convert the source to float
        if (src) {
            tmp_float = (float)(*(const int*)src);
            src       = &tmp_float;
        }
        srctype = TypeFloat;
    }

    // Just copy equivalent types
    if (equivalent(dsttype, srctype)) {
        if (dst && src)
            memmove(dst, src, dsttype.size());
        return true;
    }

    if (srctype == TypeFloat) {
        // float->triple conversion
        if (equivalent(dsttype, TypePoint)) {
            if (dst && src) {
                float f = *(const float*)src;
                ((OSL::Vec3*)dst)->setValue(f, f, f);
            }
            return true;
        }
        // float->int
        if (dsttype == TypeInt) {
            if (dst && src)
                *(int*)dst = (int)*(const float*)src;
            return true;
        }
        // float->float[2]
        if (dsttype == TypeFloatArray2) {
            if (dst && src) {
                float f          = *(const float*)src;
                ((float*)dst)[0] = f;
                ((float*)dst)[1] = f;
            }
            return true;
        }
        // float->float[4]
        if (dsttype == TypeFloatArray4) {
            if (dst && src) {
                float f          = *(const float*)src;
                ((float*)dst)[0] = f;
                ((float*)dst)[1] = f;
                ((float*)dst)[2] = f;
                ((float*)dst)[3] = f;
            }
            return true;
        }
        return false;  // Unsupported conversion
    }

    // float[3] -> triple
    if ((srctype == TypeFloatArray3 && equivalent(dsttype, TypePoint))
        || (dsttype == TypeFloatArray3 && equivalent(srctype, TypePoint))) {
        if (dst && src)
            memmove(dst, src, dsttype.size());
        return true;
    }

    // float[4] -> vec4
    if ((srctype == TypeFloatArray4 && equivalent(dsttype, TypeFloat4))
        || (dsttype == TypeFloatArray4 && equivalent(srctype, TypeFloat4))) {
        if (dst && src)
            memmove(dst, src, dsttype.size());
        return true;
    }

    // float[2] -> triple
    if (srctype == TypeFloatArray2 && equivalent(dsttype, TypePoint)) {
        if (dst && src) {
            float f0 = ((const float*)src)[0];
            float f1 = ((const float*)src)[1];
            ((OSL::Vec3*)dst)->setValue(f0, f1, 0.0f);
        }
        return true;
    }

    return false;  // Unsupported conversion
}



void
register_JIT_Global(const char* global_var_name, void* global_var_addr)
{
    LLVM_Util::add_global_mapping(global_var_name, global_var_addr);
}

PerThreadInfo::PerThreadInfo() {}



PerThreadInfo::~PerThreadInfo()
{
    while (!context_pool.empty())
        delete pop_context();
}



ShadingContext*
PerThreadInfo::pop_context()
{
    ShadingContext* sc = context_pool.top();
    context_pool.pop();
    return sc;
}



namespace Strings {
#define STRDECL(str, var_name) const ustring var_name(str);
#include <OSL/strdecls.h>
#undef STRDECL
}  // namespace Strings



namespace pvt {  // OSL::pvt


ShadingSystemImpl::ShadingSystemImpl(RendererServices* renderer,
                                     TextureSystem* texturesystem,
                                     ErrorHandler* err)
    : m_renderer(renderer)
    , m_texturesys(texturesystem)
    , m_err(err)
    , m_statslevel(0)
    , m_lazylayers(true)
    , m_lazyglobals(true)
    , m_lazyunconnected(true)
    , m_lazyerror(true)
    , m_lazy_userdata(false)
    , m_lazy_trace(true)
    , m_userdata_isconnected(false)
    , m_clearmemory(false)
    , m_debugnan(false)
    , m_debug_uninit(false)
    , m_lockgeom_default(true)
    , m_strict_messages(true)
    , m_error_repeats(false)
    , m_range_checking(true)
    , m_connection_error(true)
    , m_greedyjit(false)
    , m_countlayerexecs(false)
    , m_relaxed_param_typecheck(false)
    , m_profile(0)
    , m_optimize(2)
    , m_opt_simplify_param(true)
    , m_opt_constant_fold(true)
    , m_opt_stale_assign(true)
    , m_opt_elide_useless_ops(true)
    , m_opt_elide_unconnected_outputs(true)
    , m_opt_peephole(true)
    , m_opt_coalesce_temps(true)
    , m_opt_assign(true)
    , m_opt_mix(true)
    , m_opt_merge_instances(1)
    , m_opt_merge_instances_with_userdata(true)
    , m_opt_fold_getattribute(true)
    , m_opt_middleman(true)
    , m_opt_texture_handle(true)
    , m_opt_seed_bblock_aliases(true)
    , m_opt_useparam(false)
    , m_opt_groupdata(true)
#if OSL_USE_BATCHED
    , m_opt_batched_analysis((renderer->batched(WidthOf<16>()) != nullptr)
                             || (renderer->batched(WidthOf<8>()) != nullptr)
                             || (renderer->batched(WidthOf<4>()) != nullptr))
#else
    , m_opt_batched_analysis(false)
#endif
    , m_llvm_jit_fma(false)
    , m_llvm_jit_aggressive(false)
    , m_optimize_nondebug(false)
    , m_vector_width(4)
    , m_opt_passes(10)
    , m_llvm_optimize(1)
    , m_debug(0)
    , m_llvm_debug(0)
    , m_llvm_debug_layers(0)
    , m_llvm_debug_ops(0)
    , m_llvm_target_host(1)
    , m_llvm_debugging_symbols(0)
    , m_llvm_profiling_events(0)
    , m_llvm_output_bitcode(0)
    , m_llvm_dumpasm(0)
    , m_dump_forced_llvm_bool_symbols(0)
    , m_dump_uniform_symbols(0)
    , m_dump_varying_symbols(0)
    , m_max_local_mem_KB(2048)
    , m_compile_report(0)
    , m_use_optix(renderer->supports("OptiX"))
    , m_use_optix_cache(m_use_optix && renderer->supports("optix_ptx_cache"))
    , m_max_optix_groupdata_alloc(0)
    , m_buffer_printf(true)
    , m_no_noise(false)
    , m_no_pointcloud(false)
    , m_force_derivs(false)
    , m_allow_shader_replacement(false)
    , m_exec_repeat(1)
    , m_opt_warnings(0)
    , m_gpu_opt_error(0)
    , m_optix_no_inline(false)
    , m_optix_no_inline_layer_funcs(false)
    , m_optix_merge_layer_funcs(true)
    , m_optix_no_inline_rend_lib(false)
    , m_optix_no_inline_thresh(100000)
    , m_optix_force_inline_thresh(0)
    , m_colorspace("Rec709")
    , m_stat_opt_locking_time(0)
    , m_stat_specialization_time(0)
    , m_stat_total_llvm_time(0)
    , m_stat_llvm_setup_time(0)
    , m_stat_llvm_irgen_time(0)
    , m_stat_llvm_opt_time(0)
    , m_stat_llvm_jit_time(0)
    , m_stat_inst_merge_time(0)
    , m_stat_max_llvm_local_mem(0)
{
    m_shading_state_uniform.m_commonspace_synonym     = Strings::world;
    m_shading_state_uniform.m_unknown_coordsys_error  = true;
    m_shading_state_uniform.m_max_warnings_per_thread = 100;

    m_stat_shaders_loaded                    = 0;
    m_stat_shaders_requested                 = 0;
    m_stat_groups                            = 0;
    m_stat_groupinstances                    = 0;
    m_stat_instances_compiled                = 0;
    m_stat_groups_compiled                   = 0;
    m_stat_empty_instances                   = 0;
    m_stat_merged_inst                       = 0;
    m_stat_merged_inst_opt                   = 0;
    m_stat_empty_groups                      = 0;
    m_stat_regexes                           = 0;
    m_stat_preopt_syms                       = 0;
    m_stat_postopt_syms                      = 0;
    m_stat_syms_with_derivs                  = 0;
    m_stat_preopt_ops                        = 0;
    m_stat_postopt_ops                       = 0;
    m_stat_middlemen_eliminated              = 0;
    m_stat_const_connections                 = 0;
    m_stat_global_connections                = 0;
    m_stat_tex_calls_codegened               = 0;
    m_stat_tex_calls_as_handles              = 0;
    m_stat_useparam_ops                      = 0;
    m_stat_call_layers_inserted              = 0;
    m_stat_master_load_time                  = 0;
    m_stat_optimization_time                 = 0;
    m_stat_getattribute_time                 = 0;
    m_stat_getattribute_fail_time            = 0;
    m_stat_getattribute_calls                = 0;
    m_stat_get_userdata_calls                = 0;
    m_stat_noise_calls                       = 0;
    m_stat_pointcloud_searches               = 0;
    m_stat_pointcloud_searches_total_results = 0;
    m_stat_pointcloud_max_results            = 0;
    m_stat_pointcloud_failures               = 0;
    m_stat_pointcloud_gets                   = 0;
    m_stat_pointcloud_writes                 = 0;
    m_stat_layers_executed                   = 0;
    m_stat_total_shading_time_ticks          = 0;
    m_stat_reparam_calls_total               = 0;
    m_stat_reparam_bytes_total               = 0;
    m_stat_reparam_calls_changed             = 0;
    m_stat_reparam_bytes_changed             = 0;

    m_groups_to_compile_count     = 0;
    m_threads_currently_compiling = 0;

    // If client didn't supply an error handler, just use the default
    // one that echoes to the terminal.
    if (!m_err) {
        m_err = &ErrorHandler::default_handler();
    }

    // If client didn't supply a texture system, use the one already held
    // by the renderer (if it returns one).
    if (!m_texturesys)
        m_texturesys = renderer->texturesys();

    // If we still don't have a texture system, create a new one
    if (!m_texturesys) {
#if OSL_NO_DEFAULT_TEXTURESYSTEM
        // This build option instructs OSL to never create a TextureSystem
        // itself. (Most likely reason: this build of OSL is for a renderer
        // that replaces OIIO's TextureSystem with its own, and therefore
        // wouldn't want to accidentally make an OIIO one here.
        OSL_ASSERT(0
                   && "ShadingSystem was not passed a working TextureSystem*");
#else
#    if OIIO_TEXTURESYSTEM_CREATE_SHARED
        m_texturesys_sp = TextureSystem::create(true /* shared */);
        m_texturesys    = m_texturesys_sp.get();
#    else
        m_texturesys = TextureSystem::create(true /* shared */);
#    endif
        // Make some good guesses about default options
        m_texturesys->attribute("automip", 1);
        m_texturesys->attribute("autotile", 64);
#endif
    }

    // Alternate way of turning on LLVM debug mode (temporary/experimental)
    const char* llvm_debug_env = getenv("OSL_LLVM_DEBUG");
    if (llvm_debug_env && *llvm_debug_env)
        m_llvm_debug = atoi(llvm_debug_env);

    // Initialize a default set of raytype names.  A particular renderer
    // can override this, add custom names, or change the bits around,
    // if this default ordering is not to its liking.
    static const char* raytypes[]
        = { /*1*/ "camera",        /*2*/ "shadow",  /*4*/ "reflection",
            /*8*/ "refraction",
            /*16*/ "diffuse",      /*32*/ "glossy", /*64*/ "subsurface",
            /*128*/ "displacement" };
    const int nraytypes = sizeof(raytypes) / sizeof(raytypes[0]);
    attribute("raytypes", TypeDesc(TypeDesc::STRING, nraytypes), raytypes);

    // Allow environment variable to override default options
    const char* options = getenv("OSL_OPTIONS");
    if (options)
        attribute("options", TypeDesc::STRING, &options);

    setup_op_descriptors();

    colorsystem().set_colorspace(ustringhash_from(m_colorspace));
}



static void
shading_system_setup_op_descriptors(
    ShadingSystemImpl::OpDescriptorMap& op_descriptor)
{
    // clang-format off
#if OSL_USE_BATCHED
#define OP2(alias,name,ll,fold,simp,flag)                                \
    extern bool llvm_gen_##ll (BatchedBackendLLVM &rop, int opnum);      \
    extern bool llvm_gen_##ll (BackendLLVM &rop, int opnum);             \
    extern int  constfold_##fold (RuntimeOptimizer &rop, int opnum);     \
    op_descriptor[ustring(#alias)] = OpDescriptor(#name, llvm_gen_##ll,  \
                                                  llvm_gen_##ll,         \
                                                  constfold_##fold, simp, flag);
#else
#define OP2(alias,name,ll,fold,simp,flag)                                \
    extern bool llvm_gen_##ll (BackendLLVM &rop, int opnum);             \
    extern int  constfold_##fold (RuntimeOptimizer &rop, int opnum);     \
    op_descriptor[ustring(#alias)] = OpDescriptor(#name, llvm_gen_##ll,  \
                                                  constfold_##fold, simp, flag);
#endif

#define OP(name,ll,fold,simp,flag) OP2(name,name,ll,fold,simp,flag)
#define TEX OpDescriptor::Tex
#define SIDE OpDescriptor::SideEffects
#define STRCHARS OpDescriptor::StrChars
#define STRCREATE OpDescriptor::StrCreate

    // name          llvmgen              folder         simple     flags
    OP (aassign,     aassign,             aassign,       false,     0);
    OP (abs,         generic,             abs,           true,      0);
    OP (acos,        generic,             acos,          true,      0);
    OP (add,         add,                 add,           true,      0);
    OP (and,         andor,               and,           true,      0);
    OP (area,        area,                deriv,         true,      0);
    OP (aref,        aref,                aref,          true,      0);
    OP (arraycopy,   arraycopy,           none,          false,     0);
    OP (arraylength, arraylength,         arraylength,   true,      0);
    OP (asin,        generic,             asin,          true,      0);
    OP (assign,      assign,              none,          true,      0);
    OP (atan,        generic,             none,          true,      0);
    OP (atan2,       generic,             none,          true,      0);
    OP (backfacing,  get_simple_SG_field, none,          true,      0);
    OP (bitand,      bitwise_binary_op,   bitand,        true,      0);
    OP (bitor,       bitwise_binary_op,   bitor,         true,      0);
    OP (blackbody,   blackbody,           none,          true,      0);
    OP (break,       loopmod_op,          none,          false,     0);
    OP (calculatenormal, calculatenormal, none,          true,      0);
    OP (cbrt,        generic,             cbrt,          true,      0);
    OP (ceil,        generic,             ceil,          true,      0);
    OP (cellnoise,   noise,               noise,         true,      0);
    OP (clamp,       clamp,               clamp,         true,      0);
    OP (closure,     closure,             none,          true,      0);
    OP (color,       construct_color,     triple,        true,      0);
    OP (compassign,  compassign,          compassign,    false,     0);
    OP (compl,       unary_op,            compl,         true,      0);
    OP (compref,     compref,             compref,       true,      0);
    OP (concat,      generic,             concat,        true,      STRCREATE);
    OP (continue,    loopmod_op,          none,          false,     0);
    OP (cos,         generic,             cos,           true,      0);
    OP (cosh,        generic,             none,          true,      0);
    OP (cross,       generic,             none,          true,      0);
    OP (degrees,     generic,             degrees,       true,      0);
    OP (determinant, generic,             none,          true,      0);
    OP (dict_find,   dict_find,           none,          false,     0);
    OP (dict_next,   dict_next,           none,          false,     0);
    OP (dict_value,  dict_value,          none,          false,     0);
    OP (distance,    generic,             none,          true,      0);
    OP (div,         div,                 div,           true,      0);
    OP (dot,         generic,             dot,           true,      0);
    OP (Dx,          DxDy,                deriv,         true,      0);
    OP (Dy,          DxDy,                deriv,         true,      0);
    OP (Dz,          Dz,                  deriv,         true,      0);
    OP (dowhile,     loop_op,             none,          false,     0);
    OP (end,         end,                 none,          false,     0);
    OP (endswith,    generic,             endswith,      true,      STRCHARS);
    OP (environment, environment,         none,          true,      TEX);
    OP (eq,          compare_op,          eq,            true,      0);
    OP (erf,         generic,             erf,           true,      0);
    OP (erfc,        generic,             erfc,          true,      0);
    OP (error,       printf,              none,          false,     SIDE);
    OP (exit,        return,              none,          false,     0);
    OP (exp,         generic,             exp,           true,      0);
    OP (exp2,        generic,             exp2,          true,      0);
    OP (expm1,       generic,             expm1,         true,      0);
    OP (fabs,        generic,             abs,           true,      0);
    OP (filterwidth, filterwidth,         deriv,         true,      0);
    OP (floor,       generic,             floor,         true,      0);
    OP (fmod,        modulus,             none,          true,      0);
    OP (for,         loop_op,             none,          false,     0);
    OP (format,      printf,              format,        true,      STRCREATE);
    OP (fprintf,     printf,              none,          false,     SIDE);
    OP (functioncall, functioncall,       functioncall,  false,     0);
    OP (functioncall_nr,functioncall_nr,  none,          false,     0);
    OP (ge,          compare_op,          ge,            true,      0);
    OP (getattribute, getattribute,       getattribute,  false,     0);
    OP (getchar,      generic,            getchar,       true,      STRCHARS);
    OP (getmatrix,   getmatrix,           getmatrix,     false,     0);
    OP (getmessage,  getmessage,          getmessage,    false,     0);
    OP (gettextureinfo, gettextureinfo,   gettextureinfo,false,     TEX);
    OP (gt,          compare_op,          gt,            true,      0);
    OP (hash,        generic,             hash,          true,      0);
    OP (hashnoise,   noise,               noise,         true,      0);
    OP (if,          if,                  if,            false,     0);
    OP (inversesqrt, generic,             inversesqrt,   true,      0);
    OP (isconnected, generic,             none,          true,      0);
    OP (isconstant,  isconstant,          isconstant,    true,      0);
    OP (isfinite,    generic,             none,          true,      0);
    OP (isinf,       generic,             none,          true,      0);
    OP (isnan,       generic,             none,          true,      0);
    OP (le,          compare_op,          le,            true,      0);
    OP (length,      generic,             none,          true,      0);
    OP (log,         generic,             log,           true,      0);
    OP (log10,       generic,             log10,         true,      0);
    OP (log2,        generic,             log2,          true,      0);
    OP (logb,        generic,             logb,          true,      0);
    OP (lt,          compare_op,          lt,            true,      0);
    OP (luminance,   luminance,           none,          true,      0);
    OP (matrix,      matrix,              matrix,        true,      0);
    OP (max,         minmax,              max,           true,      0);
    OP (mxcompassign, mxcompassign,       mxcompassign,  false,     0);
    OP (mxcompref,   mxcompref,           none,          true,      0);
    OP (min,         minmax,              min,           true,      0);
    OP (mix,         mix,                 mix,           true,      0);
    OP (mod,         modulus,             mod,           true,      0);
    OP (mul,         mul,                 mul,           true,      0);
    OP (neg,         neg,                 neg,           true,      0);
    OP (neq,         compare_op,          neq,           true,      0);
    OP (noise,       noise,               noise,         true,      0);
    OP (nop,         nop,                 none,          true,      0);
    OP (normal,      construct_triple,    triple,        true,      0);
    OP (normalize,   generic,             normalize,     true,      0);
    OP (or,          andor,               or,            true,      0);
    OP (pnoise,      noise,               noise,         true,      0);
    OP (point,       construct_triple,    triple,        true,      0);
    OP (pointcloud_search, pointcloud_search, pointcloud_search,
                                                         false,     TEX);
    OP (pointcloud_get, pointcloud_get,   pointcloud_get,false,     TEX);
    OP (pointcloud_write, pointcloud_write, none,        false,     SIDE);
    OP (pow,         generic,             pow,           true,      0);
    OP (printf,      printf,              none,          false,     SIDE);
    OP (psnoise,     noise,               noise,         true,      0);
    OP (radians,     generic,             radians,       true,      0);
    OP (raytype,     raytype,             raytype,       true,      0);
    OP (regex_match, regex,               none,          false,     STRCHARS);
    OP (regex_search, regex,              regex_search,  false,     STRCHARS);
    OP (return,      return,              none,          false,     0);
    OP (round,       generic,             none,          true,      0);
    OP (select,      select,              select,        true,      0);
    OP (setmessage,  setmessage,          setmessage,    false,     SIDE);
    OP (shl,         bitwise_binary_op,   none,          true,      0);
    OP (shr,         bitwise_binary_op,   none,          true,      0);
    OP (sign,        generic,             none,          true,      0);
    OP (sin,         generic,             sin,           true,      0);
    OP (sincos,      sincos,              sincos,        false,     0);
    OP (sinh,        generic,             none,          true,      0);
    OP (smoothstep,  generic,             none,          true,      0);
    OP (snoise,      noise,               noise,         true,      0);
    OP (spline,      spline,              none,          true,      0);
    OP (splineinverse, spline,            none,          true,      0);
    OP (split,       split,               split,         false,     0);
    OP (sqrt,        generic,             sqrt,          true,      0);
    OP (startswith,  generic,             startswith,    true,      STRCHARS);
    OP (step,        generic,             none,          true,      0);
    OP (stof,        generic,             stof,          true,      STRCHARS);
    OP (stoi,        generic,             stoi,          true,      STRCHARS);
    OP (strlen,      generic,             strlen,        true,      STRCHARS);
    OP2(strtof,stof, generic,             stof,          true,      STRCHARS);
    OP2(strtoi,stoi, generic,             stoi,          true,      STRCHARS);
    OP (sub,         sub,                 sub,           true,      0);
    OP (substr,      generic,             substr,        true,      STRCHARS | STRCREATE);
    OP (surfacearea, get_simple_SG_field, none,          true,      0);
    OP (tan,         generic,             none,          true,      0);
    OP (tanh,        generic,             none,          true,      0);
    OP (texture,     texture,             texture,       true,      TEX);
    OP (texture3d,   texture3d,           none,          true,      TEX);
    OP (trace,       trace,               none,          false,     SIDE);
    OP (transform,   transform,           transform,     true,      0);
    OP (transformc,  transformc,          transformc,    true,      0);
    OP (transformn,  transform,           transform,     true,      0);
    OP (transformv,  transform,           transform,     true,      0);
    OP (transpose,   generic,             none,          true,      0);
    OP (trunc,       generic,             none,          true,      0);
    OP (useparam,    useparam,            useparam,      false,     0);
    OP (vector,      construct_triple,    triple,        true,      0);
    OP (warning,     printf,              warning,       false,     SIDE);
    OP (wavelength_color, blackbody,      none,          true,      0);
    OP (while,       loop_op,             none,          false,     0);
    OP (xor,         bitwise_binary_op,   xor,           true,      0);
#undef OP
#undef TEX
#undef SIDE
#undef STRCHARS
#undef STRCREATE
    // clang-format on
}



void
ShadingSystemImpl::setup_op_descriptors()
{
    // This is not a class member function to avoid namespace issues
    // with function declarations in the function body, when building
    // with visual studio.
    shading_system_setup_op_descriptors(m_op_descriptor);
}



void
ShadingSystemImpl::register_closure(string_view name, int id,
                                    const ClosureParam* params,
                                    PrepareClosureFunc prepare,
                                    SetupClosureFunc setup)
{
    for (int i = 0; params && params[i].type != TypeDesc(); ++i) {
        if (params[i].key == NULL
            && params[i].type.size() != (size_t)params[i].field_size) {
            errorfmt(
                "Parameter {} of '{}' closure is assigned to a field of incompatible size",
                i + 1, name);
            return;
        }
    }
    m_closure_registry.register_closure(name, id, params, prepare, setup);
}



bool
ShadingSystemImpl::query_closure(const char** name, int* id,
                                 const ClosureParam** params)
{
    if (!name && !id)
        return false;
    const ClosureRegistry::ClosureEntry* entry
        = (name && *name) ? m_closure_registry.get_entry(ustring(*name))
                          : m_closure_registry.get_entry(*id);
    if (!entry)
        return false;

    if (name)
        *name = entry->name.c_str();
    if (id)
        *id = entry->id;
    if (params)
        *params = &entry->params[0];

    return true;
}



ShadingSystemImpl::~ShadingSystemImpl()
{
    size_t ngroups = m_all_shader_groups.size();
    for (size_t i = 0; i < ngroups; ++i) {
        if (ShaderGroupRef g = m_all_shader_groups[i].lock()) {
            if (!g->jitted() || !g->batch_jitted()) {
                // As we are now lazier in jitting and need to keep the OSL IR
                // around in case we want to create a batched JIT or vice versa
                // we may have OSL IR to cleanup
                group_post_jit_cleanup(*g);
            }
        }
    }

    printstats();
    // N.B. just let m_texsys go -- if we asked for one to be created,
    // we asked for a shared one.

    // FIXME(boulos): According to the docs, we should also call
    // llvm_shutdown once we're done. However, ~ShadingSystemImpl
    // seems like the wrong place for this since in a multi-threaded
    // implementation we might destroy this impl while having others
    // outstanding. I'll leave this as a fixme for now.

    //llvm::llvm_shutdown();
}



// Return a comma-separated list of all the important SIMD/capabilities
// that were enabled as a compile-time option when OSL was built.
// (Keep this in sync with oiio_simd_caps in imageio.cpp).
static std::string
osl_simd_caps()
{
    // clang-format off
    std::vector<string_view> caps;
    if (OIIO_SIMD_SSE >= 2)      caps.emplace_back ("sse2");
    if (OIIO_SIMD_SSE >= 3)      caps.emplace_back ("sse3");
    if (OIIO_SIMD_SSE >= 3)      caps.emplace_back ("ssse3");
    if (OIIO_SIMD_SSE >= 4)      caps.emplace_back ("sse41");
    if (OIIO_SIMD_SSE >= 4)      caps.emplace_back ("sse42");
    if (OIIO_SIMD_AVX)           caps.emplace_back ("avx");
    if (OIIO_SIMD_AVX >= 2)      caps.emplace_back ("avx2");
    if (OIIO_SIMD_AVX >= 512)    caps.emplace_back ("avx512f");
    if (OIIO_AVX512DQ_ENABLED)   caps.emplace_back ("avx512dq");
    if (OIIO_AVX512IFMA_ENABLED) caps.emplace_back ("avx512ifma");
    if (OIIO_AVX512PF_ENABLED)   caps.emplace_back ("avx512pf");
    if (OIIO_AVX512CD_ENABLED)   caps.emplace_back ("avx512cd");
    if (OIIO_AVX512BW_ENABLED)   caps.emplace_back ("avx512bw");
    if (OIIO_AVX512VL_ENABLED)   caps.emplace_back ("avx512vl");
    if (OIIO_FMA_ENABLED)        caps.emplace_back ("fma");
    if (OIIO_F16C_ENABLED)       caps.emplace_back ("f16c");
    // if (OIIO_POPCOUNT_ENABLED)   caps.emplace_back ("popcnt");
    return OIIO::Strutil::join (caps, ",");
    // clang-format on
}



bool
ShadingSystemImpl::attribute(string_view name, TypeDesc type, const void* val)
{
#define ATTR_SET(_name, _ctype, _dst)                                  \
    if (name == _name && type == OIIO::BaseTypeFromC<_ctype>::value) { \
        _dst = *(_ctype*)(val);                                        \
        return true;                                                   \
    }
#define ATTR_SET_STRING(_name, _dst)                 \
    if (name == _name && type == TypeDesc::STRING) { \
        _dst = ustring(*(const char**)val);          \
        return true;                                 \
    }

#define ATTR_SET_STRINGHASH(_name, _dst)             \
    if (name == _name && type == TypeDesc::STRING) { \
        _dst = ustringhash(*(const char**)val);      \
        return true;                                 \
    }

    if (name == "options" && type == TypeDesc::STRING) {
        return OIIO::optparser(*this, *(const char**)val);
    }

    lock_guard guard(m_mutex);  // Thread safety
    ATTR_SET("statistics:level", int, m_statslevel);
    ATTR_SET("debug", int, m_debug);
    ATTR_SET("lazylayers", int, m_lazylayers);
    ATTR_SET("lazyglobals", int, m_lazyglobals);
    ATTR_SET("lazyunconnected", int, m_lazyunconnected);
    ATTR_SET("lazyerror", int, m_lazyerror);
    ATTR_SET("lazytrace", int, m_lazy_trace);
    ATTR_SET("lazy_userdata", int, m_lazy_userdata);
    ATTR_SET("userdata_isconnected", int, m_userdata_isconnected);
    ATTR_SET("clearmemory", int, m_clearmemory);
    ATTR_SET("debug_nan", int, m_debugnan);
    ATTR_SET("debugnan", int, m_debugnan);  // back-compatible alias
    ATTR_SET("debug_uninit", int, m_debug_uninit);
    ATTR_SET("lockgeom", int, m_lockgeom_default);
    ATTR_SET("profile", int, m_profile);
    ATTR_SET("optimize", int, m_optimize);
    ATTR_SET("opt_simplify_param", int, m_opt_simplify_param);
    ATTR_SET("opt_constant_fold", int, m_opt_constant_fold);
    ATTR_SET("opt_stale_assign", int, m_opt_stale_assign);
    ATTR_SET("opt_elide_useless_ops", int, m_opt_elide_useless_ops);
    ATTR_SET("opt_elide_unconnected_outputs", int,
             m_opt_elide_unconnected_outputs);
    ATTR_SET("opt_peephole", int, m_opt_peephole);
    ATTR_SET("opt_coalesce_temps", int, m_opt_coalesce_temps);
    ATTR_SET("opt_assign", int, m_opt_assign);
    ATTR_SET("opt_mix", int, m_opt_mix);
    ATTR_SET("opt_merge_instances", int, m_opt_merge_instances);
    ATTR_SET("opt_merge_instances_with_userdata", int,
             m_opt_merge_instances_with_userdata);
    ATTR_SET("opt_fold_getattribute", int, m_opt_fold_getattribute);
    ATTR_SET("opt_middleman", int, m_opt_middleman);
    ATTR_SET("opt_texture_handle", int, m_opt_texture_handle);
    ATTR_SET("opt_seed_bblock_aliases", int, m_opt_seed_bblock_aliases);
    ATTR_SET("opt_useparam", int, m_opt_useparam);
    ATTR_SET("opt_groupdata", int, m_opt_groupdata);
    ATTR_SET("opt_batched_analysis", int, m_opt_batched_analysis);
    ATTR_SET("llvm_jit_fma", int, m_llvm_jit_fma);
    ATTR_SET("llvm_jit_aggressive", int, m_llvm_jit_aggressive);
    ATTR_SET_STRING("llvm_jit_target", m_llvm_jit_target);
    ATTR_SET("vector_width", int, m_vector_width);
    ATTR_SET("opt_passes", int, m_opt_passes);
    ATTR_SET("optimize_nondebug", int, m_optimize_nondebug);
    ATTR_SET("llvm_optimize", int, m_llvm_optimize);
    ATTR_SET("llvm_debug", int, m_llvm_debug);
    ATTR_SET("llvm_debug_layers", int, m_llvm_debug_layers);
    ATTR_SET("llvm_debug_ops", int, m_llvm_debug_ops);
    ATTR_SET("llvm_target_host", int, m_llvm_target_host);
    ATTR_SET("llvm_debugging_symbols", int, m_llvm_debugging_symbols);
    ATTR_SET("llvm_profiling_events", int, m_llvm_profiling_events);
    ATTR_SET("llvm_output_bitcode", int, m_llvm_output_bitcode);
    ATTR_SET("llvm_dumpasm", int, m_llvm_dumpasm);
    ATTR_SET("dump_forced_llvm_bool_symbols", int,
             m_dump_forced_llvm_bool_symbols);
    ATTR_SET("dump_uniform_symbols", int, m_dump_uniform_symbols);
    ATTR_SET("dump_varying_symbols", int, m_dump_varying_symbols);
    ATTR_SET_STRING("llvm_prune_ir_strategy", m_llvm_prune_ir_strategy);
    ATTR_SET("strict_messages", int, m_strict_messages);
    ATTR_SET("range_checking", int, m_range_checking);
    ATTR_SET("unknown_coordsys_error", int,
             m_shading_state_uniform.m_unknown_coordsys_error);
    ATTR_SET("connection_error", int, m_connection_error);
    ATTR_SET("greedyjit", int, m_greedyjit);
    ATTR_SET("relaxed_param_typecheck", int, m_relaxed_param_typecheck);
    ATTR_SET("countlayerexecs", int, m_countlayerexecs);
    ATTR_SET("max_warnings_per_thread", int,
             m_shading_state_uniform.m_max_warnings_per_thread);
    ATTR_SET("max_local_mem_KB", int, m_max_local_mem_KB);
    ATTR_SET("compile_report", int, m_compile_report);
    ATTR_SET("max_optix_groupdata_alloc", int, m_max_optix_groupdata_alloc);
    ATTR_SET("buffer_printf", int, m_buffer_printf);
    ATTR_SET("no_noise", int, m_no_noise);
    ATTR_SET("no_pointcloud", int, m_no_pointcloud);
    ATTR_SET("force_derivs", int, m_force_derivs);
    ATTR_SET("allow_shader_replacement", int, m_allow_shader_replacement);
    ATTR_SET("exec_repeat", int, m_exec_repeat);
    ATTR_SET("opt_warnings", int, m_opt_warnings);
    ATTR_SET("gpu_opt_error", int, m_gpu_opt_error);
    ATTR_SET("optix_no_inline", int, m_optix_no_inline);
    ATTR_SET("optix_no_inline_layer_funcs", int, m_optix_no_inline_layer_funcs);
    ATTR_SET("optix_merge_layer_funcs", int, m_optix_merge_layer_funcs);
    ATTR_SET("optix_no_inline_rend_lib", int, m_optix_no_inline_rend_lib);
    ATTR_SET("optix_no_inline_thresh", int, m_optix_no_inline_thresh);
    ATTR_SET("optix_force_inline_thresh", int, m_optix_force_inline_thresh);
    ATTR_SET_STRINGHASH("commonspace",
                        m_shading_state_uniform.m_commonspace_synonym);
    ATTR_SET_STRING("debug_groupname", m_debug_groupname);
    ATTR_SET_STRING("debug_layername", m_debug_layername);
    ATTR_SET_STRING("opt_layername", m_opt_layername);
    ATTR_SET_STRING("only_groupname", m_only_groupname);
    ATTR_SET_STRING("archive_groupname", m_archive_groupname);
    ATTR_SET_STRING("archive_filename", m_archive_filename);

    // cases for special handling
    if (name == "searchpath:shader" && type == TypeDesc::STRING) {
        m_searchpath = std::string(*(const char**)val);
        OIIO::Filesystem::searchpath_split(m_searchpath, m_searchpath_dirs);
        return true;
    }
    if (name == "searchpath:library" && type == TypeDesc::STRING) {
        m_library_searchpath = std::string(*(const char**)val);
        OIIO::Filesystem::searchpath_split(m_library_searchpath,
                                           m_library_searchpath_dirs);
        return true;
    }
    if (name == "colorspace" && type == TypeDesc::STRING) {
        ustring c = ustring(*(const char**)val);
        if (colorsystem().set_colorspace(ustringhash_from(c)))
            m_colorspace = c;
        else
            errorfmt("Unknown color space \"{}\"", c);
        return true;
    }
    if (name == "raytypes" && type.basetype == TypeDesc::STRING) {
        OSL_ASSERT(type.numelements() <= 32
                   && "ShaderGlobals.raytype is an int, max of 32 raytypes");
        m_raytypes.clear();
        for (size_t i = 0; i < type.numelements(); ++i)
            m_raytypes.emplace_back(((const char**)val)[i]);
        return true;
    }
    if (name == "renderer_outputs" && type.basetype == TypeDesc::STRING) {
        m_renderer_outputs.clear();
        for (size_t i = 0; i < type.numelements(); ++i)
            m_renderer_outputs.emplace_back(((const char**)val)[i]);
        return true;
    }
    if (name == "lib_bitcode" && type.basetype == TypeDesc::UINT8) {
        if (type.arraylen < 0) {
            errorfmt("Invalid bitcode size: {}", type.arraylen);
            return false;
        }
        m_lib_bitcode.clear();
        if (type.arraylen) {
            const char* bytes = static_cast<const char*>(val);
            std::copy(bytes, bytes + type.arraylen,
                      back_inserter(m_lib_bitcode));
        }
        return true;
    }
    if (name == "rs_bitcode" && type.basetype == TypeDesc::UINT8) {
        if (type.arraylen < 0) {
            errorfmt("Invalid bitcode size: {}", type.arraylen);
            return false;
        }
        m_rs_bitcode.clear();
        if (type.arraylen) {
            const char* bytes = static_cast<const char*>(val);
            std::copy(bytes, bytes + type.arraylen,
                      back_inserter(m_rs_bitcode));
        }
        return true;
    }

    if (name == "error_repeats") {
        // Special case: setting error_repeats also clears the "previously
        // seen" error and warning lists.
        m_errseen.clear();
        m_warnseen.clear();
        ATTR_SET("error_repeats", int, m_error_repeats);
    }

    return false;
#undef ATTR_SET
#undef ATTR_SET_STRING
}



bool
ShadingSystemImpl::getattribute(string_view name, TypeDesc type, void* val)
{
#define ATTR_DECODE(_name, _ctype, _src)                               \
    if (name == _name && type == OIIO::BaseTypeFromC<_ctype>::value) { \
        *(_ctype*)(val) = (_ctype)(_src);                              \
        return true;                                                   \
    }
#define ATTR_DECODE_STRING(_name, _src)              \
    if (name == _name && type == TypeDesc::STRING) { \
        *(const char**)(val) = _src.c_str();         \
        return true;                                 \
    }

#define ATTR_DECODE_STRINGHASH(_name, _src)                \
    if (name == _name && type == TypeDesc::STRING) {       \
        *(const char**)(val) = ustring_from(_src).c_str(); \
        return true;                                       \
    }

    lock_guard guard(m_mutex);  // Thread safety

    ATTR_DECODE_STRING("searchpath:shader", m_searchpath);
    ATTR_DECODE_STRING("searchpath:library", m_library_searchpath);
    ATTR_DECODE("statistics:level", int, m_statslevel);
    ATTR_DECODE("lazylayers", int, m_lazylayers);
    ATTR_DECODE("lazyglobals", int, m_lazyglobals);
    ATTR_DECODE("lazyunconnected", int, m_lazyunconnected);
    ATTR_DECODE("lazytrace", int, m_lazy_trace);
    ATTR_DECODE("lazy_userdata", int, m_lazy_userdata);
    ATTR_DECODE("userdata_isconnected", int, m_userdata_isconnected);
    ATTR_DECODE("clearmemory", int, m_clearmemory);
    ATTR_DECODE("debug_nan", int, m_debugnan);
    ATTR_DECODE("debugnan", int, m_debugnan);  // back-compatible alias
    ATTR_DECODE("debug_uninit", int, m_debug_uninit);
    ATTR_DECODE("lockgeom", int, m_lockgeom_default);
    ATTR_DECODE("profile", int, m_profile);
    ATTR_DECODE("optimize", int, m_optimize);
    ATTR_DECODE("opt_simplify_param", int, m_opt_simplify_param);
    ATTR_DECODE("opt_constant_fold", int, m_opt_constant_fold);
    ATTR_DECODE("opt_stale_assign", int, m_opt_stale_assign);
    ATTR_DECODE("opt_elide_useless_ops", int, m_opt_elide_useless_ops);
    ATTR_DECODE("opt_elide_unconnected_outputs", int,
                m_opt_elide_unconnected_outputs);
    ATTR_DECODE("opt_peephole", int, m_opt_peephole);
    ATTR_DECODE("opt_coalesce_temps", int, m_opt_coalesce_temps);
    ATTR_DECODE("opt_assign", int, m_opt_assign);
    ATTR_DECODE("opt_mix", int, m_opt_mix);
    ATTR_DECODE("opt_merge_instances", int, m_opt_merge_instances);
    ATTR_DECODE("opt_merge_instances_with_userdata", int,
                m_opt_merge_instances_with_userdata);
    ATTR_DECODE("opt_fold_getattribute", int, m_opt_fold_getattribute);
    ATTR_DECODE("opt_middleman", int, m_opt_middleman);
    ATTR_DECODE("opt_texture_handle", int, m_opt_texture_handle);
    ATTR_DECODE("opt_seed_bblock_aliases", int, m_opt_seed_bblock_aliases);
    ATTR_DECODE("opt_useparam", int, m_opt_useparam);
    ATTR_DECODE("opt_groupdata", int, m_opt_groupdata);
    ATTR_DECODE("opt_batched_analysis", int, m_opt_batched_analysis);
    ATTR_DECODE("llvm_jit_fma", int, m_llvm_jit_fma);
    ATTR_DECODE("llvm_jit_aggressive", int, m_llvm_jit_aggressive);
    ATTR_DECODE_STRING("llvm_jit_target", m_llvm_jit_target);
    ATTR_DECODE("vector_width", int, m_vector_width);
    ATTR_DECODE("opt_passes", int, m_opt_passes);
    ATTR_DECODE("optimize_nondebug", int, m_optimize_nondebug);
    ATTR_DECODE("llvm_optimize", int, m_llvm_optimize);
    ATTR_DECODE("debug", int, m_debug);
    ATTR_DECODE("llvm_debug", int, m_llvm_debug);
    ATTR_DECODE("llvm_debug_layers", int, m_llvm_debug_layers);
    ATTR_DECODE("llvm_debug_ops", int, m_llvm_debug_ops);
    ATTR_DECODE("llvm_target_host", int, m_llvm_target_host);
    ATTR_DECODE("llvm_debugging_symbols", int, m_llvm_debugging_symbols);
    ATTR_DECODE("llvm_profiling_events", int, m_llvm_profiling_events);
    ATTR_DECODE("llvm_output_bitcode", int, m_llvm_output_bitcode);
    ATTR_DECODE("llvm_dumpasm", int, m_llvm_dumpasm);
    ATTR_DECODE("dump_forced_llvm_bool_symbols", int,
                m_dump_forced_llvm_bool_symbols);
    ATTR_DECODE("dump_uniform_symbols", int, m_dump_uniform_symbols);
    ATTR_DECODE("dump_varying_symbols", int, m_dump_varying_symbols);
    ATTR_DECODE("strict_messages", int, m_strict_messages);
    ATTR_DECODE("error_repeats", int, m_error_repeats);
    ATTR_DECODE("range_checking", int, m_range_checking);
    ATTR_DECODE("unknown_coordsys_error", int,
                m_shading_state_uniform.m_unknown_coordsys_error);
    ATTR_DECODE("connection_error", int, m_connection_error);
    ATTR_DECODE("greedyjit", int, m_greedyjit);
    ATTR_DECODE("countlayerexecs", int, m_countlayerexecs);
    ATTR_DECODE("relaxed_param_typecheck", int, m_relaxed_param_typecheck);
    ATTR_DECODE("max_warnings_per_thread", int,
                m_shading_state_uniform.m_max_warnings_per_thread);
    ATTR_DECODE_STRINGHASH("commonspace",
                           m_shading_state_uniform.m_commonspace_synonym);
    ATTR_DECODE_STRING("colorspace", m_colorspace);
    ATTR_DECODE_STRING("debug_groupname", m_debug_groupname);
    ATTR_DECODE_STRING("debug_layername", m_debug_layername);
    ATTR_DECODE_STRING("opt_layername", m_opt_layername);
    ATTR_DECODE_STRING("only_groupname", m_only_groupname);
    ATTR_DECODE_STRING("archive_groupname", m_archive_groupname);
    ATTR_DECODE_STRING("archive_filename", m_archive_filename);
    ATTR_DECODE("max_local_mem_KB", int, m_max_local_mem_KB);
    ATTR_DECODE("compile_report", int, m_compile_report);
    ATTR_DECODE("max_optix_groupdata_alloc", int, m_max_optix_groupdata_alloc);
    ATTR_DECODE("buffer_printf", int, m_buffer_printf);
    ATTR_DECODE("no_noise", int, m_no_noise);
    ATTR_DECODE("no_pointcloud", int, m_no_pointcloud);
    ATTR_DECODE("force_derivs", int, m_force_derivs);
    ATTR_DECODE("allow_shader_replacement", int, m_allow_shader_replacement);
    ATTR_DECODE("exec_repeat", int, m_exec_repeat);
    ATTR_DECODE("opt_warnings", int, m_opt_warnings);
    ATTR_DECODE("gpu_opt_error", int, m_gpu_opt_error);
    ATTR_DECODE("optix_no_inline", int, m_optix_no_inline);
    ATTR_DECODE("optix_no_inline_layer_funcs", int,
                m_optix_no_inline_layer_funcs);
    ATTR_DECODE("optix_merge_layer_funcs", int, m_optix_merge_layer_funcs);
    ATTR_DECODE("optix_no_inline_rend_lib", int, m_optix_no_inline_rend_lib);
    ATTR_DECODE("optix_no_inline_thresh", int, m_optix_no_inline_thresh);
    ATTR_DECODE("optix_force_inline_thresh", int, m_optix_force_inline_thresh);

    ATTR_DECODE("stat:masters", int, m_stat_shaders_loaded);
    ATTR_DECODE("stat:groups", int, m_stat_groups);
    ATTR_DECODE("stat:instances_compiled", int, m_stat_instances_compiled);
    ATTR_DECODE("stat:groups_compiled", int, m_stat_groups_compiled);
    ATTR_DECODE("stat:empty_instances", int, m_stat_empty_instances);
    ATTR_DECODE("stat:merged_inst", int, m_stat_merged_inst);
    ATTR_DECODE("stat:merged_inst_opt", int, m_stat_merged_inst_opt);
    ATTR_DECODE("stat:empty_groups", int, m_stat_empty_groups);
    ATTR_DECODE("stat:instances", int, m_stat_groupinstances);
    ATTR_DECODE("stat:regexes", int, m_stat_regexes);
    ATTR_DECODE("stat:preopt_syms", int, m_stat_preopt_syms);
    ATTR_DECODE("stat:postopt_syms", int, m_stat_postopt_syms);
    ATTR_DECODE("stat:syms_with_derivs", int, m_stat_syms_with_derivs);
    ATTR_DECODE("stat:preopt_ops", int, m_stat_preopt_ops);
    ATTR_DECODE("stat:postopt_ops", int, m_stat_postopt_ops);
    ATTR_DECODE("stat:middlemen_eliminated", int, m_stat_middlemen_eliminated);
    ATTR_DECODE("stat:const_connections", int, m_stat_const_connections);
    ATTR_DECODE("stat:global_connections", int, m_stat_global_connections);
    ATTR_DECODE("stat:tex_calls_codegened", int, m_stat_tex_calls_codegened);
    ATTR_DECODE("stat:tex_calls_as_handles", int, m_stat_tex_calls_as_handles);
    ATTR_DECODE("stat:useparam_ops", int, m_stat_useparam_ops);
    ATTR_DECODE("stat:call_layers_inserted", int, m_stat_call_layers_inserted);
    ATTR_DECODE("stat:master_load_time", float, m_stat_master_load_time);
    ATTR_DECODE("stat:optimization_time", float, m_stat_optimization_time);
    ATTR_DECODE("stat:opt_locking_time", float, m_stat_opt_locking_time);
    ATTR_DECODE("stat:specialization_time", float, m_stat_specialization_time);
    ATTR_DECODE("stat:total_llvm_time", float, m_stat_total_llvm_time);
    ATTR_DECODE("stat:llvm_setup_time", float, m_stat_llvm_setup_time);
    ATTR_DECODE("stat:llvm_irgen_time", float, m_stat_llvm_irgen_time);
    ATTR_DECODE("stat:llvm_opt_time", float, m_stat_llvm_opt_time);
    ATTR_DECODE("stat:llvm_jit_time", float, m_stat_llvm_jit_time);
    ATTR_DECODE("stat:inst_merge_time", float, m_stat_inst_merge_time);
    ATTR_DECODE("stat:getattribute_calls", long long,
                m_stat_getattribute_calls);
    ATTR_DECODE("stat:get_userdata_calls", long long,
                m_stat_get_userdata_calls);
    ATTR_DECODE("stat:noise_calls", long long, m_stat_noise_calls);
    ATTR_DECODE("stat:pointcloud_searches", long long,
                m_stat_pointcloud_searches);
    ATTR_DECODE("stat:pointcloud_gets", long long, m_stat_pointcloud_gets);
    ATTR_DECODE("stat:pointcloud_writes", long long, m_stat_pointcloud_writes);
    ATTR_DECODE("stat:pointcloud_searches_total_results", long long,
                m_stat_pointcloud_searches_total_results);
    ATTR_DECODE("stat:pointcloud_max_results", int,
                m_stat_pointcloud_max_results);
    ATTR_DECODE("stat:pointcloud_failures", int, m_stat_pointcloud_failures);
    ATTR_DECODE("stat:reparam_calls_total", long long,
                m_stat_reparam_calls_total);
    ATTR_DECODE("stat:reparam_bytes_total", long long,
                m_stat_reparam_bytes_total);
    ATTR_DECODE("stat:reparam_calls_changed", long long,
                m_stat_reparam_calls_changed);
    ATTR_DECODE("stat:reparam_bytes_changed", long long,
                m_stat_reparam_bytes_changed);
    ATTR_DECODE("stat:memory_current", long long, m_stat_memory.current());
    ATTR_DECODE("stat:memory_peak", long long, m_stat_memory.peak());
    ATTR_DECODE("stat:mem_master_current", long long,
                m_stat_mem_master.current());
    ATTR_DECODE("stat:mem_master_peak", long long, m_stat_mem_master.peak());
    ATTR_DECODE("stat:mem_master_ops_current", long long,
                m_stat_mem_master_ops.current());
    ATTR_DECODE("stat:mem_master_ops_peak", long long,
                m_stat_mem_master_ops.peak());
    ATTR_DECODE("stat:mem_master_args_current", long long,
                m_stat_mem_master_args.current());
    ATTR_DECODE("stat:mem_master_args_peak", long long,
                m_stat_mem_master_args.peak());
    ATTR_DECODE("stat:mem_master_syms_current", long long,
                m_stat_mem_master_syms.current());
    ATTR_DECODE("stat:mem_master_syms_peak", long long,
                m_stat_mem_master_syms.peak());
    ATTR_DECODE("stat:mem_master_defaults_current", long long,
                m_stat_mem_master_defaults.current());
    ATTR_DECODE("stat:mem_master_defaults_peak", long long,
                m_stat_mem_master_defaults.peak());
    ATTR_DECODE("stat:mem_master_consts_current", long long,
                m_stat_mem_master_consts.current());
    ATTR_DECODE("stat:mem_master_consts_peak", long long,
                m_stat_mem_master_consts.peak());
    ATTR_DECODE("stat:mem_inst_current", long long, m_stat_mem_inst.current());
    ATTR_DECODE("stat:mem_inst_peak", long long, m_stat_mem_inst.peak());
    ATTR_DECODE("stat:mem_inst_syms_current", long long,
                m_stat_mem_inst_syms.current());
    ATTR_DECODE("stat:mem_inst_syms_peak", long long,
                m_stat_mem_inst_syms.peak());
    ATTR_DECODE("stat:mem_inst_paramvals_current", long long,
                m_stat_mem_inst_paramvals.current());
    ATTR_DECODE("stat:mem_inst_paramvals_peak", long long,
                m_stat_mem_inst_paramvals.peak());
    ATTR_DECODE("stat:mem_inst_connections_current", long long,
                m_stat_mem_inst_connections.current());
    ATTR_DECODE("stat:mem_inst_connections_peak", long long,
                m_stat_mem_inst_connections.peak());

    if (name == "colorsystem" && type.basetype == TypeDesc::PTR) {
        *(void**)val = &colorsystem();
        return true;
    }
    if (name == "colorsystem:sizes" && type.basetype == TypeDesc::LONGLONG) {
        if (type.arraylen != 2) {
            error(
                "Must request two colorsystem:sizes, [sizeof(pvt::ColorSystem), num-strings]");
            return false;
        }
        long long* lptr = (long long*)val;
        lptr[0]         = sizeof(pvt::ColorSystem);
        lptr[1]         = 1;  // 1 string (pvt::ColorSystem::m_colorspace)

        // Make sure everything adds up!
        OSL_ASSERT(
            (((char*)&colorsystem() + lptr[0]) - sizeof(ustring) * lptr[1])
            == (char*)&colorsystem().colorspace());
        return true;
    }
    if (name == "osl:simd" && type == TypeDesc::STRING) {
        *(const char**)val = ustring(osl_simd_caps()).c_str();
        return true;
    }
    if (name == "hw:simd" && type == TypeDesc::STRING) {
        return OIIO::getattribute("hw:string", type, val);
    }
    if (name == "osl:cuda_version" && type == TypeDesc::STRING) {
        *(const char**)val = OSL_CUDA_VERSION;
        return true;
    }
    if (name == "osl:optix_version" && type == TypeDesc::STRING) {
        *(const char**)val = OSL_OPTIX_VERSION;
        return true;
    }
    if (name == "osl:dependencies" && type == TypeDesc::STRING) {
        std::string deps = fmtformat("LLVM-{},OpenImageIO-{},Imath-{}",
                                     OSL_LLVM_FULL_VERSION, OIIO_VERSION_STRING,
                                     IMATH_VERSION_STRING);
        if (!strcmp(OSL_CUDA_VERSION, ""))
            deps += ",Cuda-NONE";
        else
            deps += ",Cuda-" OSL_CUDA_VERSION;
        if (!strcmp(OSL_OPTIX_VERSION, ""))
            deps += ",OptiX-NONE";
        else
            deps += ",OptiX-" OSL_OPTIX_VERSION;
        *(const char**)val = ustring(deps).c_str();
        return true;
    }
#ifdef OSL_LLVM_CUDA_BITCODE
    if (name == "shadeops_cuda_ptx" && type.basetype == TypeDesc::PTR) {
        *(const char**)val = reinterpret_cast<const char*>(
            shadeops_cuda_ptx_compiled_ops_block);
        return true;
    }
    if (name == "shadeops_cuda_ptx_size" && type.basetype == TypeDesc::INT) {
        *(int*)val = shadeops_cuda_ptx_compiled_ops_size;
        return true;
    }
#endif

    return false;
#undef ATTR_DECODE
#undef ATTR_DECODE_STRING
}



bool
ShadingSystemImpl::attribute(ShaderGroup* group, string_view name,
                             TypeDesc type, const void* val)
{
    // No current group attributes to set
    if (!group)
        return attribute(name, type, val);
    lock_guard lock(group->m_mutex);
    if (name == "renderer_outputs" && type.basetype == TypeDesc::STRING) {
        group->m_renderer_outputs.clear();
        for (size_t i = 0; i < type.numelements(); ++i)
            group->m_renderer_outputs.emplace_back(((const char**)val)[i]);
        return true;
    }
    if (name == "entry_layers" && type.basetype == TypeDesc::STRING) {
        group->clear_entry_layers();
        for (int i = 0; i < (int)type.numelements(); ++i)
            group->mark_entry_layer(ustring(((const char**)val)[i]));
        return true;
    }
    if (name == "exec_repeat" && type == TypeInt) {
        group->m_exec_repeat = *(const int*)val;
        return true;
    }
    if (name == "groupname" && type == TypeString) {
        group->name(ustring(((const char**)val)[0]));
        return true;
    }
    return false;
}



bool
ShadingSystemImpl::getattribute(ShaderGroup* group, string_view name,
                                TypeDesc type, void* val)
{
    if (!group)
        return false;

    if (name == "groupname" && type == TypeString) {
        *(ustring*)val = group->name();
        return true;
    }
    if (name == "num_layers" && type == TypeInt) {
        *(int*)val = group->nlayers();
        return true;
    }
    if (name == "layer_names" && type.basetype == TypeDesc::STRING) {
        size_t n = std::min(type.numelements(), (size_t)group->nlayers());
        for (size_t i = 0; i < n; ++i)
            ((ustring*)val)[i] = (*group)[i]->layername();
        return true;
    }
    if (name == "num_renderer_outputs" && type.basetype == TypeDesc::INT) {
        *(int*)val = (int)group->m_renderer_outputs.size();
        return true;
    }
    if (name == "renderer_outputs" && type.basetype == TypeDesc::STRING) {
        size_t n = std::min(type.numelements(),
                            group->m_renderer_outputs.size());
        for (size_t i = 0; i < n; ++i)
            ((ustring*)val)[i] = group->m_renderer_outputs[i];
        for (size_t i = n; i < type.numelements(); ++i)
            ((ustring*)val)[i] = ustring();
        return true;
    }
    if (name == "raytype_queries" && type.basetype == TypeDesc::INT) {
        *(int*)val = group->raytype_queries();
        return true;
    }
    if (name == "num_entry_layers" && type.basetype == TypeDesc::INT) {
        int n = 0;
        for (int i = 0; i < group->nlayers(); ++i)
            n += group->layer(i)->entry_layer();
        *(int*)val = n;
        return true;
    }
    if (name == "entry_layers" && type.basetype == TypeDesc::STRING) {
        size_t n = 0;
        for (size_t i = 0;
             i < (size_t)group->nlayers() && i < type.numelements(); ++i)
            if (group->layer(i)->entry_layer())
                ((ustring*)val)[n++] = (*group)[i]->layername();
        for (size_t i = n; i < type.numelements(); ++i)
            ((ustring*)val)[i] = ustring();
        return true;
    }
    if (name == "group_init_name" && type.basetype == TypeDesc::STRING) {
        *(ustring*)val = init_function_name(*this, *group, true);
        return true;
    }
    if (name == "group_entry_name" && type.basetype == TypeDesc::STRING) {
        int nlayers          = group->nlayers();
        ShaderInstance* inst = (*group)[nlayers - 1];
        *(ustring*)val       = layer_function_name(*group, *inst, true);
        return true;
    }
    if (name == "group_fused_name" && type.basetype == TypeDesc::STRING) {
        *(ustring*)val = fused_function_name(*group);
        return true;
    }
    if (name == "layer_osofiles" && type.basetype == TypeDesc::STRING) {
        size_t n = std::min(type.numelements(), (size_t)group->nlayers());
        for (size_t i = 0; i < n; ++i)
            ((ustring*)val)[i] = (*group)[i]->master()->osofilename();
        return true;
    }
    if (name == "pickle" && type == TypeDesc::STRING) {
        *(ustring*)val = ustring(group->serialize());
        return true;
    }
    if (name == "exec_repeat" && type == TypeInt) {
        *(int*)val = group->m_exec_repeat;
        return true;
    }
    if (name == "ptx_compiled_version" && type.basetype == TypeDesc::PTR) {
        bool exists        = !group->m_llvm_ptx_compiled_version.empty();
        *(std::string*)val = exists ? group->m_llvm_ptx_compiled_version : "";
        return true;
    }
    if (name == "interactive_params" && type.basetype == TypeDesc::PTR) {
        *(void**)val = group->m_interactive_arena.get();
        return true;
    }
    if (name == "device_interactive_params" && type.basetype == TypeDesc::PTR) {
        *(void**)val = group->m_device_interactive_arena.d_get();
        return true;
    }

    // All the remaining attributes require the group to already be
    // optimized.
    if (!group->optimized()) {
        auto threadinfo = create_thread_info();
        auto ctx        = get_context(threadinfo);
        optimize_group(*group, ctx, false /*jit*/);
        release_context(ctx);
        destroy_thread_info(threadinfo);
    }

    if (name == "num_textures_needed" && type == TypeInt) {
        *(int*)val = (int)group->m_textures_needed.size();
        return true;
    }
    if (name == "textures_needed" && type.basetype == TypeDesc::PTR) {
        size_t n        = group->m_textures_needed.size();
        *(ustring**)val = n ? &group->m_textures_needed[0] : NULL;
        return true;
    }
    if (name == "unknown_textures_needed" && type == TypeInt) {
        *(int*)val = (int)group->m_unknown_textures_needed;
        return true;
    }

    if (name == "num_closures_needed" && type == TypeInt) {
        *(int*)val = (int)group->m_closures_needed.size();
        return true;
    }
    if (name == "closures_needed" && type.basetype == TypeDesc::PTR) {
        size_t n        = group->m_closures_needed.size();
        *(ustring**)val = n ? &group->m_closures_needed[0] : NULL;
        return true;
    }
    if (name == "unknown_closures_needed" && type == TypeInt) {
        *(int*)val = (int)group->m_unknown_closures_needed;
        return true;
    }

    if (name == "num_globals_needed" && type == TypeInt) {
        *(int*)val = (int)group->m_globals_needed.size();
        return true;
    }
    if (name == "globals_needed" && type.basetype == TypeDesc::PTR) {
        size_t n        = group->m_globals_needed.size();
        *(ustring**)val = n ? &group->m_globals_needed[0] : NULL;
        return true;
    }
    if (name == "globals_read" && type.basetype == TypeDesc::INT) {
        *(int*)val = group->m_globals_read;
        return true;
    }
    if (name == "globals_write" && type.basetype == TypeDesc::INT) {
        *(int*)val = group->m_globals_write;
        return true;
    }

    if (name == "num_userdata" && type == TypeInt) {
        *(int*)val = (int)group->m_userdata_names.size();
        return true;
    }
    if (name == "userdata_names" && type.basetype == TypeDesc::PTR) {
        size_t n        = group->m_userdata_names.size();
        *(ustring**)val = n ? &group->m_userdata_names[0] : NULL;
        return true;
    }
    if (name == "userdata_types" && type.basetype == TypeDesc::PTR) {
        size_t n         = group->m_userdata_types.size();
        *(TypeDesc**)val = n ? &group->m_userdata_types[0] : NULL;
        return true;
    }
    if (name == "userdata_offsets" && type.basetype == TypeDesc::PTR) {
        size_t n    = group->m_userdata_offsets.size();
        *(int**)val = n ? &group->m_userdata_offsets[0] : NULL;
        return true;
    }
    if (name == "userdata_derivs" && type.basetype == TypeDesc::PTR) {
        size_t n     = group->m_userdata_derivs.size();
        *(char**)val = n ? &group->m_userdata_derivs[0] : NULL;
        return true;
    }
    if (name == "num_attributes_needed" && type == TypeInt) {
        *(int*)val = (int)group->m_attributes_needed.size();
        return true;
    }
    if (name == "attributes_needed" && type.basetype == TypeDesc::PTR) {
        size_t n        = group->m_attributes_needed.size();
        *(ustring**)val = n ? &group->m_attributes_needed[0] : NULL;
        return true;
    }
    if (name == "attribute_scopes" && type.basetype == TypeDesc::PTR) {
        size_t n        = group->m_attribute_scopes.size();
        *(ustring**)val = n ? &group->m_attribute_scopes[0] : NULL;
        return true;
    }
    if (name == "attribute_types" && type.basetype == TypeDesc::PTR) {
        size_t n         = group->m_attribute_types.size();
        *(TypeDesc**)val = n ? &group->m_attribute_types[0] : NULL;
        return true;
    }
    if (name == "attribute_derivs" && type.basetype == TypeDesc::PTR) {
        size_t n     = group->m_attribute_derivs.size();
        *(char**)val = n ? &group->m_attribute_derivs[0] : NULL;
        return true;
    }
    if (name == "unknown_attributes_needed" && type == TypeInt) {
        *(int*)val = (int)group->m_unknown_attributes_needed;
        return true;
    }
    if (name == "group_id" && type == TypeInt) {
        *(int*)val = (int)group->id();
        return true;
    }

    // Additional attributes useful to OptiX-based renderers
    if (name == "userdata_layers" && type.basetype == TypeDesc::PTR) {
        size_t n    = group->m_userdata_layers.size();
        *(int**)val = n ? &group->m_userdata_layers[0] : NULL;
        return true;
    }
    if (name == "userdata_init_vals" && type.basetype == TypeDesc::PTR) {
        size_t n     = group->m_userdata_init_vals.size();
        *(void**)val = n ? &group->m_userdata_init_vals[0] : NULL;
        return true;
    }
    if (name == "llvm_groupdata_size" && type == TypeInt) {
        *(int*)val = (int)group->llvm_groupdata_size();
        return true;
    }

    return false;
}



void
ShadingSystemImpl::error(const std::string& msg) const
{
    lock_guard guard(m_errmutex);
    int n = 0;
    for (auto&& s : m_errseen) {
        if (s == msg && !m_error_repeats)
            return;
        ++n;
    }
    if (n >= m_errseenmax)
        m_errseen.pop_front();
    m_errseen.push_back(msg);
    m_err->error(msg);
}



void
ShadingSystemImpl::warning(const std::string& msg) const
{
    lock_guard guard(m_errmutex);
    int n = 0;
    for (auto&& s : m_warnseen) {
        if (s == msg && !m_error_repeats)
            return;
        ++n;
    }
    if (n >= m_errseenmax)
        m_warnseen.pop_front();
    m_warnseen.push_back(msg);
    m_err->warning(msg);
}



void
ShadingSystemImpl::info(const std::string& msg) const
{
    lock_guard guard(m_errmutex);
    m_err->info(msg);
}



void
ShadingSystemImpl::message(const std::string& msg) const
{
    lock_guard guard(m_errmutex);
    m_err->message(msg);
}



namespace {
typedef std::pair<ustring, long long> GroupTimeVal;
struct group_time_compare {  // So looking forward to C++11 lambdas!
    bool operator()(const GroupTimeVal& a, const GroupTimeVal& b)
    {
        return a.second > b.second;
    }
};
}  // namespace



std::string
ShadingSystemImpl::getstats(int level) const
{
    int columns = OIIO::Sysutil::terminal_columns() - 2;

    if (level <= 0)
        return "";
    std::ostringstream out;
    out.imbue(std::locale::classic());  // force C locale
    out << "Open Shading Language " << OSL_LIBRARY_VERSION_STRING << "\n";
    ustring build_deps;
    const_cast<ShadingSystemImpl*>(this)->getattribute("osl:dependencies",
                                                       TypeDesc::STRING,
                                                       &build_deps);
    out << "  Build deps: "
        << Strutil::wordwrap(Strutil::join(Strutil::splitsv(build_deps, ","),
                                           ", "),
                             columns, 14)
        << "\n";

    std::string opt;
#define BOOLOPT(name) opt += fmtformat(#name "={} ", m_##name)
#define INTOPT(name)  opt += fmtformat(#name "={} ", m_##name)
#define STROPT(name)     \
    if (m_##name.size()) \
    opt += fmtformat(#name "=\"{}\" ", m_##name)
    INTOPT(optimize);
    INTOPT(llvm_optimize);
    INTOPT(debug);
    INTOPT(profile);
    INTOPT(llvm_debug);
    BOOLOPT(llvm_debug_layers);
    BOOLOPT(llvm_debug_ops);
    BOOLOPT(llvm_target_host);
    BOOLOPT(llvm_output_bitcode);
    BOOLOPT(llvm_dumpasm);
    BOOLOPT(llvm_prune_ir_strategy);
    BOOLOPT(lazylayers);
    BOOLOPT(lazyglobals);
    BOOLOPT(lazyunconnected);
    BOOLOPT(lazyerror);
    BOOLOPT(lazy_userdata);
    BOOLOPT(lazy_trace);
    BOOLOPT(userdata_isconnected);
    BOOLOPT(clearmemory);
    BOOLOPT(debugnan);
    BOOLOPT(debug_uninit);
    BOOLOPT(lockgeom_default);
    BOOLOPT(strict_messages);
    BOOLOPT(error_repeats);
    BOOLOPT(range_checking);
    BOOLOPT(greedyjit);
    BOOLOPT(countlayerexecs);
    BOOLOPT(opt_simplify_param);
    BOOLOPT(opt_constant_fold);
    BOOLOPT(opt_stale_assign);
    BOOLOPT(opt_elide_useless_ops);
    BOOLOPT(opt_elide_unconnected_outputs);
    BOOLOPT(opt_peephole);
    BOOLOPT(opt_coalesce_temps);
    BOOLOPT(opt_assign);
    BOOLOPT(opt_mix);
    INTOPT(opt_merge_instances);
    BOOLOPT(opt_merge_instances_with_userdata);
    BOOLOPT(opt_fold_getattribute);
    BOOLOPT(opt_middleman);
    BOOLOPT(opt_texture_handle);
    BOOLOPT(opt_seed_bblock_aliases);
    BOOLOPT(opt_batched_analysis);
    BOOLOPT(llvm_jit_fma);
    BOOLOPT(llvm_jit_aggressive);
    INTOPT(vector_width);
    STROPT(llvm_jit_target);
    INTOPT(opt_passes);
    INTOPT(no_noise);
    INTOPT(no_pointcloud);
    INTOPT(force_derivs);
    INTOPT(allow_shader_replacement);
    INTOPT(exec_repeat);
    INTOPT(opt_warnings);
    INTOPT(gpu_opt_error);
    BOOLOPT(optix_no_inline);
    BOOLOPT(optix_no_inline_layer_funcs);
    BOOLOPT(optix_merge_layer_funcs);
    BOOLOPT(optix_no_inline_rend_lib);
    INTOPT(optix_no_inline_thresh);
    INTOPT(optix_force_inline_thresh);
    STROPT(debug_groupname);
    STROPT(debug_layername);
    STROPT(archive_groupname);
    STROPT(archive_filename);
#undef BOOLOPT
#undef INTOPT
#undef STROPT

    // Print the HW info
    ustring buildsimd;
    if (!const_cast<ShadingSystemImpl*>(this)->getattribute("osl:simd",
                                                            TypeDesc::STRING,
                                                            &buildsimd))
        buildsimd = "no SIMD";
    out << "  Build HW support: "
        << Strutil::wordwrap(Strutil::join(Strutil::splitsv(buildsimd, ","),
                                           ", "),
                             columns, 20)
        << "\n";
    out << Strutil::wordwrap(
        fmtformat("  Runtime HW: {} cores, {:.1f}GB, {}",
                  OIIO::Sysutil::hardware_concurrency(),
                  OIIO::Sysutil::physical_memory() / float(1 << 30),
                  Strutil::replace(OIIO::get_string_attribute("hw:simd"), ",",
                                   ", ", true)),
        columns, 8)
        << "\n";
    // TODO: detect GPU info and print it here
    out << "\n";

    out << "ShadingSystem Options:\n";
    out << "    " << Strutil::wordwrap(opt, columns, 4) << "\n";

    out << "\nOSL ShadingSystem statistics (" << (void*)this << ")\n";
    if (m_stat_shaders_requested == 0 && m_stat_shaders_loaded == 0) {
        out << "  No shaders requested or loaded\n";
        return out.str();
    }

    out << "  Shaders:\n";
    out << "    Requested: " << m_stat_shaders_requested << "\n";
    out << "    Loaded:    " << m_stat_shaders_loaded << "\n";
    out << "    Masters:   " << m_stat_shaders_loaded << "\n";
    out << "    Instances: " << m_stat_instances << "\n";
    out << "  Time loading masters: "
        << Strutil::timeintervalformat(m_stat_master_load_time, 2) << "\n";
    out << "  Shading groups:   " << m_stat_groups << "\n";
    out << "    Total instances in all groups: " << m_stat_groupinstances
        << "\n";
    float iperg = (float)m_stat_groupinstances
                  / std::max((int)m_stat_groups, 1);
    out << "    Avg instances per group: " << fmtformat("{:.1f}", iperg)
        << "\n";
    out << "  Shading contexts: " << m_stat_contexts << "\n";
    if (m_countlayerexecs)
        out << "  Total layers executed: " << m_stat_layers_executed << "\n";

#if 0
    long long totalexec = m_layers_executed_uncond + m_layers_executed_lazy +
                          m_layers_executed_never;
    print(out, "  Total layers run: {:10}\n", totalexec);
    double inv_totalexec = 1.0 / std::max (totalexec, 1LL);  // prevent div by 0
    print(out, "    Unconditional:  {:10}  ({:.1f}%)\n",
          m_layers_executed_uncond,
          (100.0*m_layers_executed_uncond) * inv_totalexec);
    print(out, "    On demand:      {:10}  ({:.1f}%)\n",
          m_layers_executed_lazy,
          (100.0*m_layers_executed_lazy) * inv_totalexec);
    print(out, "    Skipped:        {:10}  ({:.1f}%)\n",
          m_layers_executed_never,
          (100.0*m_layers_executed_never) * inv_totalexec);

#endif

    out << "  Compiled " << m_stat_groups_compiled << " groups, "
        << m_stat_instances_compiled << " instances\n";
    out << "  Merged " << (m_stat_merged_inst + m_stat_merged_inst_opt)
        << " instances (" << m_stat_merged_inst << " initial, "
        << m_stat_merged_inst_opt << " after opt) in "
        << Strutil::timeintervalformat(m_stat_inst_merge_time, 2) << "\n";
    if (m_stat_instances_compiled > 0)
        out << "  After optimization, " << m_stat_empty_instances
            << " empty instances ("
            << (int)(100.0f * m_stat_empty_instances
                     / m_stat_instances_compiled)
            << "%)\n";
    if (m_stat_groups_compiled > 0)
        out << "  After optimization, " << m_stat_empty_groups
            << " empty groups ("
            << (int)(100.0f * m_stat_empty_groups / m_stat_groups_compiled)
            << "%)\n";
    if (m_stat_instances_compiled > 0 || m_stat_groups_compiled > 0) {
        print(out, "  Optimized {} ops to {} ({:.1f}%)\n",
              (int)m_stat_preopt_ops, (int)m_stat_postopt_ops,
              100.0
                  * (double(m_stat_postopt_ops)
                         / (std::max(1, (int)m_stat_preopt_ops))
                     - 1.0));
        print(out, "  Optimized {} symbols to {} ({:.1f}%)\n",
              (int)m_stat_preopt_syms, (int)m_stat_postopt_syms,
              100.0
                  * (double(m_stat_postopt_syms)
                         / (std::max(1, (int)m_stat_preopt_syms))
                     - 1.0));
        print(out, "  Optimized {} useparam ops into {} llvm run layer calls\n",
              (int)m_stat_useparam_ops, (int)m_stat_call_layers_inserted);
    }
    print(out, "  Constant connections eliminated: {}\n",
          (int)m_stat_const_connections);
    print(out, "  Global connections eliminated: {}\n",
          (int)m_stat_global_connections);
    print(out, "  Middlemen eliminated: {}\n",
          (int)m_stat_middlemen_eliminated);
    print(out, "  Derivatives needed on {} / {} symbols ({:.1f}%)\n",
          (int)m_stat_syms_with_derivs, (int)m_stat_postopt_syms,
          (100.0 * (int)m_stat_syms_with_derivs)
              / std::max((int)m_stat_postopt_syms, 1));
    out << "  Runtime optimization cost: "
        << Strutil::timeintervalformat(m_stat_optimization_time, 2) << "\n";
    out << "    locking:                   "
        << Strutil::timeintervalformat(m_stat_opt_locking_time, 2) << "\n";
    out << "    runtime specialization:    "
        << Strutil::timeintervalformat(m_stat_specialization_time, 2) << "\n";
    if (m_stat_total_llvm_time > 0.0) {
        out << "    LLVM setup:                "
            << Strutil::timeintervalformat(m_stat_llvm_setup_time, 2) << "\n";
        out << "    LLVM IR gen:               "
            << Strutil::timeintervalformat(m_stat_llvm_irgen_time, 2) << "\n";
        out << "    LLVM optimize:             "
            << Strutil::timeintervalformat(m_stat_llvm_opt_time, 2) << "\n";
        out << "    LLVM JIT:                  "
            << Strutil::timeintervalformat(m_stat_llvm_jit_time, 2) << "\n";
    }

    out << "  Texture calls compiled: " << (int)m_stat_tex_calls_codegened
        << " (" << (int)m_stat_tex_calls_as_handles << " used handles)\n";
    out << "  Regex's compiled: " << m_stat_regexes << "\n";
    out << "  Largest generated function local memory size: "
        << m_stat_max_llvm_local_mem / 1024 << " KB\n";
    if (m_stat_getattribute_calls) {
        out << "  getattribute calls: " << m_stat_getattribute_calls << " ("
            << Strutil::timeintervalformat(m_stat_getattribute_time, 2)
            << ")\n";
        out << "     (fail time "
            << Strutil::timeintervalformat(m_stat_getattribute_fail_time, 2)
            << ")\n";
    }
    out << "  Number of get_userdata calls: " << m_stat_get_userdata_calls
        << "\n";
    if (profile() > 1)
        out << "  Number of noise calls: " << m_stat_noise_calls << "\n";
    if (m_stat_pointcloud_searches || m_stat_pointcloud_writes) {
        out << "  Pointcloud operations:\n";
        out << "    pointcloud_search calls: " << m_stat_pointcloud_searches
            << "\n";
        out << "      max query results: " << m_stat_pointcloud_max_results
            << "\n";
        double avg = m_stat_pointcloud_searches
                         ? (double)m_stat_pointcloud_searches_total_results
                               / (double)m_stat_pointcloud_searches
                         : 0.0;
        out << "      average query results: " << fmtformat("{:.1f}", avg)
            << "\n";
        out << "      failures: " << m_stat_pointcloud_failures << "\n";
        out << "    pointcloud_get calls: " << m_stat_pointcloud_gets << "\n";
        out << "    pointcloud_write calls: " << m_stat_pointcloud_writes
            << "\n";
    }
    if (m_stat_reparam_calls_total) {
        print(out,
              "  ReParameter: {} calls ({}) total, changed {} calls ({})\n",
              (long long)m_stat_reparam_calls_total,
              OIIO::Strutil::memformat(m_stat_reparam_bytes_total),
              (long long)m_stat_reparam_calls_changed,
              OIIO::Strutil::memformat(m_stat_reparam_bytes_changed));
    }
    out << "  Memory total: " << m_stat_memory.memstat() << '\n';
    out << "    Master memory: " << m_stat_mem_master.memstat() << '\n';
    out << "        Master ops:            " << m_stat_mem_master_ops.memstat()
        << '\n';
    out << "        Master args:           " << m_stat_mem_master_args.memstat()
        << '\n';
    out << "        Master syms:           " << m_stat_mem_master_syms.memstat()
        << '\n';
    out << "        Master defaults:       "
        << m_stat_mem_master_defaults.memstat() << '\n';
    out << "        Master consts:         "
        << m_stat_mem_master_consts.memstat() << '\n';
    out << "    Instance memory: " << m_stat_mem_inst.memstat() << '\n';
    out << "        Instance syms:         " << m_stat_mem_inst_syms.memstat()
        << '\n';
    out << "        Instance param values: "
        << m_stat_mem_inst_paramvals.memstat() << '\n';
    out << "        Instance connections:  "
        << m_stat_mem_inst_connections.memstat() << '\n';

    size_t jitmem = LLVM_Util::total_jit_memory_held();
    out << "    LLVM JIT memory: " << Strutil::memformat(jitmem) << '\n';

    if (m_profile) {
        out << "  Execution profile:\n";
        out << "    Total shader execution time: "
            << Strutil::timeintervalformat(OIIO::Timer::seconds(
                                               m_stat_total_shading_time_ticks),
                                           2)
            << " (sum of all threads)\n";
        // Account for times of any groups that haven't yet been destroyed
        {
            spin_lock lock(m_all_shader_groups_mutex);
            for (auto&& grp : m_all_shader_groups) {
                if (ShaderGroupRef g = grp.lock()) {
                    long long ticks = g->m_stat_total_shading_time_ticks;
                    m_group_profile_times[g->name()] += ticks;
                    g->m_stat_total_shading_time_ticks -= ticks;
                }
            }
        }
        {
            spin_lock lock(m_stat_mutex);
            std::vector<GroupTimeVal> grouptimes;
            for (std::map<ustring, long long>::const_iterator m
                 = m_group_profile_times.begin();
                 m != m_group_profile_times.end(); ++m) {
                grouptimes.emplace_back(m->first, m->second);
            }
            std::sort(grouptimes.begin(), grouptimes.end(),
                      group_time_compare());
            if (grouptimes.size() > 5)
                grouptimes.resize(5);
            if (grouptimes.size())
                out << "    Most expensive shader groups:\n";
            for (std::vector<GroupTimeVal>::const_iterator i
                 = grouptimes.begin();
                 i != grouptimes.end(); ++i) {
                out << "      "
                    << Strutil::timeintervalformat(OIIO::Timer::seconds(
                                                       i->second),
                                                   2)
                    << ' '
                    << (i->first.size() ? i->first.c_str() : "<unnamed group>")
                    << "\n";
            }
        }
    }

    return out.str();
}



void
ShadingSystemImpl::printstats() const
{
    if (m_statslevel == 0)
        return;
    m_err->message(getstats(m_statslevel));
}



bool
ShadingSystemImpl::Parameter(string_view name, TypeDesc t, const void* val,
                             ParamHints hints)
{
    return Parameter(*m_curgroup, name, t, val, hints);
}



bool
ShadingSystemImpl::Parameter(ShaderGroup& group, string_view name, TypeDesc t,
                             const void* val, ParamHints hints)
{
    group.m_pending_params.emplace_back(name, t, 1, val);
    group.m_pending_hints.push_back(hints);
    return true;
}



ShaderGroupRef
ShadingSystemImpl::ShaderGroupBegin(string_view groupname)
{
    ShaderGroupRef group(new ShaderGroup(groupname, *this));
    group->m_exec_repeat = m_exec_repeat;
    {
        // Record the group in the SS's census of all extant groups
        spin_lock lock(m_all_shader_groups_mutex);
        // Group inherits global symbol location information that was
        // active at the time the group was created.
        group->add_symlocs(m_symlocs);
        m_all_shader_groups.push_back(group);
        ++m_groups_to_compile_count;
        m_curgroup = group;
    }
    return group;
}



bool
ShadingSystemImpl::ShaderGroupEnd(void)
{
    if (!m_curgroup) {
        error("ShaderGroupEnd() was called without ShaderGroupBegin()");
        return false;
    }
    bool ok = ShaderGroupEnd(*m_curgroup);
    m_curgroup.reset();  // no currently active group
    return ok;
}



bool
ShadingSystemImpl::ShaderGroupEnd(ShaderGroup& group)
{
    // Lock just in case we do something not thread-safe within
    // ShaderGroupEnd. This may be overly cautious, but unless it shows
    // up as a major bottleneck, I'm inclined to play it safe.
    lock_guard lock(m_mutex);

    // Mark the layers that can be run lazily
    if (!group.m_group_use.empty()) {
        int nlayers = group.nlayers();
        for (int layer = 0; layer < nlayers; ++layer) {
            ShaderInstance* inst = group[layer];
            if (!inst)
                continue;
            inst->last_layer(layer == nlayers - 1);
        }

        // Merge instances now if they really want it bad, otherwise wait
        // until we optimize the group.
        if (m_opt_merge_instances >= 2)
            merge_instances(group);
    }

    // Merge the raytype_queries of all the individual layers
    group.m_raytype_queries = 0;
    for (int layer = 0, n = group.nlayers(); layer < n; ++layer) {
        if (ShaderInstance* inst = group[layer])
            group.m_raytype_queries |= inst->master()->raytype_queries();
    }
    // std::cout << "Group " << group.name() << " ray query bits "
    //         << group.m_raytype_queries << "\n";

    ustring groupname = group.name();
    if (groupname.size() && groupname == m_archive_groupname) {
        std::string filename = m_archive_filename.string();
        if (!filename.size()) {
            filename = OIIO::Filesystem::filename(groupname.string())
                       + ".tar.gz";
            // Transform characters that will ruin our day when passed to
            // tar as a filename.
            if (OIIO::Strutil::contains(filename, ":"))
                filename = OIIO::Strutil::replace(filename, ":", "_");
            if (OIIO::Strutil::contains(filename, "|"))
                filename = OIIO::Strutil::replace(filename, "|", "_");
        }
        archive_shadergroup(group, filename);
    }

    group.m_complete = true;
    return true;
}



bool
ShadingSystemImpl::Shader(string_view shaderusage, string_view shadername,
                          string_view layername)
{
    // Make sure we have a current attrib state
    bool singleton = (!m_curgroup);
    if (singleton)
        ShaderGroupBegin("");

    return Shader(*m_curgroup, shaderusage, shadername, layername);
}



bool
ShadingSystemImpl::Shader(ShaderGroup& group, string_view shaderusage,
                          string_view shadername, string_view layername)
{
    ShaderMaster::ref master = loadshader(shadername);
    if (!master) {
        errorfmt("Could not find shader \"{}\"\n"
                 "        group: {}",
                 shadername, group.name());
        return false;
    }

    if (shaderusage.empty()) {
        errorfmt("Shader usage required\n"
                 "        group: {}",
                 shadername, group.name());
        return false;
    }

    // If a layer name was not supplied, make one up.
    std::string local_layername;
    if (layername.empty()) {
        local_layername = fmtformat("{}_{}", master->shadername(),
                                    group.nlayers());
        layername       = string_view(local_layername);
    }

    ShaderInstanceRef instance(new ShaderInstance(master, layername));
    instance->parameters(group.m_pending_params, group.m_pending_hints);
    group.m_pending_params.clear();
    group.m_pending_params.shrink_to_fit();
    group.m_pending_hints.clear();
    group.m_pending_hints.shrink_to_fit();

    if (group.m_group_use.empty()) {
        // First in a group
        group.clear();
        m_stat_groups += 1;
        group.m_group_use = shaderusage;
    } else if (shaderusage != group.m_group_use) {
        errorfmt("Shader usage \"{}\" does not match current group ({})\n"
                 "        group: {}",
                 shaderusage, group.m_group_use, group.name());
        return false;
    }

    group.append(instance);
    m_stat_groupinstances += 1;

    // FIXME -- check for duplicate layer name within the group?

    return true;
}



bool
ShadingSystemImpl::ConnectShaders(string_view srclayer, string_view srcparam,
                                  string_view dstlayer, string_view dstparam)
{
    if (!m_curgroup) {
        error("ConnectShaders can only be called within ShaderGroupBegin/End");
        return false;
    }
    return ConnectShaders(*m_curgroup, srclayer, srcparam, dstlayer, dstparam);
}



bool
ShadingSystemImpl::ConnectShaders(ShaderGroup& group, string_view srclayer,
                                  string_view srcparam, string_view dstlayer,
                                  string_view dstparam)
{
    // Basic sanity checks
    // ConnectShaders, and that the layer and parameter names are not empty.
    if (!srclayer.size() || !srcparam.size()) {
        errorfmt("ConnectShaders: badly formed source layer/parameter\n"
                 "        group: {}",
                 group.name());
        return false;
    }
    if (!dstlayer.size() || !dstparam.size()) {
        errorfmt("ConnectShaders: badly formed destination layer/parameter\n"
                 "        group: {}",
                 group.name());
        return false;
    }

    // Decode the layers, finding the indices within our group and
    // pointers to the instances.  Error and return if they are not found,
    // or if it's not connecting an earlier src to a later dst.
    ShaderInstance *srcinst, *dstinst;
    int srcinstindex = find_named_layer_in_group(group, ustring(srclayer),
                                                 srcinst);
    int dstinstindex = find_named_layer_in_group(group, ustring(dstlayer),
                                                 dstinst);
    if (!srcinst) {
        errorfmt("ConnectShaders: source layer \"{}\" not found\n"
                 "        group: {}",
                 srclayer, group.name());
        return false;
    }
    if (!dstinst) {
        errorfmt("ConnectShaders: destination layer \"{}\" not found\n"
                 "        group: {}",
                 dstlayer, group.name());
        return false;
    }
    if (dstinstindex <= srcinstindex) {
        errorfmt(
            "ConnectShaders: destination layer must follow source layer (tried to connect {}.{} -> {}.{})\n"
            "        group: {}",
            srclayer, srcparam, dstlayer, dstparam, group.name());
        return false;
    }

    // Decode the parameter names, find their symbols in their
    // respective layers, and also decode request to attach specific
    // array elements or color/vector channels.
    ConnectedParam srccon = decode_connected_param(srcparam, srclayer, srcinst);
    ConnectedParam dstcon = decode_connected_param(dstparam, dstlayer, dstinst);
    if (!(srccon.valid() && dstcon.valid())) {
        if (connection_error())
            errorfmt(
                "ConnectShaders: cannot connect a {} ({}) to a {} ({}), invalid connection\n"
                "        group: {}",
                srccon.type, srcparam, dstcon.type, dstparam, group.name());
        else
            warningfmt(
                "ConnectShaders: cannot connect a {} ({}) to a {} ({}), invalid connection\n"
                "        group: {}",
                srccon.type, srcparam, dstcon.type, dstparam, group.name());
        return false;
    }

    if (srccon.type.is_structure() && dstcon.type.is_structure()
        && equivalent(srccon.type, dstcon.type)) {
        // If the connection is whole struct-to-struct (and they are
        // structs with equivalent data layout), implement it underneath
        // as connections between their respective fields.
        StructSpec* srcstruct = srccon.type.structspec();
        StructSpec* dststruct = dstcon.type.structspec();
        for (size_t i = 0; i < (size_t)srcstruct->numfields(); ++i) {
            std::string s = fmtformat("{}.{}", srcparam,
                                      srcstruct->field(i).name);
            std::string d = fmtformat("{}.{}", dstparam,
                                      dststruct->field(i).name);
            ConnectShaders(group, srclayer, s, dstlayer, d);
        }
        return true;
    }

    if (!assignable(dstcon.type, srccon.type)) {
        if (connection_error())
            errorfmt("ConnectShaders: cannot connect a {} ({}) to a {} ({})\n"
                     "        group: {}",
                     srccon.type, srcparam, dstcon.type, dstparam,
                     group.name());
        else
            warningfmt("ConnectShaders: cannot connect a {} ({}) to a {} ({})\n"
                       "        group: {}",
                       srccon.type, srcparam, dstcon.type, dstparam,
                       group.name());
        return false;
    }

    const Symbol* dstsym = dstinst->mastersymbol(dstcon.param);
    if (dstsym && !dstsym->allowconnect()) {
        std::string name = dstlayer.size()
                               ? fmtformat("{}.{}", dstlayer, dstparam)
                               : std::string(dstparam);
        errorfmt(
            "ConnectShaders: cannot connect to {} because it has metadata allowconnect=0\n"
            "        group: {}",
            name, group.name());
        return false;
    }

    dstinst->add_connection(srcinstindex, srccon, dstcon);
    dstinst->instoverride(dstcon.param)->valuesource(Symbol::ConnectedVal);
    srcinst->instoverride(srccon.param)->connected_down(true);
    srcinst->outgoing_connections(true);

    // if (debug())
    //     messagefmt("ConnectShaders {} {} -> {} {}\n",
    //                srclayer, srcparam, dstlayer, dstparam);

    return true;
}



ShaderGroupRef
ShadingSystemImpl::ShaderGroupBegin(string_view groupname, string_view usage,
                                    string_view groupspec)
{
    ShaderGroupRef g = ShaderGroupBegin(groupname);
    bool err         = false;
    std::string errdesc;
    string_view errstatement;
    std::vector<int> intvals;
    std::vector<float> floatvals;
    std::vector<ustring> stringvals;
    string_view p = groupspec;  // parse view
    // std::cout << "!!!!!\n---\n" << groupspec << "\n---\n\n";
    while (p.size()) {
        string_view pstart = p;  // save where we were for error reporting
        Strutil::skip_whitespace(p);
        if (!p.size())
            break;
        while (Strutil::parse_char(p, ';'))  // skip blank statements
            ;
        string_view keyword = Strutil::parse_word(p);

        if (keyword == "shader") {
            string_view shadername = Strutil::parse_identifier(p);
            Strutil::skip_whitespace(p);
            string_view layername = Strutil::parse_until(p, " \t\r\n,;");
            bool ok               = Shader(*g, usage, shadername, layername);
            if (!ok) {
                errstatement = pstart;
                err          = true;
                break;
            }
            Strutil::parse_char(p, ';') || Strutil::parse_char(p, ',');
            Strutil::skip_whitespace(p);
            continue;
        }

        if (keyword == "connect") {
            Strutil::skip_whitespace(p);
            string_view lay1 = Strutil::parse_until(p, " \t\r\n.");
            Strutil::parse_char(p, '.');
            string_view param1 = Strutil::parse_until(p, " \t\r\n,;");
            Strutil::skip_whitespace(p);
            string_view lay2 = Strutil::parse_until(p, " \t\r\n.");
            Strutil::parse_char(p, '.');
            string_view param2 = Strutil::parse_until(p, " \t\r\n,;");
            bool ok            = ConnectShaders(*g, lay1, param1, lay2, param2);
            if (!ok) {
                errstatement = pstart;
                err          = true;
                break;
            }
            Strutil::parse_char(p, ';') || Strutil::parse_char(p, ',');
            Strutil::skip_whitespace(p);
            continue;
        }

        // Remaining case -- it should be declaring a parameter.
        string_view typestring;
        if (keyword == "param") {
            typestring = Strutil::parse_word(p);
        } else if (TypeDesc(keyword) != TypeDesc::UNKNOWN) {
            // compatibility: let the 'param' keyword be optional, if it's
            // obvious that it's a type name.
            typestring = keyword;
        } else {
            err     = true;
            errdesc = fmtformat("Unknown statement (expected 'param', "
                                "'shader', or 'connect'): \"{}\"",
                                keyword);
            break;
        }
        TypeDesc type;
        if (typestring == "int")
            type = TypeInt;
        else if (typestring == "float")
            type = TypeFloat;
        else if (typestring == "color")
            type = TypeColor;
        else if (typestring == "point")
            type = TypePoint;
        else if (typestring == "vector")
            type = TypeVector;
        else if (typestring == "normal")
            type = TypeNormal;
        else if (typestring == "matrix")
            type = TypeMatrix;
        else if (typestring == "string")
            type = TypeString;
        else {
            err     = true;
            errdesc = fmtformat("Unknown type: {}", typestring);
            break;  // error
        }
        if (Strutil::parse_char(p, '[')) {
            int arraylen = -1;
            Strutil::parse_int(p, arraylen);
            Strutil::parse_char(p, ']');
            type.arraylen = arraylen;
        }
        std::string paramname_string;
        while (1) {
            paramname_string += Strutil::parse_identifier(p);
            Strutil::skip_whitespace(p);
            if (Strutil::parse_char(p, '.')) {
                paramname_string += ".";
            } else {
                break;
            }
        }
        string_view paramname(paramname_string);
        int lockgeom     = m_lockgeom_default;
        ParamHints hints = ParamHints::none;
        // For speed, reserve space. Note that for "unsized" arrays, we only
        // preallocate 1 slot and let it grow as needed. That's ok. For
        // everything else, we will reserve the right amount up front.
        int vals_to_preallocate = type.is_unsized_array()
                                      ? 1
                                      : type.numelements() * type.aggregate;
        // Stop parsing values when we hit the limit based on the
        // declaration.
        int max_vals = type.is_unsized_array() ? 1 << 28 : vals_to_preallocate;
        if (type.basetype == TypeDesc::INT) {
            intvals.clear();
            intvals.reserve(vals_to_preallocate);
            int i;
            for (i = 0; i < max_vals; ++i) {
                int val = 0;
                if (Strutil::parse_int(p, val))
                    intvals.push_back(val);
                else
                    break;
            }
            if (type.is_unsized_array()) {
                // For unsized arrays, now set the size based on how many
                // values we actually read.
                type.arraylen = std::max(1, i / type.aggregate);
            }
            // Zero-pad if we parsed fewer values than we needed
            intvals.resize(type.numelements() * type.aggregate, 0);
            OSL_DASSERT(int(type.numelements()) * type.aggregate
                        == int(intvals.size()));
        } else if (type.basetype == TypeDesc::FLOAT) {
            floatvals.clear();
            floatvals.reserve(vals_to_preallocate);
            int i;
            for (i = 0; i < max_vals; ++i) {
                float val = 0;
                if (Strutil::parse_float(p, val))
                    floatvals.push_back(val);
                else
                    break;
            }
            if (type.is_unsized_array()) {
                // For unsized arrays, now set the size based on how many
                // values we actually read.
                type.arraylen = std::max(1, i / type.aggregate);
            }
            // Zero-pad if we parsed fewer values than we needed
            floatvals.resize(type.numelements() * type.aggregate, 0);
            OSL_DASSERT(int(type.numelements()) * type.aggregate
                        == int(floatvals.size()));
        } else if (type.basetype == TypeDesc::STRING) {
            stringvals.clear();
            stringvals.reserve(vals_to_preallocate);
            int i;
            for (i = 0; i < max_vals; ++i) {
                std::string unescaped;
                string_view s;
                Strutil::skip_whitespace(p);
                if (p.size() && p[0] == '\"') {
                    if (!Strutil::parse_string(p, s))
                        break;
                    unescaped = Strutil::unescape_chars(s);
                    s         = unescaped;
                } else {
                    s = Strutil::parse_until(p, " \t\r\n;");
                    if (s.size() == 0)
                        break;
                }
                stringvals.emplace_back(s);
            }
            if (type.is_unsized_array()) {
                // For unsized arrays, now set the size based on how many
                // values we actually read.
                type.arraylen = std::max(1, i / type.aggregate);
            }
            // Zero-pad if we parsed fewer values than we needed
            stringvals.resize(type.numelements() * type.aggregate, ustring());
            OSL_DASSERT(int(type.numelements()) * type.aggregate
                        == int(stringvals.size()));
        }

        if (Strutil::parse_prefix(p, "[[")) {  // hints
            do {
                Strutil::skip_whitespace(p);
                string_view hint_typename = Strutil::parse_word(p);
                string_view hint_name     = Strutil::parse_identifier(p);
                TypeDesc hint_type(hint_typename);
                if (!hint_name.size() || hint_type == TypeDesc::UNKNOWN) {
                    err     = true;
                    errdesc = "malformed hint";
                    break;
                }
                if (!Strutil::parse_char(p, '=')) {
                    err     = true;
                    errdesc = "hint expected value";
                    break;
                }
                if (hint_name == "lockgeom" && hint_type == TypeDesc::INT) {
                    if (!Strutil::parse_int(p, lockgeom)) {
                        err     = true;
                        errdesc = fmtformat("hint {} expected int value",
                                            hint_name);
                        break;
                    }
                    set(hints, ParamHints::interpolated, lockgeom == 0);
                } else if (hint_name == "interpolated"
                           && hint_type == TypeInt) {
                    int v = 0;
                    if (!Strutil::parse_int(p, v)) {
                        err     = true;
                        errdesc = fmtformat("hint {} expected int value",
                                            hint_name);
                        break;
                    }
                    set(hints, ParamHints::interpolated, v);
                } else if (hint_name == "interactive" && hint_type == TypeInt) {
                    int v = 0;
                    if (!Strutil::parse_int(p, v)) {
                        err     = true;
                        errdesc = fmtformat("hint {} expected int value",
                                            hint_name);
                        break;
                    }
                    set(hints, ParamHints::interactive, v);
                } else {
                    err     = true;
                    errdesc = fmtformat("unknown hint '{} {}'", hint_type,
                                        hint_name);
                    break;
                }
            } while (Strutil::parse_char(p, ','));
            if (err)
                break;
            if (!Strutil::parse_prefix(p, "]]")) {
                err     = true;
                errdesc = "malformed hint";
                break;
            }
        }

        bool ok = true;
        if (type.basetype == TypeDesc::INT) {
            ok = Parameter(*g, paramname, type, &intvals[0], hints);
        } else if (type.basetype == TypeDesc::FLOAT) {
            ok = Parameter(*g, paramname, type, &floatvals[0], hints);
        } else if (type.basetype == TypeDesc::STRING) {
            ok = Parameter(*g, paramname, type, &stringvals[0], hints);
        }
        if (!ok) {
            errstatement = pstart;
            err          = true;
            break;
        }

        Strutil::skip_whitespace(p);
        if (!p.size())
            break;

        if (Strutil::parse_char(p, ';') || Strutil::parse_char(p, ','))
            continue;  // next command

        Strutil::parse_until_char(p, ';');
        if (!Strutil::parse_char(p, ';')) {
            err     = true;
            errdesc = "semicolon expected";
        }
    }

    if (err) {
        std::string msg
            = fmtformat("ShaderGroupBegin: error parsing group description: {}\n"
                        "        group: {}",
                        errdesc, g->name());
        if (errstatement.empty()) {
            size_t offset     = p.data() - groupspec.data();
            size_t begin_stmt = std::min(groupspec.find_last_of(';', offset),
                                         groupspec.find_last_of(',', offset));
            size_t end_stmt   = groupspec.find_first_of(';', begin_stmt + 1);
            errstatement      = groupspec.substr(begin_stmt + 1,
                                                 end_stmt - begin_stmt);
        }
        if (errstatement.size())
            msg += fmtformat("\n        problem might be here: {}",
                             errstatement);
        error(msg);
        if (debug())
            infofmt("Broken group was:\n---{}\n---\n", groupspec);
        return ShaderGroupRef();
    }

    return g;
}



bool
ShadingSystemImpl::ReParameter(ShaderGroup& group, string_view layername_,
                               string_view paramname, TypeDesc type,
                               const void* val)
{
    // Find the named layer
    ustring layername(layername_);
    ShaderInstance* layer = nullptr;
    int layerindex        = -1;
    for (int i = 0, e = group.nlayers(); i < e; ++i) {
        if (group[i]->layername() == layername) {
            layer      = group[i];
            layerindex = i;
            break;
        }
    }
    if (!layer)
        return false;  // could not find the named layer

    // Find the named parameter within the layer
    int paramindex = layer->findparam(ustring(paramname),
                                      false /* don't go to master */);
    if (paramindex < 0) {
        paramindex = layer->findparam(ustring(paramname), true);
        if (paramindex >= 0)
            // This param exists, but it got optimized away, no failure
            return true;
    }
    if (paramindex < 0)
        return false;  // could not find the named parameter

    Symbol* sym = layer->symbol(paramindex);
    if (!sym) {
        // Can have a paramindex >= 0, but no symbol when it's a master-symbol
        OSL_DASSERT(layer->mastersymbol(paramindex)
                    && "No symbol for paramindex");
        return false;
    }

    // Check that it's declared to be an interactive parameter
    if (!sym->interactive()) {
        errorfmt(
            "ReParameter cannot adjust {}.{}, which was not declared interactive",
            layername, paramname);
        return false;
    }

    // Check for mismatch versus previously-declared type
    if ((relaxed_param_typecheck() && !relaxed_equivalent(sym->typespec(), type))
        || (!relaxed_param_typecheck()
            && !relaxed_equivalent(sym->typespec(), type)))
        return false;

    // Can't change param value if the group has already been optimized,
    // unless that parameter is marked lockgeom=0.
    if (group.optimized() && sym->lockgeom())
        return false;

    // Do the deed
    int offset = group.interactive_param_offset(layerindex, sym->name());

    if (offset >= 0) {
        size_t size = type.size();
        m_stat_reparam_calls_total += 1;
        m_stat_reparam_bytes_total += size;

        // Copy ustringhashes instead of ustrings
        const void* payload;
        ustringhash string_hash;
        if (type == TypeDesc::STRING) {
            string_hash = ustringhash_from(
                *reinterpret_cast<const ustring*>(val));
            payload = &string_hash;
        } else
            payload = val;

        if (memcmp(group.interactive_arena_ptr() + offset, payload, size)) {
            memcpy(group.interactive_arena_ptr() + offset, payload,
                   type.size());
            if (use_optix())
                renderer()->copy_to_device(
                    group.device_interactive_arena().d_get() + offset, payload,
                    type.size());
            m_stat_reparam_calls_changed += 1;
            m_stat_reparam_bytes_changed += size;
        }
        return true;
    } else
        return true;
}



PerThreadInfo*
ShadingSystemImpl::create_thread_info()
{
    return new PerThreadInfo;
}



void
ShadingSystemImpl::destroy_thread_info(PerThreadInfo* threadinfo)
{
    delete threadinfo;
}



ShadingContext*
ShadingSystemImpl::get_context(PerThreadInfo* threadinfo,
                               TextureSystem::Perthread* texture_threadinfo)
{
    if (!threadinfo) {
        error("ShadingSystem::get_context called without a PerThreadInfo");
        return nullptr;
    }
    ShadingContext* ctx = threadinfo->context_pool.empty()
                              ? new ShadingContext(*this, threadinfo)
                              : threadinfo->pop_context();
    ctx->texture_thread_info(texture_threadinfo);
    return ctx;
}



void
ShadingSystemImpl::release_context(ShadingContext* ctx)
{
    if (!ctx)
        return;
    ctx->process_errors();
    ctx->thread_info()->context_pool.push(ctx);
}



bool
ShadingSystemImpl::execute(ShadingContext& ctx, ShaderGroup& group,
                           int thread_index, int shade_index,
                           ShaderGlobals& ssg, void* userdata_base_ptr,
                           void* output_base_ptr, bool run)
{
    return ctx.execute(group, thread_index, shade_index, ssg, userdata_base_ptr,
                       output_base_ptr, run);
}



const void*
ShadingSystemImpl::get_symbol(ShadingContext& ctx, ustring layername,
                              ustring symbolname, TypeDesc& type)
{
    const Symbol* sym = ctx.symbol(layername, symbolname);
    if (sym) {
        type = sym->typespec().simpletype();
        return ctx.symbol_data(*sym);
    } else {
        return NULL;
    }
}



int
ShadingSystemImpl::find_named_layer_in_group(ShaderGroup& group,
                                             ustring layername,
                                             ShaderInstance*& inst)
{
    inst = NULL;
    if (group.m_group_use.empty())
        return -1;
    for (int i = 0; i < group.nlayers(); ++i) {
        if (group[i]->layername() == layername) {
            inst = group[i];
            return i;
        }
    }
    return -1;
}



ConnectedParam
ShadingSystemImpl::decode_connected_param(string_view connectionname,
                                          string_view layername,
                                          ShaderInstance* inst)
{
    ConnectedParam c;  // initializes to "invalid"

    // Look for a bracket in the "parameter name"
    size_t bracketpos = connectionname.find('[');
    // Grab just the part of the param name up to the bracket
    ustring param(connectionname, 0, bracketpos);
    string_view cname_remaining = connectionname.substr(bracketpos);

    // Search for the param with that name, fail if not found
    c.param = inst->findsymbol(param);
    if (c.param < 0) {
        if (connection_error())
            errorfmt(
                "ConnectShaders: \"{}\" is not a parameter or global of layer \"{}\" (shader \"{}\")",
                param, layername, inst->shadername());
        else
            warningfmt(
                "ConnectShaders: \"{}\" is not a parameter or global of layer \"{}\" (shader \"{}\")",
                param, layername, inst->shadername());
        return c;
    }

    const Symbol* sym = inst->mastersymbol(c.param);
    OSL_ASSERT(sym);

    // Only params, output params, and globals are legal for connections
    if (!(sym->symtype() == SymTypeParam || sym->symtype() == SymTypeOutputParam
          || sym->symtype() == SymTypeGlobal)) {
        errorfmt(
            "ConnectShaders: \"{}\" is not a parameter or global of layer \"{}\" (shader \"%s\")",
            param, layername, inst->shadername());
        c.param = -1;  // mark as invalid
        return c;
    }

    c.type = sym->typespec();

    if (!cname_remaining.empty() && c.type.is_array()) {
        // There was at least one set of brackets that appears to be
        // selecting an array element.
        int index = 0;
        if (!(Strutil::parse_char(cname_remaining, '[')
              && Strutil::parse_int(cname_remaining, index)
              && Strutil::parse_char(cname_remaining, ']'))) {
            errorfmt("ConnectShaders: malformed parameter \"{}\"",
                     connectionname);
            c.param = -1;  // mark as invalid
            return c;
        }
        c.arrayindex = index;
        if (c.arrayindex >= c.type.arraylength()) {
            errorfmt("ConnectShaders: cannot request array element {} from a {}",
                     connectionname, c.type);
            c.arrayindex = c.type.arraylength() - 1;  // clamp it
        }
        c.type.make_array(0);                       // chop to the element type
        Strutil::skip_whitespace(cname_remaining);  // skip to next bracket
    }

    if (!cname_remaining.empty() && cname_remaining.front() == '['
        && !c.type.is_closure() && c.type.aggregate() != TypeDesc::SCALAR) {
        // There was at least one set of brackets that appears to be
        // selecting a color/vector component.
        int index = 0;
        if (!(Strutil::parse_char(cname_remaining, '[')
              && Strutil::parse_int(cname_remaining, index)
              && Strutil::parse_char(cname_remaining, ']'))) {
            errorfmt("ConnectShaders: malformed parameter \"{}\"",
                     connectionname);
            c.param = -1;  // mark as invalid
            return c;
        }
        c.channel = index;
        if (c.channel >= (int)c.type.aggregate()) {
            errorfmt("ConnectShaders: cannot request component {} from a {}",
                     connectionname, c.type);
            c.channel = (int)c.type.aggregate() - 1;  // clamp it
        }
        // chop to just the scalar part
        c.type = TypeSpec((TypeDesc::BASETYPE)c.type.simpletype().basetype);
        Strutil::skip_whitespace(cname_remaining);
    }

    // Deal with left over nonsense or unsupported param designations
    if (!cname_remaining.empty()) {
        // Still a leftover bracket, no idea what to do about that
        errorfmt(
            "ConnectShaders: don't know how to connect '{}' when \"{}\" is a \"{}\"",
            connectionname, param, c.type);
        c.param = -1;  // mark as invalid
    }
    return c;
}



int
ShadingSystemImpl::raytype_bit(ustring name)
{
    for (size_t i = 0, e = m_raytypes.size(); i < e; ++i)
        if (name == m_raytypes[i])
            return (1 << i);
    return 0;  // not found
}



bool
ShadingSystemImpl::is_renderer_output(ustring layername, ustring paramname,
                                      ShaderGroup* group) const
{
    ustring name2 = ustring::fmtformat("{}.{}", layername, paramname);
    if (group) {
        for (auto&& sl : group->m_symlocs) {
            if (sl.arena == SymArena::Outputs
                && (sl == paramname || sl == name2))
                return true;
        }
        const std::vector<ustring>& aovs(group->m_renderer_outputs);
        if (aovs.size() > 0) {
            if (std::find(aovs.begin(), aovs.end(), paramname) != aovs.end())
                return true;
            // Try "layer.name"
            if (std::find(aovs.begin(), aovs.end(), name2) != aovs.end())
                return true;
        }
    }
    const std::vector<ustring>& aovs(m_renderer_outputs);
    if (aovs.size() > 0) {
        if (std::find(aovs.begin(), aovs.end(), paramname) != aovs.end())
            return true;
        // Try "layer.name"
        if (std::find(aovs.begin(), aovs.end(), name2) != aovs.end())
            return true;
    }
    return false;
}



void
ShadingSystemImpl::group_post_jit_cleanup(ShaderGroup& group)
{
    // Once we're generated the IR, we really don't need the ops and args,
    // and we only need the syms that include the params.
    off_t symmem         = 0;
    size_t connectionmem = 0;
    for (int layer = 0; layer < group.nlayers(); ++layer) {
        ShaderInstance* inst = group[layer];
        // We no longer needs ops and args -- create empty vectors and
        // swap with the ones in the instance.
        OpcodeVec emptyops;
        inst->ops().swap(emptyops);
        std::vector<int> emptyargs;
        inst->args().swap(emptyargs);
        if (inst->unused()) {
            // If we'll never use the layer, we don't need the syms at all
            SymbolVec nosyms;
            std::swap(inst->symbols(), nosyms);
            symmem += vectorbytes(nosyms);
            // also don't need the connection info any more
            connectionmem += (off_t)inst->clear_connections();
        }
    }
    {
        // adjust memory stats
        spin_lock lock(m_stat_mutex);
        m_stat_mem_inst_syms -= symmem;
        m_stat_mem_inst_connections -= connectionmem;
        m_stat_mem_inst -= symmem + connectionmem;
        m_stat_memory -= symmem + connectionmem;
    }
}



void
ShadingSystemImpl::optimize_group(ShaderGroup& group, ShadingContext* ctx,
                                  bool do_jit)
{
    if (ctx) {
        // Always have ShadingContext remember the group we just optimized
        // to allow calls to find_symbol and get_symbol to be valid after
        // optimization without needed to execute or jit
        ctx->group(&group);
    }
    if (group.optimized() && (!do_jit || group.jitted()))
        return;  // already optimized and optionally jitted

    OIIO::Timer timer;
    lock_guard lock(group.m_mutex);
    bool need_jit = do_jit && !group.jitted();
    if (group.optimized() && !need_jit) {
        // The group was somehow optimized by another thread between the
        // time we checked group.optimized() and now that we have the lock.
        // Nothing to do but record how long we waited for the lock.
        spin_lock stat_lock(m_stat_mutex);
        double t = timer();
        m_stat_optimization_time += t;
        m_stat_opt_locking_time += t;
        return;
    }

    if (!m_only_groupname.empty() && m_only_groupname != group.name()) {
        // For debugging purposes, we are requested to compile only one
        // shader group, and this is not it.  Mark it as does_nothing,
        // and also as optimized so nobody locks on it again, and record
        // how long we waited for the lock.
        group.does_nothing(true);
        group.m_optimized = true;
        group.m_jitted    = true;
        spin_lock stat_lock(m_stat_mutex);
        double t = timer();
        m_stat_optimization_time += t;
        m_stat_opt_locking_time += t;
        return;
    }

    double locking_time = timer();

    bool ctx_allocated         = false;
    PerThreadInfo* thread_info = nullptr;
    if (!ctx) {
        thread_info   = create_thread_info();
        ctx           = get_context(thread_info);
        ctx_allocated = true;
    }
    if (!group.optimized()) {
        RuntimeOptimizer rop(*this, group, ctx);
        rop.run();
        rop.police_failed_optimizations();

        // Copy some info recorded by the RuntimeOptimizer into the group
        group.m_unknown_textures_needed = rop.m_unknown_textures_needed;
        for (auto&& f : rop.m_textures_needed)
            group.m_textures_needed.push_back(f);
        group.m_unknown_closures_needed = rop.m_unknown_closures_needed;
        for (auto&& f : rop.m_closures_needed)
            group.m_closures_needed.push_back(f);
        for (auto&& f : rop.m_globals_needed)
            group.m_globals_needed.push_back(f);
        group.m_globals_read  = rop.m_globals_read;
        group.m_globals_write = rop.m_globals_write;
        size_t num_userdata   = rop.m_userdata_needed.size();
        group.m_userdata_names.reserve(num_userdata);
        group.m_userdata_types.reserve(num_userdata);
        group.m_userdata_offsets.resize(num_userdata, 0);
        group.m_userdata_derivs.reserve(num_userdata);
        group.m_userdata_layers.reserve(num_userdata);
        group.m_userdata_init_vals.reserve(num_userdata);
        for (auto&& n : rop.m_userdata_needed) {
            group.m_userdata_names.push_back(n.name);
            group.m_userdata_types.push_back(n.type);
            group.m_userdata_derivs.push_back(n.derivs);
            group.m_userdata_layers.push_back(n.layer_num);
            group.m_userdata_init_vals.push_back(n.data);
        }
        group.m_unknown_attributes_needed = rop.m_unknown_attributes_needed;
        for (auto&& f : rop.m_attributes_needed) {
            group.m_attributes_needed.push_back(f.name);
            group.m_attribute_scopes.push_back(f.scope);
            group.m_attribute_types.push_back(f.type);
            group.m_attribute_derivs.push_back(f.derivs);
        }
        group.m_optimized = true;

        if (use_optix_cache())
            group.generate_optix_cache_key(rop.serialize());

        spin_lock stat_lock(m_stat_mutex);
        if (!need_jit) {
            m_stat_opt_locking_time += locking_time;
            m_stat_optimization_time += timer();
        }
        m_stat_opt_locking_time += rop.m_stat_opt_locking_time;
        m_stat_opt_locking_time += locking_time + rop.m_stat_opt_locking_time;
        m_stat_specialization_time += rop.m_stat_specialization_time;
    }

    if (need_jit) {
        bool cached = false;
        if (use_optix_cache()) {
            std::string cache_key = group.optix_cache_key();

            std::string cache_value;
            if (renderer()->cache_get("optix_ptx", cache_key, cache_value)) {
                cached = true;
                optix_cache_unwrap(cache_value,
                                   group.m_llvm_ptx_compiled_version,
                                   group.m_llvm_groupdata_size);
            }
        }

        if (!cached) {
            BackendLLVM lljitter(*this, group, ctx);
            lljitter.run();

            // NOTE: it is now possible to optimize and not JIT
            // which would leave the cleanup to happen
            // when the ShadingSystem is destroyed

            // Only cleanup when are not batching or if
            // the batch jit has already happened,
            // as it requires the ops so we can't delete them yet!
            if (((renderer()->batched(WidthOf<16>()) == nullptr)
                 && (renderer()->batched(WidthOf<8>()) == nullptr)
                 && (renderer()->batched(WidthOf<4>()) == nullptr))
                || group.batch_jitted()) {
                group_post_jit_cleanup(group);
            }

            group.m_jitted = true;
            spin_lock stat_lock(m_stat_mutex);
            m_stat_opt_locking_time += locking_time;
            m_stat_optimization_time += timer();
            m_stat_total_llvm_time += lljitter.m_stat_total_llvm_time;
            m_stat_llvm_setup_time += lljitter.m_stat_llvm_setup_time;
            m_stat_llvm_irgen_time += lljitter.m_stat_llvm_irgen_time;
            m_stat_llvm_opt_time += lljitter.m_stat_llvm_opt_time;
            m_stat_llvm_jit_time += lljitter.m_stat_llvm_jit_time;
            m_stat_max_llvm_local_mem = std::max(m_stat_max_llvm_local_mem,
                                                 lljitter.m_llvm_local_mem);
        }
    }

    if (ctx_allocated) {
        release_context(ctx);
        destroy_thread_info(thread_info);
    }

    m_stat_groups_compiled += 1;
    m_stat_instances_compiled += group.nlayers();
    m_groups_to_compile_count -= 1;
}

#if OSL_USE_BATCHED
template<int WidthT>
void
ShadingSystemImpl::Batched<WidthT>::jit_group(ShaderGroup& group,
                                              ShadingContext* ctx)
{
    if (group.batch_jitted())
        return;  // already optimized

    bool ctx_allocated         = false;
    PerThreadInfo* thread_info = nullptr;
    if (!ctx) {
        thread_info   = m_ssi.create_thread_info();
        ctx           = m_ssi.get_context(thread_info);
        ctx_allocated = true;
    }

    if (!group.optimized())
        m_ssi.optimize_group(group, ctx, false /*do_jit*/);

    OIIO::Timer timer;
    // TODO: we could have separate mutexes for jit vs. batched_jit
    // choose to keep it simple to start with
    lock_guard lock(group.m_mutex);
    if (group.batch_jitted()) {
        if (ctx_allocated) {
            // TODO: scope object to manage temporary context&threadinfo
            m_ssi.release_context(ctx);
            m_ssi.destroy_thread_info(thread_info);
        }

        // The group was somehow batch_jitted by another thread between the
        // time we checked group.batch_jitted() and now that we have the lock.
        // Nothing to do (expect maybe record how long we waited for the lock).
        spin_lock stat_lock(m_ssi.m_stat_mutex);
        double t = timer();
        m_ssi.m_stat_optimization_time += t;
        m_ssi.m_stat_opt_locking_time += t;
        return;
    }
    double locking_time = timer();

    // TODO:  Add BatchedBackendLLVM in subsequent pull request
    BatchedBackendLLVM lljitter(m_ssi, group, ctx, WidthT);
    lljitter.run();

    // Keep OSL instructions around in case someone
    // wants the scalar version jitted
    if (group.jitted()) {
        m_ssi.group_post_jit_cleanup(group);
    }

    if (ctx_allocated) {
        m_ssi.release_context(ctx);
        m_ssi.destroy_thread_info(thread_info);
    }

    group.m_batch_jitted = true;
    spin_lock stat_lock(m_ssi.m_stat_mutex);
    m_ssi.m_stat_opt_locking_time += locking_time;
    m_ssi.m_stat_optimization_time += timer();
    m_ssi.m_stat_total_llvm_time += lljitter.m_stat_total_llvm_time;
    m_ssi.m_stat_llvm_setup_time += lljitter.m_stat_llvm_setup_time;
    m_ssi.m_stat_llvm_irgen_time += lljitter.m_stat_llvm_irgen_time;
    m_ssi.m_stat_llvm_opt_time += lljitter.m_stat_llvm_opt_time;
    m_ssi.m_stat_llvm_jit_time += lljitter.m_stat_llvm_jit_time;
    m_ssi.m_stat_max_llvm_local_mem = std::max(m_ssi.m_stat_max_llvm_local_mem,
                                               lljitter.m_llvm_local_mem);

    // TODO: not sure how to count these given batched vs. not
    m_ssi.m_stat_groups_compiled += 1;
    m_ssi.m_stat_instances_compiled += group.nlayers();
    m_ssi.m_groups_to_compile_count -= 1;
}
#endif

static void
optimize_all_groups_wrapper(ShadingSystemImpl* ss, int mythread,
                            int totalthreads, bool do_jit)
{
    ss->optimize_all_groups(1, mythread, totalthreads, do_jit);
}


#if OSL_USE_BATCHED
template<int WidthT>
static void
batched_jit_all_groups_wrapper(ShadingSystemImpl* ss, int mythread,
                               int totalthreads)
{
    ss->batched<WidthT>().jit_all_groups(1, mythread, totalthreads);
}
#endif

void
ShadingSystemImpl::optimize_all_groups(int nthreads, int mythread,
                                       int totalthreads, bool do_jit)
{
    // Spawn a bunch of threads to do this in parallel -- just call this
    // routine again (with threads=1) for each thread.
    if (nthreads < 1)  // threads <= 0 means use all hardware available
        nthreads = std::min((int)std::thread::hardware_concurrency(),
                            (int)m_groups_to_compile_count);
    if (nthreads > 1) {
        if (m_threads_currently_compiling)
            return;  // never mind, somebody else spawned the JIT threads
        OIIO::thread_group threads;
        m_threads_currently_compiling += nthreads;
        for (int t = 0; t < nthreads; ++t)
            threads.add_thread(new std::thread(optimize_all_groups_wrapper,
                                               this, t, nthreads, do_jit));
        threads.join_all();
        m_threads_currently_compiling -= nthreads;
        return;
    }

    // And here's the single thread case
    size_t ngroups = 0;
    {
        spin_lock lock(m_all_shader_groups_mutex);
        ngroups = m_all_shader_groups.size();
    }
    PerThreadInfo* threadinfo = create_thread_info();
    ShadingContext* ctx       = get_context(threadinfo);
    for (size_t i = 0; i < ngroups; ++i) {
        // Assign to threads based on mod of totalthreads
        if ((i % totalthreads) == (unsigned)mythread) {
            ShaderGroupRef group;
            {
                spin_lock lock(m_all_shader_groups_mutex);
                group = m_all_shader_groups[i].lock();
            }
            if (group && group->m_complete)
                optimize_group(*group, ctx, do_jit);
        }
    }
    release_context(ctx);
    destroy_thread_info(threadinfo);
}

#if OSL_USE_BATCHED
template<int WidthT>
void
ShadingSystemImpl::Batched<WidthT>::jit_all_groups(int nthreads, int mythread,
                                                   int totalthreads)
{
    // Spawn a bunch of threads to do this in parallel -- just call this
    // routine again (with threads=1) for each thread.
    if (nthreads < 1)  // threads <= 0 means use all hardware available
        nthreads = std::min((int)std::thread::hardware_concurrency(),
                            (int)m_ssi.m_groups_to_compile_count);
    if (nthreads > 1) {
        if (m_ssi.m_threads_currently_compiling)
            return;  // never mind, somebody else spawned the JIT threads
        OIIO::thread_group threads;
        m_ssi.m_threads_currently_compiling += nthreads;
        for (int t = 0; t < nthreads; ++t)
            threads.add_thread(
                new std::thread(batched_jit_all_groups_wrapper<WidthT>, &m_ssi,
                                t, nthreads));
        threads.join_all();
        m_ssi.m_threads_currently_compiling -= nthreads;
        return;
    }

    // And here's the single thread case
    size_t ngroups = 0;
    {
        spin_lock lock(m_ssi.m_all_shader_groups_mutex);
        ngroups = m_ssi.m_all_shader_groups.size();
    }
    PerThreadInfo* threadinfo = m_ssi.create_thread_info();
    ShadingContext* ctx       = m_ssi.get_context(threadinfo);
    for (size_t i = 0; i < ngroups; ++i) {
        // Assign to threads based on mod of totalthreads
        if ((i % totalthreads) == (unsigned)mythread) {
            ShaderGroupRef group;
            {
                spin_lock lock(m_ssi.m_all_shader_groups_mutex);
                group = m_ssi.m_all_shader_groups[i].lock();
            }
            if (group)
                jit_group(*group, ctx);
        }
    }
    m_ssi.release_context(ctx);
    m_ssi.destroy_thread_info(threadinfo);
}

// Explicitly instantiate, although might need to specialize on target
// machine as well, start with just the batch size
template class pvt::ShadingSystemImpl::Batched<16>;
template class pvt::ShadingSystemImpl::Batched<8>;
template class pvt::ShadingSystemImpl::Batched<4>;
#endif

int
ShadingSystemImpl::merge_instances(ShaderGroup& group, bool post_opt)
{
    // Look through the shader group for pairs of nodes/layers that
    // actually do exactly the same thing, and eliminate one of the
    // redundant shaders, carefully rewiring all its outgoing
    // connections to later layers to refer to the one we keep.
    //
    // It turns out that in practice, it's not uncommon to have
    // duplicate nodes.  For example, some materials are "layered" --
    // like a character skin shader that has separate sub-networks for
    // skin, oil, wetness, and so on -- and those different sub-nets
    // often reference the same texture maps or noise functions by
    // repetition.  Yes, ideally, the redundancies would be eliminated
    // before they were fed to the renderer, but in practice that's hard
    // and for many scenes we get substantial savings of time (mostly
    // because of reduced texture calls) and instance memory by finding
    // these redundancies automatically.  The amount of savings is quite
    // scene dependent, as well as probably very dependent on the
    // general shading and lookdev approach of the studio.  But it was
    // very helpful for us in many cases.
    //
    // The basic loop below looks very inefficient, O(n^2) in number of
    // instances in the group. But it's really not -- a few seconds (sum
    // of all threads) for even our very complex scenes. This is because
    // most potential pairs have a very fast rejection case if they are
    // not using the same master.  Since there's no appreciable cost to
    // the brute force approach, it seems silly to have a complex scheme
    // to try to reduce the number of pairings.

    if (!m_opt_merge_instances || optimize() < 1)
        return 0;

    OIIO::Timer timer;         // Time we spend looking for and doing merges
    int merges           = 0;  // number of merges we do
    size_t connectionmem = 0;  // Connection memory we free
    int nlayers          = group.nlayers();

    // Need to quickly make sure userdata_params is up to date before any
    // mergeability tests.
    for (int layer = 0; layer < nlayers; ++layer)
        if (!group[layer]->unused())
            group[layer]->evaluate_writes_globals_and_userdata_params();

    // Loop over all layers...
    for (int a = 0; a < nlayers - 1; ++a) {
        if (group[a]->unused()
            || group[a]->entry_layer())  // Don't merge a layer that's not used
            continue;                    // or if it's an entry layer
        // Check all later layers...
        for (int b = a + 1; b < nlayers; ++b) {
            if (group[b]->unused())  // Don't merge a layer that's not used
                continue;
            if (b == nlayers - 1)  // Don't merge the last layer -- causes
                continue;          // many tears because it's the group entry

            // Now we have two used layers, a and b, to examine.
            // See if they are mergeable (identical).  All the heavy
            // lifting is done by ShaderInstance::mergeable().
            if (!group[a]->mergeable(*group[b], group))
                continue;

            // The two nodes a and b are mergeable, so merge them.
            ShaderInstance* A = group[a];
            ShaderInstance* B = group[b];
            ++merges;

            // We'll keep A, get rid of B.  For all layers later than B,
            // check its incoming connections and replace all references
            // to B with references to A.
            for (int j = b + 1; j < nlayers; ++j) {
                ShaderInstance* inst = group[j];
                if (inst->unused())  // don't bother if it's unused
                    continue;
                for (int c = 0, ce = inst->nconnections(); c < ce; ++c) {
                    Connection& con = inst->connection(c);
                    if (con.srclayer == b) {
                        con.srclayer = a;
                        A->outgoing_connections(true);
                        if (A->symbols().size() && B->symbols().size()) {
                            OSL_DASSERT(A->symbol(con.src.param)->name()
                                        == B->symbol(con.src.param)->name());
                        }
                    }
                }
            }

            // Mark parameters of B as no longer connected
            for (int p = B->firstparam(); p < B->lastparam(); ++p) {
                if (B->symbols().size())
                    B->symbol(p)->connected_down(false);
                if (B->m_instoverrides.size())
                    B->instoverride(p)->connected_down(false);
            }
            // B won't be used, so mark it as having no outgoing
            // connections and clear its incoming connections (which are
            // no longer used).
            OSL_DASSERT(B->merged_unused() == false);
            B->outgoing_connections(false);
            connectionmem += B->clear_connections();
            B->m_merged_unused = true;
            OSL_DASSERT(B->unused());
        }
    }

    {
        // Adjust stats
        spin_lock lock(m_stat_mutex);
        m_stat_mem_inst_connections -= connectionmem;
        m_stat_mem_inst -= connectionmem;
        m_stat_memory -= connectionmem;
        if (post_opt)
            m_stat_merged_inst_opt += merges;
        else
            m_stat_merged_inst += merges;
        m_stat_inst_merge_time += timer();
    }

    return merges;
}



#ifndef __CUDACC__

OIIO::ColorProcessorHandle
OCIOColorSystem::load_transform(ustring fromspace, ustring tospace,
                                ShadingSystemImpl* ss)
{
    if (fromspace != m_last_colorproc_fromspace
        || tospace != m_last_colorproc_tospace) {
        m_last_colorproc = colorconfig(ss).createColorProcessor(fromspace,
                                                                tospace);
        m_last_colorproc_fromspace = fromspace;
        m_last_colorproc_tospace   = tospace;
    }
    return m_last_colorproc;
}



const OIIO::ColorConfig&
OCIOColorSystem::colorconfig(ShadingSystemImpl* shadingsys)
{
    if (!m_colorconfig) {
        if (shadingsys)
            m_colorconfig = shadingsys->colorconfig();
        else
            m_colorconfig.reset(new OIIO::ColorConfig);
    }
    return *m_colorconfig;
}

#endif



std::shared_ptr<OIIO::ColorConfig>
ShadingSystemImpl::colorconfig()
{
    lock_guard lock(m_mutex);
    if (!m_colorconfig)
        m_colorconfig.reset(new OIIO::ColorConfig);
    return m_colorconfig;
}



bool
ShadingSystemImpl::archive_shadergroup(ShaderGroup& group, string_view filename)
{
    std::string filename_base = OIIO::Filesystem::filename(filename);
    std::string extension;
    for (std::string e = OIIO::Filesystem::extension(filename);
         e.size() && filename.size();
         e = OIIO::Filesystem::extension(filename)) {
        extension = e + extension;
        filename.remove_suffix(e.size());
    }
    if (extension.size() < 2 || extension[0] != '.') {
        errorfmt("archive_shadergroup: invalid filename \"{}\"", filename);
        return false;
    }
    filename_base.erase(filename_base.size() - extension.size());

    std::string pattern = OIIO::Filesystem::temp_directory_path()
                          + "/OSL-%%%%-%%%%";
    if (!pattern.size()) {
        error("archive_shadergroup: Could not find a temp directory");
        return false;
    }
    std::string tmpdir = OIIO::Filesystem::unique_path(pattern);
    if (!pattern.size()) {
        error("archive_shadergroup: Could not find a temp filename");
        return false;
    }
    std::string errmessage;
    bool dir_ok = OIIO::Filesystem::create_directory(tmpdir, errmessage);
    if (!dir_ok) {
        errorfmt("archive_shadergroup: Could not create temp directory {} ({})",
                 tmpdir, errmessage);
        return false;
    }

    bool ok                   = true;
    std::string groupfilename = tmpdir + "/shadergroup";
    OIIO::ofstream groupfile;
    OIIO::Filesystem::open(groupfile, groupfilename);
    if (groupfile.good()) {
        groupfile << group.serialize();
        groupfile.close();
    } else {
        error("archive_shadergroup: Could not open shadergroup file");
        ok = false;
    }

    std::string filename_list = "shadergroup";
    {
        std::lock_guard<ShaderGroup> lock(group);
        std::set<std::string> entries;  // to avoid duplicates
        for (int i = 0, nl = group.nlayers(); i < nl; ++i) {
            std::string osofile = group[i]->master()->osofilename();
            std::string osoname = OIIO::Filesystem::filename(osofile);
            if (entries.find(osoname) == entries.end()) {
                entries.insert(osoname);
                std::string localfile = tmpdir + "/" + osoname;
                OIIO::Filesystem::copy(osofile, localfile);
                filename_list
                    += Strutil::fmt::format(" \"{}\"",
                                            Strutil::escape_chars(osoname));
            }
        }
    }

    std::string full_filename
        = Strutil::fmt::format("{}{}", Strutil::escape_chars(filename),
                               extension);
    if (extension == ".tar" || extension == ".tar.gz" || extension == ".tgz") {
        std::string z   = Strutil::ends_with(extension, "gz") ? "-z" : "";
        std::string cmd = fmtformat("tar -c {} -C \"{}\" -f \"{}\" {}", z,
                                    Strutil::escape_chars(tmpdir),
                                    full_filename, filename_list);
        // std::cout << "Command =\n" << cmd << "\n";
        if (system(cmd.c_str()) != 0) {
            error("archive_shadergroup: executing tar command failed");
            ok = false;
        }

    } else if (extension == ".zip") {
        std::string cmd = fmtformat("zip -q \"{}\" {}", full_filename,
                                    filename_list);
        // std::cout << "Command =\n" << cmd << "\n";
        if (system(cmd.c_str()) != 0) {
            error("archive_shadergroup: executing zip command failed");
            ok = false;
        }
    } else {
        error("archive_shadergroup: no archiving/compressing command");
        ok = false;
    }

    OIIO::Filesystem::remove_all(tmpdir);

    return ok;
}



void
ShadingSystemImpl::register_inline_function(ustring name)
{
    m_inline_functions.insert(name);
}



void
ShadingSystemImpl::unregister_inline_function(ustring name)
{
    m_inline_functions.erase(name);
}



void
ShadingSystemImpl::register_noinline_function(ustring name)
{
    m_noinline_functions.insert(name);
}



void
ShadingSystemImpl::unregister_noinline_function(ustring name)
{
    m_noinline_functions.erase(name);
}



void
ClosureRegistry::register_closure(string_view name, int id,
                                  const ClosureParam* params,
                                  PrepareClosureFunc prepare,
                                  SetupClosureFunc setup)
{
    if (m_closure_table.size() <= (size_t)id)
        m_closure_table.resize(id + 1);
    ClosureEntry& entry = m_closure_table[id];
    entry.id            = id;
    entry.name          = name;
    entry.nformal       = 0;
    entry.nkeyword      = 0;
    entry.struct_size   = 0; /* params could be NULL */
    for (int i = 0; params; ++i) {
        /* always push so the end marker is there */
        entry.params.push_back(params[i]);
        if (params[i].type == TypeDesc()) {
            entry.struct_size = params[i].offset;
            /* CLOSURE_FINISH_PARAM stashes the real struct alignment here
             * make sure that the closure struct doesn't want more alignment than ClosureComponent
             * because we will be allocating the real struct inside it. */
            OSL_ASSERT_MSG(
                params[i].field_size <= int(alignof(ClosureComponent)),
                "Closure %s wants alignment of %d which is larger than that of ClosureComponent",
                std::string(name).c_str(), params[i].field_size);
            break;
        }
        if (params[i].key == nullptr)
            entry.nformal++;
        else
            entry.nkeyword++;
    }
    entry.prepare                       = prepare;
    entry.setup                         = setup;
    m_closure_name_to_id[ustring(name)] = id;
}



const ClosureRegistry::ClosureEntry*
ClosureRegistry::get_entry(ustring name) const
{
    std::map<ustring, int>::const_iterator i = m_closure_name_to_id.find(name);
    if (i != m_closure_name_to_id.end()) {
        OSL_DASSERT((size_t)i->second < m_closure_table.size());
        return &m_closure_table[i->second];
    } else
        return NULL;
}


};  // namespace pvt



template<>
bool
ShadingContext::ocio_transform(ustring fromspace, ustring tospace,
                               const Color3& C, Color3& Cout)
{
#ifndef __CUDA_ARCH__
    if (auto cp = m_ocio_system.load_transform(fromspace, tospace,
                                               &shadingsys())) {
        Cout = C;
        cp->apply((float*)&Cout);
        return true;
    }
#endif
    return false;
}


template<>
bool
ShadingContext::ocio_transform(ustring fromspace, ustring tospace,
                               const Dual2<Color3>& C, Dual2<Color3>& Cout)
{
#ifndef __CUDA_ARCH__
    if (auto cp = m_ocio_system.load_transform(fromspace, tospace,
                                               &shadingsys())) {
        // Use finite differencing to approximate the derivative. Make 3
        // color values to convert.
        const float eps = 0.001f;
        Color3 CC[3]    = { C.val(), C.val() + eps * C.dx(),
                            C.val() + eps * C.dy() };
        cp->apply((float*)&CC, 3, 1, 3, sizeof(float), sizeof(Color3),
                  3 * sizeof(Color3));
        Cout.set(CC[0], (CC[1] - CC[0]) * (1.0f / eps),
                 (CC[2] - CC[0]) * (1.0f / eps));
        return true;
    }
#endif
    return false;
}



OSL_NAMESPACE_END



OSLQuery
OSL::ShadingSystem::oslquery(const ShaderGroup& group, int layernum)
{
    OSLQuery Q;  // This will use named return value optimization
    if (layernum < 0 || layernum >= group.nlayers()) {
        Q.errorfmt("Invalid layer number {} (valid indices: 0-{}).", layernum,
                   group.nlayers() - 1);
        return Q;
    }

    const ShaderMaster* master = group[layernum]->master();
    Q.m_shadername             = master->shadername();
    Q.m_shadertypename         = master->shadertypename();
    Q.m_params.clear();
    if (int nparams = master->num_params()) {
        Q.m_params.resize(nparams);
        for (int i = 0; i < nparams; ++i) {
            const Symbol* sym = master->symbol(i);
            OSLQuery::Parameter& p(Q.m_params[i]);
            p.name = sym->name().string();
            const TypeSpec& ts(sym->typespec());
            p.type        = ts.simpletype();
            p.isoutput    = (sym->symtype() == SymTypeOutputParam);
            p.varlenarray = ts.is_unsized_array();
            p.isstruct    = ts.is_structure() || ts.is_structure_array();
            p.isclosure   = ts.is_closure_based();
            p.data        = sym->data();
            if (p.type.arraylen >= 0) {
                int n = int(p.type.numelements() * p.type.aggregate);
                if (p.type.basetype == TypeDesc::INT) {
                    for (int i = 0; i < n; ++i)
                        p.idefault.push_back(sym->get_int(i));
                }
                if (p.type.basetype == TypeDesc::FLOAT) {
                    for (int i = 0; i < n; ++i)
                        p.fdefault.push_back(sym->get_float(i));
                }
                if (p.type.basetype == TypeDesc::STRING) {
                    for (int i = 0; i < n; ++i)
                        p.sdefault.push_back(sym->get_string(i));
                }
            }
            if (StructSpec* ss = ts.structspec()) {
                p.structname = ss->name().string();
                for (size_t i = 0, e = ss->numfields(); i < e; ++i)
                    p.fields.push_back(ss->field(i).name);
            } else {
                p.structname.clear();
            }
            p.validdefault = (p.data != nullptr);
        }
    }

    return Q;
}



OSL::OSLQuery::OSLQuery(const ShaderGroup* group, int layernum)
{
    init(group, layernum);
}



void
ShadingSystem::register_inline_function(ustring name)
{
    return m_impl->register_inline_function(name);
}



void
ShadingSystem::unregister_inline_function(ustring name)
{
    return m_impl->unregister_inline_function(name);
}



void
ShadingSystem::register_noinline_function(ustring name)
{
    return m_impl->register_noinline_function(name);
}



void
ShadingSystem::unregister_noinline_function(ustring name)
{
    return m_impl->unregister_noinline_function(name);
}



// DEPRECATED(1.12)
bool
OSL::OSLQuery::init(const ShaderGroup* group, int layernum)
{
    geterror();  // clear the error, we're newly initializing
    if (!group) {
        errorfmt("No group pointer supplied.");
        return false;
    }
    if (layernum < 0 || layernum >= group->nlayers()) {
        errorfmt("Invalid layer number {} (valid indices: 0-{}).", layernum,
                 group->nlayers() - 1);
        return false;
    }

    const ShaderMaster* master = (*group)[layernum]->master();
    m_shadername               = master->shadername();
    m_shadertypename           = master->shadertypename();
    m_params.clear();
    if (int nparams = master->num_params()) {
        m_params.resize(nparams);
        for (int i = 0; i < nparams; ++i) {
            const Symbol* sym = master->symbol(i);
            Parameter& p(m_params[i]);
            p.name = sym->name().string();
            const TypeSpec& ts(sym->typespec());
            p.type        = ts.simpletype();
            p.isoutput    = (sym->symtype() == SymTypeOutputParam);
            p.varlenarray = ts.is_unsized_array();
            p.isstruct    = ts.is_structure() || ts.is_structure_array();
            p.isclosure   = ts.is_closure_based();
            p.data        = sym->data();
            if (p.type.arraylen >= 0) {
                int n = int(p.type.numelements() * p.type.aggregate);
                if (p.type.basetype == TypeDesc::INT) {
                    for (int i = 0; i < n; ++i)
                        p.idefault.push_back(sym->get_int(i));
                }
                if (p.type.basetype == TypeDesc::FLOAT) {
                    for (int i = 0; i < n; ++i)
                        p.fdefault.push_back(sym->get_float(i));
                }
                if (p.type.basetype == TypeDesc::STRING) {
                    for (int i = 0; i < n; ++i)
                        p.sdefault.push_back(sym->get_string(i));
                }
            }
            if (StructSpec* ss = ts.structspec()) {
                p.structname = ss->name().string();
                for (size_t i = 0, e = ss->numfields(); i < e; ++i)
                    p.fields.push_back(ss->field(i).name);
            } else {
                p.structname.clear();
            }
            p.validdefault = (p.data != nullptr);
        }
    }

    m_meta.clear();  // no metadata available at this point

    return true;
}



// vals points to a symbol with a total of ncomps floats (ncomps ==
// aggregate*arraylen).  If has_derivs is true, it's actually 3 times
// that length, the main values then the derivatives.  We want to check
// for nans in vals[firstcheck..firstcheck+nchecks-1], and also in the
// derivatives if present.  Note that if firstcheck==0 and nchecks==ncomps,
// we are checking the entire contents of the symbol.  More restrictive
// firstcheck,nchecks are used to check just one element of an array.
OSL_SHADEOP void
osl_naninf_check(int ncomps, const void* vals_, int has_derivs, void* sg,
                 ustringhash_pod sourcefile_, int sourceline,
                 ustringhash_pod symbolname_, int firstcheck, int nchecks,
                 ustringhash_pod opname_)
{
    auto ec           = pvt::get_ec(sg);
    const float* vals = (const float*)vals_;
    for (int d = 0; d < (has_derivs ? 3 : 1); ++d) {
        for (int c = firstcheck, e = c + nchecks; c < e; ++c) {
            int i = d * ncomps + c;
            if (!std::isfinite(vals[i])) {
                OSL::errorfmt(ec, "Detected {} value in {}{} at {}:{} (op {})",
                              vals[i], d > 0 ? "the derivatives of " : "",
                              OSL::ustringhash_from(symbolname_),
                              OSL::ustringhash_from(sourcefile_), sourceline,
                              OSL::ustringhash_from(opname_));
                return;
            }
        }
    }
}



// vals points to the data of a float-, int-, or string-based symbol.
// (described by typedesc).  We want to check
// vals[firstcheck..firstcheck+nchecks-1] for floats that are NaN , or
// ints that are -MAXINT, or strings that are "!!!uninitialized!!!"
// which would indicate that the value is uninitialized if
// 'debug_uninit' is turned on.  Note that if firstcheck==0 and
// nchecks==ncomps, we are checking the entire contents of the symbol.
// More restrictive firstcheck,nchecks are used to check just one
// element of an array.
OSL_SHADEOP void
osl_uninit_check(long long typedesc_, void* vals_, void* sg,
                 ustringhash_pod sourcefile_, int sourceline,
                 ustringhash_pod groupname_, int layer,
                 ustringhash_pod layername_, ustringhash_pod shadername_,
                 int opnum, ustringhash_pod opname_, int argnum,
                 ustringhash_pod symbolname_, int firstcheck, int nchecks)
{
    TypeDesc typedesc = TYPEDESC(typedesc_);
    bool uninit       = false;
    if (typedesc.basetype == TypeDesc::FLOAT) {
        float* vals = (float*)vals_;
        for (int c = firstcheck, e = firstcheck + nchecks; c < e; ++c)
            if (!std::isfinite(vals[c])) {
                uninit  = true;
                vals[c] = 0;
            }
    }
    if (typedesc.basetype == TypeDesc::INT) {
        int* vals = (int*)vals_;
        for (int c = firstcheck, e = firstcheck + nchecks; c < e; ++c)
            if (vals[c] == std::numeric_limits<int>::min()) {
                uninit  = true;
                vals[c] = 0;
            }
    }
    if (typedesc.basetype == TypeDesc::STRING) {
        OSL::ustringhash* vals = (OSL::ustringhash*)vals_;
        for (int c = firstcheck, e = firstcheck + nchecks; c < e; ++c)
            if (vals[c] == Hashes::uninitialized_string) {
                uninit  = true;
                vals[c] = OSL::ustringhash();
            }
    }
    if (uninit) {
        auto groupname         = OSL::ustringhash_from(groupname_);
        auto layername         = OSL::ustringhash_from(layername_);
        OSL::ExecContextPtr ec = pvt::get_ec(sg);
        OSL::errorfmt(
            ec,
            "Detected possible use of uninitialized value in {} {} at {}:{} (group {}, layer {} {}, shader {}, op {} '{}', arg {})",
            typedesc, OSL::ustringhash_from(symbolname_),
            OSL::ustringhash_from(sourcefile_), sourceline,
            groupname.empty() ? OSL::ustringhash("<unnamed group>") : groupname,
            layer,
            layername.empty() ? OSL::ustringhash("<unnamed layer>") : layername,
            OSL::ustringhash_from(shadername_), opnum,
            OSL::ustringhash_from(opname_), argnum);
    }
}



OSL_SHADEOP int
osl_range_check_err(int indexvalue, int length, ustringhash_pod symname_,
                    void* sg, ustringhash_pod sourcefile_, int sourceline,
                    ustringhash_pod groupname_, int layer,
                    ustringhash_pod layername_, ustringhash_pod shadername_)
{
    auto ec = pvt::get_ec(sg);
    if (indexvalue < 0 || indexvalue >= length) {
        auto groupname = OSL::ustringhash_from(groupname_);
        auto layername = OSL::ustringhash_from(layername_);
        OSL::errorfmt(
            ec,
            "Index [{}] out of range {}[0..{}]: {}:{} (group {}, layer {} {}, shader {})",
            indexvalue, OSL::ustringhash_from(symname_), length - 1,
            OSL::ustringhash_from(sourcefile_), sourceline,
            groupname.empty() ? OSL::ustringhash("<unnamed group>") : groupname,
            layer,
            layername.empty() ? OSL::ustringhash("<unnamed layer>") : layername,
            OSL::ustringhash_from(shadername_));
        if (indexvalue >= length)
            indexvalue = length - 1;
        else
            indexvalue = 0;
    }
    return indexvalue;
}



// Asked if the raytype is a name we can't know until mid-shader.
OSL_SHADEOP int
osl_raytype_name(void* sg_, ustringhash_pod name_)
{
    ShaderGlobals* sg = (ShaderGlobals*)sg_;
    auto name         = ustring_from(name_);
    // TODO: add 2nd version of raytype_bit that takes ustringhash
    int bit = sg->context->shadingsys().raytype_bit(name);
    return (sg->raytype & bit) != 0;
}


OSL_SHADEOP int
osl_get_attribute(void* sg_, int dest_derivs, ustringhash_pod obj_name_,
                  ustringhash_pod attr_name_, int array_lookup, int index,
                  long long attr_type, void* attr_dest)
{
    ShaderGlobals* sg     = (ShaderGlobals*)sg_;
    ustringhash obj_name  = ustringhash_from(obj_name_);
    ustringhash attr_name = ustringhash_from(attr_name_);
    return sg->context->osl_get_attribute(sg, sg->objdata, dest_derivs,
                                          obj_name, attr_name, array_lookup,
                                          index, TYPEDESC(attr_type),
                                          attr_dest);
}



OSL_SHADEOP int
osl_bind_interpolated_param(void* sg_, ustringhash_pod name_, long long type,
                            int userdata_has_derivs, void* userdata_data,
                            int /*symbol_has_derivs*/, void* symbol_data,
                            int symbol_data_size, char* userdata_initialized,
                            int /*userdata_index*/)
{
    char status      = *userdata_initialized;
    ustringhash name = ustringhash_from(name_);
    if (status == 0) {
        // First time retrieving this userdata
        ShaderGlobals* sg = (ShaderGlobals*)sg_;
        bool ok = sg->renderer->get_userdata(userdata_has_derivs, name,
                                             TYPEDESC(type), sg, userdata_data);
        *userdata_initialized = status = 1 + ok;  // 1 = not found, 2 = found
        sg->context->incr_get_userdata_calls();
    }
    if (status == 2) {
        int udata_size = (userdata_has_derivs ? 3 : 1) * TYPEDESC(type).size();
        // If userdata was present, copy it to the shader variable
        if (TYPEDESC(type) == TypeDesc::STRING) {
            const ustringhash* uh_userdata
                = reinterpret_cast<const ustringhash*>(userdata_data);
            memcpy(symbol_data, uh_userdata,
                   std::min(symbol_data_size, udata_size));
        } else {  //not string
            memcpy(symbol_data, userdata_data,
                   std::min(symbol_data_size, udata_size));
        }
        if (symbol_data_size > udata_size)
            memset((char*)symbol_data + udata_size, 0,
                   symbol_data_size - udata_size);
        return 1;
    }
    return 0;  // no such user data
}



OSL_SHADEOP void
osl_incr_get_userdata_calls(void* sg_)
{
    ShaderGlobals* sg = (ShaderGlobals*)sg_;
    sg->context->incr_get_userdata_calls();
}
