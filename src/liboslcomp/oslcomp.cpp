// Copyright Contributors to the Open Shading Language project.
// SPDX-License-Identifier: BSD-3-Clause
// https://github.com/AcademySoftwareFoundation/OpenShadingLanguage


#include <cerrno>
#include <cstdio>
#include <fstream>
#include <streambuf>
#include <string>
#include <vector>

#include "oslcomp_pvt.h"

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/platform.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/thread.h>

#if !defined(__STDC_CONSTANT_MACROS)
#    define __STDC_CONSTANT_MACROS 1
#endif
#include <clang/Basic/TargetInfo.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Frontend/Utils.h>
#include <clang/Lex/PreprocessorOptions.h>
#if OSL_LLVM_VERSION < 160
#    include <llvm/Support/Host.h>
#else
#    include <llvm/TargetParser/Host.h>
#endif
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>


OSL_NAMESPACE_BEGIN


OSLCompiler::OSLCompiler(ErrorHandler* errhandler)
{
    m_impl = new pvt::OSLCompilerImpl(errhandler);
}



OSLCompiler::~OSLCompiler()
{
    delete m_impl;
}



bool
OSLCompiler::compile(string_view filename,
                     const std::vector<std::string>& options,
                     string_view stdoslpath)
{
    return m_impl->compile(filename, options, stdoslpath);
}



bool
OSLCompiler::compile_buffer(string_view sourcecode, std::string& osobuffer,
                            const std::vector<std::string>& options,
                            string_view stdoslpath, string_view filename)
{
    return m_impl->compile_buffer(sourcecode, osobuffer, options, stdoslpath,
                                  filename);
}



string_view
OSLCompiler::output_filename() const
{
    return m_impl->output_filename();
}



namespace pvt {  // OSL::pvt



OSLCompilerImpl::OSLCompilerImpl(ErrorHandler* errhandler)
    : m_errhandler(errhandler ? errhandler : &ErrorHandler::default_handler())
    , m_err(false)
    , m_symtab(*this)
    , m_current_typespec(TypeDesc::UNKNOWN)
    , m_current_output(false)
    , m_verbose(false)
    , m_quiet(false)
    , m_debug(false)
    , m_preprocess_only(false)
    , m_err_on_warning(false)
    , m_optimizelevel(1)
    , m_next_temp(0)
    , m_next_const(0)
    , m_osofile(NULL)
    , m_total_nesting(0)
    , m_loop_nesting(0)
    , m_derivsym(NULL)
    , m_main_method_start(-1)
    , m_declaring_shader_formals(false)
{
    initialize_globals();
    initialize_builtin_funcs();
}



OSLCompilerImpl::~OSLCompilerImpl()
{
    delete m_derivsym;
}



bool
OSLCompilerImpl::preprocess_file(const std::string& filename,
                                 const std::string& stdoslpath,
                                 const std::vector<std::string>& defines,
                                 const std::vector<std::string>& includepaths,
                                 std::string& result)
{
    // Read file contents into a string
    std::string instring;
    if (!OIIO::Filesystem::read_text_file(filename, instring)) {
        errorfmt(ustring(filename), 0, "Could not open \"{}\"\n", filename);
        return false;
    }
    return preprocess_buffer(instring, filename, stdoslpath, defines,
                             includepaths, result);
}



bool
OSLCompilerImpl::preprocess_buffer(const std::string& buffer,
                                   const std::string& filename,
                                   const std::string& stdoslpath,
                                   const std::vector<std::string>& defines,
                                   const std::vector<std::string>& includepaths,
                                   std::string& result)
{
    using OIIO::Strutil::fmt::format;
    std::string instring;
    if (!stdoslpath.empty()) {
        instring = format("#include \"{}\"\n",
                          OIIO::Strutil::escape_chars(stdoslpath));
        // Note: because we're turning this from a regular string into a
        // double-quoted string injected into the OSL parse stream, we need
        // to fully escape any backslashes used in Windows file paths. We
        // don't want "c:\path\to\new\osl" to be interpreted as
        // "c:\path<tab>o<newline>ew\osl" !
    } else {
        instring = "\n";
    }
    instring += buffer;
    std::unique_ptr<llvm::MemoryBuffer> mbuf(
        llvm::MemoryBuffer::getMemBuffer(instring, filename));

    clang::CompilerInstance inst;

    // Set up error capture for the preprocessor
    std::string preproc_errors;
    llvm::raw_string_ostream errstream(preproc_errors);
    clang::DiagnosticOptions* diagOptions = new clang::DiagnosticOptions();
    clang::TextDiagnosticPrinter* diagPrinter
        = new clang::TextDiagnosticPrinter(errstream, diagOptions);
    llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagIDs(
        new clang::DiagnosticIDs);
    clang::DiagnosticsEngine* diagEngine
        = new clang::DiagnosticsEngine(diagIDs, diagOptions, diagPrinter);
    inst.setDiagnostics(diagEngine);

    const std::shared_ptr<clang::TargetOptions> targetopts
        = std::make_shared<clang::TargetOptions>(inst.getTargetOpts());
    targetopts->Triple = llvm::sys::getDefaultTargetTriple();
    clang::TargetInfo* target
        = clang::TargetInfo::CreateTargetInfo(inst.getDiagnostics(),
                                              targetopts);

    inst.setTarget(target);

    inst.createFileManager();
    inst.createSourceManager(inst.getFileManager());
    clang::SourceManager& sm = inst.getSourceManager();
    sm.setMainFileID(sm.createFileID(std::move(mbuf), clang::SrcMgr::C_User));

    inst.getPreprocessorOutputOpts().ShowCPP               = 1;
    inst.getPreprocessorOutputOpts().ShowMacros            = 0;
    inst.getPreprocessorOutputOpts().ShowComments          = 0;
    inst.getPreprocessorOutputOpts().ShowLineMarkers       = 1;
    inst.getPreprocessorOutputOpts().ShowMacroComments     = 0;
    inst.getPreprocessorOutputOpts().ShowIncludeDirectives = 0;

    clang::HeaderSearchOptions& headerOpts = inst.getHeaderSearchOpts();
    headerOpts.UseBuiltinIncludes          = 0;
    headerOpts.UseStandardSystemIncludes   = 0;
    headerOpts.UseStandardCXXIncludes      = 0;
    // headerOpts.Verbose = 1;
    std::string directory = OIIO::Filesystem::parent_path(filename);
    if (directory.empty())
        directory = OIIO::Filesystem::current_path();
    headerOpts.AddPath(directory, clang::frontend::Angled, false, true);
    for (auto&& inc : includepaths) {
        headerOpts.AddPath(inc, clang::frontend::Angled,
                           false /* not a framework */,
                           true /* ignore sys root */);
    }

    clang::PreprocessorOptions& preprocOpts = inst.getPreprocessorOpts();
    preprocOpts.UsePredefines               = 0;
    preprocOpts.addMacroDef(
        format("OSL_VERSION_MAJOR={}", OSL_LIBRARY_VERSION_MAJOR));
    preprocOpts.addMacroDef(
        format("OSL_VERSION_MINOR={}", OSL_LIBRARY_VERSION_MINOR));
    preprocOpts.addMacroDef(
        format("OSL_VERSION_PATCH={}", OSL_LIBRARY_VERSION_PATCH));
    preprocOpts.addMacroDef(format("OSL_VERSION={}", OSL_LIBRARY_VERSION_CODE));
    for (auto&& d : defines) {
        if (d[1] == 'D')
            preprocOpts.addMacroDef(d.c_str() + 2);
        else if (d[1] == 'U')
            preprocOpts.addMacroUndef(d.c_str() + 2);
    }

    inst.getLangOpts().LineComment = 1;
    inst.createPreprocessor(clang::TU_Prefix);

    llvm::raw_string_ostream ostream(result);
    diagPrinter->BeginSourceFile(inst.getLangOpts(), &inst.getPreprocessor());
    clang::DoPrintPreprocessedInput(inst.getPreprocessor(), &ostream,
                                    inst.getPreprocessorOutputOpts());
    diagPrinter->EndSourceFile();

    if (preproc_errors.size()) {
        while (preproc_errors.size()
               && preproc_errors[preproc_errors.size() - 1] == '\n')
            preproc_errors.erase(preproc_errors.size() - 1);
        errorfmt(ustring(), -1, "{}", preproc_errors);
        return false;
    }
    return true;
}



void
OSLCompilerImpl::read_compile_options(const std::vector<std::string>& options,
                                      std::vector<std::string>& defines,
                                      std::vector<std::string>& includepaths)
{
    m_output_filename.clear();
    m_preprocess_only = false;
    for (size_t i = 0; i < options.size(); ++i) {
        if (options[i] == "-v") {
            // verbose mode
            m_verbose = true;
        } else if (options[i] == "-q") {
            // quiet mode
            m_quiet = true;
        } else if (options[i] == "-d") {
            // debug mode
            m_debug = true;
        } else if (options[i] == "-E") {
            m_preprocess_only = true;
        } else if (options[i] == "-o" && i < options.size() - 1) {
            ++i;
            m_output_filename = options[i];
        } else if (options[i] == "-O0") {
            m_optimizelevel = 0;
        } else if (options[i] == "-O" || options[i] == "-O1") {
            m_optimizelevel = 1;
        } else if (options[i] == "-O2") {
            m_optimizelevel = 2;
        } else if (options[i] == "-Werror") {
            m_err_on_warning = true;
        } else if (options[i] == "-embed-source"
                   || options[i] == "--embed-source") {
            m_embed_source = true;
        } else if (options[i] == "-MD"
                   || options[i] == "--write-dependencies") {
            // write depfile w/ user and system headers
            m_generate_deps        = true;
            m_generate_system_deps = true;
        } else if (options[i] == "-MMD"
                   || options[i] == "--write-user-dependencies") {
            // write depfile w/ user headers only
            m_generate_deps = true;
        } else if (options[i] == "-M" || options[i] == "--dependencies") {
            // write depfile w/ user headers, implies -E and write to stdout
            m_generate_deps        = true;
            m_preprocess_only      = true;
            m_generate_system_deps = true;
            if (m_deps_filename.empty())
                m_deps_filename = "stdout";
        } else if (options[i] == "-MM" || options[i] == "--user-dependencies") {
            // write depfile w/ user and system headers, implies -E and write to stdout
            m_generate_deps   = true;
            m_preprocess_only = true;
            if (m_deps_filename.empty())
                m_deps_filename = "stdout";
        } else if (options[i] == "-MF") {
            m_deps_filename = options[++i];
        } else if (OIIO::Strutil::starts_with(options[i], "-MF")) {
            m_deps_filename = options[i].substr(3);
        } else if (options[i] == "-MT") {
            m_deps_target = options[++i];
        } else if (OIIO::Strutil::starts_with(options[i], "-MT")) {
            m_deps_target = options[i].substr(3);
        } else if (options[i].c_str()[0] == '-' && options[i].size() > 2) {
            // options meant for the preprocessor
            if (options[i].c_str()[1] == 'D' || options[i].c_str()[1] == 'U')
                defines.push_back(options[i]);
            else if (options[i].c_str()[1] == 'I')
                includepaths.push_back(options[i].substr(2));
        }
    }
}



// Guess the path for stdosl.h. This is only called if no explicit
// stdoslpath is given to the compile command.
static string_view
find_stdoslpath(const std::vector<std::string>& includepaths)
{
    // Try the user-supplied include paths first
    for (auto& dir : includepaths) {
        std::string path = dir + "/stdosl.h";
        if (OIIO::Filesystem::exists(path))
            return ustring(path);
    }

    // If that doesn't work, look in a bunch of likely places.

    std::string install_loc
        = OIIO::Strutil::fmt::format("{}/stdosl.h", OSL_SHADER_INSTALL_DIR);
    if (OIIO::Filesystem::exists(install_loc))
        return ustring(install_loc);

    std::string myprefix = OIIO::Filesystem::parent_path(
        OIIO::Filesystem::parent_path(OIIO::Sysutil::this_program_path()));

    // List of likely prefixes and suffixes, in order of preference
    std::string prefixes[] = { OIIO::Sysutil::getenv("OSL_ROOT"),
                               OIIO::Sysutil::getenv("OSL_ROOT_DIR"),
                               OIIO::Sysutil::getenv("OSLHOME"), myprefix,
                               OIIO::Filesystem::parent_path(myprefix) };
    std::string suffixes[] = {
        "/share/OSL/shaders/stdosl.h", "/share/shaders/stdosl.h",
        "/lib/OSL/include/std/stdosl.h", "/lib/osl/include/std/stdosl.h",
        "/shaders/stdosl.h" /*old*/
    };
    for (auto& dir : prefixes) {
        if (!dir.empty() && OIIO::Filesystem::is_directory(dir)) {
            // For each prefix, check likely suffixes, in order of preference
            for (auto& s : suffixes) {
                if (OIIO::Filesystem::exists(dir + s))
                    return ustring(dir + s);
            }
        }
    }

    // Try looking for "oslc" binary in the $PATH (in case the executable
    // running this code is not the actual oslc), and if so, look in places
    // relative to that.
    std::vector<std::string> exec_path_dirs;
    OIIO::Filesystem::searchpath_split(OIIO::Sysutil::getenv("PATH"),
                                       exec_path_dirs, true);
#ifdef _WIN32
    std::string oslcbin = "oslc.exe";
#else
    std::string oslcbin = "oslc";
#endif
    oslcbin = OIIO::Filesystem::searchpath_find(oslcbin, exec_path_dirs);
    if (oslcbin.size()) {
        std::string dir = OIIO::Filesystem::parent_path(
            oslcbin);                              // the bin dir of our program
        dir = OIIO::Filesystem::parent_path(dir);  // now the parent dir
        if (!dir.empty() && OIIO::Filesystem::is_directory(dir)) {
            // For each prefix, check likely suffixes, in order of preference
            for (auto& s : suffixes) {
                if (OIIO::Filesystem::exists(dir + s))
                    return ustring(dir + s);
            }
        }
    }

    // Give up
    return string_view();
}



bool
OSLCompilerImpl::compile(string_view filename,
                         const std::vector<std::string>& options,
                         string_view stdoslpath)
{
    if (!OIIO::Filesystem::exists(filename)) {
        errorfmt(ustring(), 0, "Input file \"{}\" not found", filename);
        return false;
    }

    std::vector<std::string> defines;
    std::vector<std::string> includepaths;
    m_cwd           = OIIO::Filesystem::current_path();
    m_main_filename = ustring(filename);
    clear_filecontents_cache();

    read_compile_options(options, defines, includepaths);

    // Determine where the installed shader include directory is, and
    // look for ../shaders/stdosl.h and force it to include.
    if (stdoslpath.empty()) {
        stdoslpath = find_stdoslpath(includepaths);
    }
    if (stdoslpath.empty() || !OIIO::Filesystem::exists(stdoslpath))
        warningfmt(ustring(filename), 0, "Unable to find \"stdosl.h\"");
    else {
        // Add the directory of stdosl.h to the include paths
        includepaths.push_back(OIIO::Filesystem::parent_path(stdoslpath));
    }

    std::string preprocess_result;
    if (!preprocess_file(filename, stdoslpath, defines, includepaths,
                         preprocess_result)) {
        return false;
    }

    if (m_preprocess_only && !m_generate_deps) {
        std::cout << preprocess_result;
    } else {
        bool parseerr = osl_parse_buffer(preprocess_result);
        if (!parseerr) {
            if (shader())
                shader()->typecheck();
            else
                errorfmt(ustring(), 0, "No shader function defined");
        }

        // Print the parse tree if there were no errors
        if (m_debug) {
            symtab().print();
            if (shader())
                shader()->print(std::cout);
        }

        if (m_generate_deps)
            write_dependency_file(filename);

        if (!error_encountered()) {
            shader()->codegen();
            track_variable_dependencies();
            track_variable_lifetimes();
            check_for_illegal_writes();
            //            if (m_optimizelevel >= 1)
            //                coalesce_temporaries ();
        }

        if (!error_encountered()) {
            if (m_output_filename.size() == 0)
                m_output_filename = default_output_filename();

            OIIO::ofstream oso_output;
            OIIO::Filesystem::open(oso_output, m_output_filename);
            if (!oso_output.good()) {
                errorfmt(ustring(), 0, "Could not open \"{}\"",
                         m_output_filename);
                return false;
            }
            OSL_DASSERT(m_osofile == nullptr);
            m_osofile = &oso_output;

            write_oso_file(OIIO::Strutil::join(options, " "),
                           preprocess_result);
            OSL_DASSERT(m_osofile == nullptr);

            oso_output.close();
            if (!oso_output.good()) {
                errorfmt(ustring(), 0, "Failed to write to \"{}\"",
                         m_output_filename);
                return false;
            }
        }
    }

    return !error_encountered();
}



bool
OSLCompilerImpl::compile_buffer(string_view sourcecode, std::string& osobuffer,
                                const std::vector<std::string>& options,
                                string_view stdoslpath, string_view filename)
{
    if (filename.empty())
        filename = string_view("<buffer>");

    std::vector<std::string> defines;
    std::vector<std::string> includepaths;
    read_compile_options(options, defines, includepaths);

    m_cwd           = OIIO::Filesystem::current_path();
    m_main_filename = ustring(filename);
    clear_filecontents_cache();

    // Determine where the installed shader include directory is, and
    // look for ../shaders/stdosl.h and force it to include.
    if (stdoslpath.empty()) {
        stdoslpath = find_stdoslpath(includepaths);
    }
    if (stdoslpath.empty() || !OIIO::Filesystem::exists(stdoslpath))
        warningfmt(ustring(filename), 0, "Unable to find \"stdosl.h\"");

    std::string preprocess_result;
    if (!preprocess_buffer(sourcecode, filename, stdoslpath, defines,
                           includepaths, preprocess_result)) {
        return false;
    }

    if (m_preprocess_only) {
        std::cout << preprocess_result;
    } else {
        bool parseerr = osl_parse_buffer(preprocess_result);
        if (!parseerr) {
            if (shader())
                shader()->typecheck();
            else
                errorfmt(ustring(), 0, "No shader function defined");
        }

        // Print the parse tree if there were no errors
        if (m_debug) {
            symtab().print();
            if (shader())
                shader()->print(std::cout);
        }

        if (!error_encountered()) {
            shader()->codegen();
            track_variable_dependencies();
            track_variable_lifetimes();
            check_for_illegal_writes();
            //            if (m_optimizelevel >= 1)
            //                coalesce_temporaries ();
        }

        if (!error_encountered()) {
            if (m_output_filename.empty())
                m_output_filename = default_output_filename();

            std::ostringstream oso_output;
            oso_output.imbue(std::locale::classic());  // force C locale
            OSL_DASSERT(m_osofile == nullptr);
            m_osofile = &oso_output;

            write_oso_file(OIIO::Strutil::join(options, " "),
                           preprocess_result);
            osobuffer = oso_output.str();
            OSL_DASSERT(m_osofile == nullptr);
        }
    }

    return !error_encountered();
}



void
OSLCompilerImpl::write_dependency_file(string_view filename)
{
    if (m_deps_filename.empty())
        m_deps_filename = OIIO::Filesystem::replace_extension(filename, ".d");
    std::string target = m_deps_target;
    if (target.empty())
        target = m_output_filename.size()
                     ? m_output_filename
                     : OIIO::Filesystem::replace_extension(filename, ".oso");
    FILE* depfile = (m_deps_filename == "stdout"
                         ? stdout
                         : OIIO::Filesystem::fopen(m_deps_filename, "w"));
    if (depfile) {
        OSL::print(depfile, "{}: {}", target, filename);
        for (ustring dep : m_file_dependencies) {
            if (OIIO::Strutil::ends_with(dep, "stdosl.h")
                && !m_generate_system_deps)
                continue;  // skip system headers if so instructed
            if (OIIO::Strutil::starts_with(dep, "<"))
                continue;  // skip pseudo files
            if (dep == filename)
                continue;  // skip this file, since we already put it first
#if defined(_WIN32) && defined(OSL_CI)
            // Special behavior for CI on Windows: remove the directory paths
            // for the dependencies, or else it won't match the reference
            // output.
            using OIIO::Strutil::replace;
            std::string cwd = replace(OIIO::Filesystem::current_path() + "/",
                                      "/", "\\", true);
            auto d          = replace(dep, "\\\\", "\\", true);
            dep             = ustring(replace(d, cwd, ""));
#endif
            OSL::print(depfile, " \\\n  {}", dep);
        }
        OSL::print(depfile, "\n");
        if (depfile != stdout)
            fclose(depfile);
    } else {
        errorfmt(ustring(), 0,
                 "Could not open dependency file '{}' for writing",
                 m_deps_filename);
    }
}



struct GlobalTable {
    const char* name;
    TypeSpec type;
    bool readonly;

    GlobalTable(const char* name, TypeSpec type, bool readonly = true)
        : name(name), type(type), readonly(readonly)
    {
    }
};


void
OSLCompilerImpl::initialize_globals()
{
    static GlobalTable globals[] = {
        { "P", TypePoint, false },  { "I", TypeVector, false },
        { "N", TypeNormal, false }, { "Ng", TypeNormal },
        { "u", TypeFloat },         { "v", TypeFloat },
        { "dPdu", TypeVector },     { "dPdv", TypeVector },
        { "Ps", TypePoint },        { "Ci", TypeSpec(TypeColor, true), false },
        { "time", TypeFloat },      { "dtime", TypeFloat },
        { "dPdtime", TypeVector }
    };

    for (auto& g : globals) {
        Symbol* s = new Symbol(ustring(g.name), g.type, SymTypeGlobal);
        s->readonly(g.readonly);
        symtab().insert(s);
    }
}



std::string
OSLCompilerImpl::default_output_filename()
{
    if (m_shader && shader_decl())
        return shader_decl()->shadername().string() + ".oso";
    return std::string();
}



void
OSLCompilerImpl::write_oso_metadata(const ASTNode* metanode) const
{
    OSL_DASSERT(metanode->nodetype() == ASTNode::variable_declaration_node);
    const ASTvariable_declaration* metavar
        = static_cast<const ASTvariable_declaration*>(metanode);
    Symbol* metasym = metavar->sym();
    OSL_DASSERT(metasym);
    TypeSpec ts = metasym->typespec();
    std::string pdl;
    bool ok = metavar->param_default_literals(metasym, metavar->init().get(),
                                              pdl, ",");
    if (ok) {
        osofmt("%meta{{{},{},{}}} ", ts, metasym->name(), pdl);
    } else {
        errorfmt(metanode->sourcefile(), metanode->sourceline(),
                 "Don't know how to print metadata {} ({}) with node type {}",
                 metasym->name(), ts, metavar->init()->nodetypename());
    }
}



void
OSLCompilerImpl::write_oso_const_value(const ConstantSymbol* sym) const
{
    OSL_ASSERT(sym);
    TypeDesc type     = sym->typespec().simpletype();
    TypeDesc elemtype = type.elementtype();
    int nelements     = std::max(1, type.arraylen);
    if (elemtype == TypeDesc::STRING)
        for (int i = 0; i < nelements; ++i)
            osofmt("\"{}\"{}", OIIO::Strutil::escape_chars(sym->get_string(i)),
                   nelements > 1 ? " " : "");
    else if (elemtype == TypeDesc::INT)
        for (int i = 0; i < nelements; ++i)
            osofmt("{}{}", sym->get_int(i), nelements > 1 ? " " : "");
    else if (elemtype == TypeDesc::FLOAT)
        for (int i = 0; i < nelements; ++i)
            osofmt("{:.9g}{}", sym->floatval(i), nelements > 1 ? " " : "");
    else if (equivalent(elemtype, TypeVector))
        for (int i = 0; i < nelements; ++i) {
            Vec3 v = sym->get_vec3(i);
            osofmt("{:.9g} {:.9g} {:.9g}{}", v.x, v.y, v.z,
                   nelements > 1 ? " " : "");
        }
    else {
        OSL_ASSERT(0 && "Don't know how to output this constant type");
    }
}



void
OSLCompilerImpl::write_oso_symbol(const Symbol* sym)
{
    // symtype / datatype / name
    osofmt("{}\t{}\t{}", sym->symtype_shortname(), sym->typespec(),
           sym->mangled());

    ASTvariable_declaration* v = NULL;
    if (sym->node()
        && sym->node()->nodetype() == ASTNode::variable_declaration_node)
        v = static_cast<ASTvariable_declaration*>(sym->node());

    // Print default values
    bool isparam = (sym->symtype() == SymTypeParam
                    || sym->symtype() == SymTypeOutputParam);
    if (sym->symtype() == SymTypeConst) {
        osofmt("\t");
        write_oso_const_value(static_cast<const ConstantSymbol*>(sym));
        osofmt("\t");
    } else if (v && isparam) {
        std::string out;
        v->param_default_literals(sym, v->init().get(), out);
        osofmt("\t{}\t", out);
    }

    //
    // Now output all the hints, which is most of the work!
    //

    int hints = 0;

    // %meta{} encodes metadata (handled by write_oso_metadata)
    if (v) {
        for (ASTNode::ref m = v->meta(); m; m = m->next()) {
            if (hints++ == 0)
                osofmt("\t");
            write_oso_metadata(m.get());
        }
    }

    // %read and %write give the range of ops over which a symbol is used.
    osofmt("{}%read{{{},{}}} %write{{{},{}}}", hints++ ? ' ' : '\t',
           sym->firstread(), sym->lastread(), sym->firstwrite(),
           sym->lastwrite());

    // %struct, %structfields, and %structfieldtypes document the
    // definition of a structure and which other symbols comprise the
    // individual fields.
    if (sym->typespec().is_structure()) {
        const StructSpec* structspec(sym->typespec().structspec());
        std::string fieldlist, signature;
        for (int i = 0; i < (int)structspec->numfields(); ++i) {
            if (i > 0)
                fieldlist += ",";
            fieldlist += structspec->field(i).name.string();
            signature += structspec->field(i).type.code_from_type();
        }
        osofmt(
            "{}%struct{{\"{}\"}} %structfields{{{}}} %structfieldtypes{{\"{}\"}} %structnfields{{{}}}",
            hints++ ? ' ' : '\t', structspec->mangled(), fieldlist, signature,
            structspec->numfields());
    }
    // %mystruct and %mystructfield document the symbols holding structure
    // fields, linking them back to the structures they are part of.
    if (sym->fieldid() >= 0) {
        ASTvariable_declaration* vd = (ASTvariable_declaration*)sym->node();
        if (vd)
            osofmt("{}%mystruct{{{}}} %mystructfield{{{}}}",
                   hints++ ? ' ' : '\t', vd->sym()->mangled(), sym->fieldid());
    }

    // %derivs hint marks symbols that need to carry derivatives
    if (sym->has_derivs())
        osofmt("{}%derivs", hints++ ? ' ' : '\t');

    // %initexpr hint marks parameters whose default is the result of code
    // that must be executed (an expression, like =noise(P) or =u), rather
    // than a true default value that is statically known (like =3.14).
    if (isparam && sym->has_init_ops())
        osofmt("{}%initexpr", hints++ ? ' ' : '\t');

#if 0  // this is recomputed by the runtime optimizer, no need to bloat the .oso with these

    // %depends marks, for potential OUTPUTs, which symbols they depend
    // upon.  This is so that derivativeness, etc., may be
    // back-propagated as shader networks are linked together.
    if (isparam || sym->symtype() == SymTypeGlobal) {
        // FIXME
        const SymPtrSet &deps (m_symdeps[sym]);
        std::vector<const Symbol *> inputdeps;
        for (auto&& d : deps)
            if (d->symtype() == SymTypeParam ||
                  d->symtype() == SymTypeOutputParam ||
                  d->symtype() == SymTypeGlobal ||
                  d->symtype() == SymTypeLocal ||
                  d->symtype() == SymTypeTemp)
                inputdeps.push_back (d);
        if (inputdeps.size()) {
            if (hints++ == 0)
                osofmt("\t");
            osofmt(" %depends{{");
            int deps = 0;
            for (size_t i = 0;  i < inputdeps.size();  ++i) {
                if (inputdeps[i]->symtype() == SymTypeTemp &&
                    inputdeps[i]->dealias() != inputdeps[i])
                    continue;   // Skip aliased temporaries
                if (deps++)
                    osofmt(",");
                osofmt("{}",  inputdeps[i]->mangled());
            }
            osofmt("}}");
        }
    }
#endif
    osofmt("\n");
}



void
OSLCompilerImpl::write_oso_file(string_view options,
                                string_view preprocessed_source)
{
    OSL_DASSERT(m_osofile && m_osofile->good());
    osofmt("OpenShadingLanguage {}.{:02d}\n", OSO_FILE_VERSION_MAJOR,
           OSO_FILE_VERSION_MINOR);
    osofmt("# Compiled by oslc {}\n", OSL_LIBRARY_VERSION_STRING);
    osofmt("# options: {}\n", options);

    ASTshader_declaration* shaderdecl = shader_decl();
    osofmt("{} {}", shaderdecl->shadertypename(), shaderdecl->shadername());

    // output global hints and metadata
    int hints = 0;
    for (ASTNode::ref m = shaderdecl->metadata(); m; m = m->next()) {
        if (hints++ == 0)
            osofmt("\t");
        write_oso_metadata(m.get());
    }

    osofmt("\n");

    // Output params, so they are first
    for (auto&& s : symtab()) {
        if (s->symtype() == SymTypeParam || s->symtype() == SymTypeOutputParam)
            write_oso_symbol(s);
    }
    // Output globals, locals, temps, const
    for (auto&& s : symtab()) {
        if (s->symtype() == SymTypeLocal || s->symtype() == SymTypeTemp
            || s->symtype() == SymTypeGlobal || s->symtype() == SymTypeConst) {
            // Don't bother writing symbols that are never used
            if (s->lastuse() >= 0) {
                write_oso_symbol(s);
            }
        }
    }

    // Output all opcodes
    int lastline = -1;
    ustring lastfile;
    ustring lastmethod("___uninitialized___");
    for (auto& op : m_ircode) {
        if (lastmethod != op.method()) {
            osofmt("code {}\n", op.method());
            lastmethod = op.method();
            lastfile   = ustring();
            lastline   = -1;
        }

        if (/*m_debug &&*/ !op.sourcefile().empty()) {
            ustring file = op.sourcefile();
            int line     = op.sourceline();
            if (file != lastfile || line != lastline)
                osofmt("# {}:{}\n# {}\n", file, line,
                       retrieve_source(file, line));
        }

        // Op name
        osofmt("\t{}", op.opname());

        // Register arguments
        if (op.nargs())
            osofmt(op.opname().length() < 8 ? "\t\t" : "\t");
        for (int i = 0; i < op.nargs(); ++i) {
            int arg = op.firstarg() + i;
            osofmt("{} ", m_opargs[arg]->dealias()->mangled());
        }

        // Jump targets
        for (size_t i = 0; i < Opcode::max_jumps; ++i)
            if (op.jump(i) >= 0)
                osofmt("{} ", op.jump(i));

        //
        // Opcode Hints
        //

        bool firsthint = true;

        // %filename and %line document the source code file and line that
        // contained code that generated this op.  To avoid clutter, we
        // only output these hints when they DIFFER from the previous op.
        if (!op.sourcefile().empty()) {
            if (op.sourcefile() != lastfile) {
                lastfile = op.sourcefile();
                osofmt("{}%filename{{\"{}\"}}", firsthint ? '\t' : ' ',
                       lastfile);
                firsthint = false;
            }
            if (op.sourceline() != lastline) {
                lastline = op.sourceline();
                osofmt("{}%line{{{}}}", firsthint ? '\t' : ' ', lastline);
                firsthint = false;
            }
        }

        // %argrw documents which arguments are read, written, or both (rwW).
        if (op.nargs()) {
            osofmt("{}%argrw{{\"", firsthint ? '\t' : ' ');
            for (int i = 0; i < op.nargs(); ++i) {
                if (op.argwrite(i))
                    osofmt(op.argread(i) ? "W" : "w");
                else
                    osofmt(op.argread(i) ? "r" : "-");
            }
            osofmt("\"}}");
            firsthint = false;
        }

        // %argderivs documents which arguments have derivs taken of
        // them by the op.
        if (op.argtakesderivs_all()) {
            osofmt(" %argderivs{{");
            int any = 0;
            for (int i = 0; i < op.nargs(); ++i)
                if (op.argtakesderivs(i)) {
                    if (any++)
                        osofmt(",");
                    osofmt("{}", i);
                }
            osofmt("}}");
            firsthint = false;
        }

        osofmt("\n");
    }

    // If no code, still need a code marker
    if (lastmethod != main_method_name())
        osofmt("code {}\n", main_method_name());

    osofmt("\tend\n");

    if (m_embed_source)
        osofmt("%preprocessed_source\n{}\n", preprocessed_source);

    m_osofile = NULL;
}



void
OSLCompilerImpl::clear_filecontents_cache()
{
    m_filecontents_map.clear();
    m_last_sourcefile.clear();
    m_last_filecontents      = nullptr;
    m_last_sourceline        = 1;  // note we call the first line "1" for users
    m_last_sourceline_offset = 0;
}



string_view
OSLCompilerImpl::retrieve_source(ustring filename, int line)
{
    // If we don't have a valid "last", look it up in the cache.
    if (filename != m_last_sourcefile || !m_last_filecontents) {
        m_last_sourceline        = 1;
        m_last_sourceline_offset = 0;
        auto found               = m_filecontents_map.find(filename);
        if (found == m_filecontents_map.end()) {
            // If it wasn't in the cache, read the file and add it.
            std::string contents;
            bool ok = OIIO::Filesystem::read_text_file(filename, contents);
            if (ok) {
                m_last_sourcefile            = filename;
                m_filecontents_map[filename] = std::move(contents);
                m_last_filecontents          = &m_filecontents_map[filename];
            } else {
                m_last_sourcefile   = ustring();
                m_last_filecontents = nullptr;
                return "<file not found>";
            }
        } else {
            m_last_sourcefile   = filename;
            m_last_filecontents = &found->second;
        }
    }

    // Now read lines up to and including the file we want.
    OIIO::string_view s(*m_last_filecontents);
    int orig_sourceline = line;
    if (line >= m_last_sourceline) {
        // Shortcut: the line we want is in the same file as the last read,
        // and at least as far in the file. Start the search from where we
        // left off last time.
        s.remove_prefix(m_last_sourceline_offset);
        line -= m_last_sourceline - 1;
    } else {
        // If we have to backtrack at all, backtrack to the file start.
        m_last_sourceline_offset = 0;
        m_last_sourceline        = 1;
    }
    size_t offset = m_last_sourceline_offset;
    for (; line > 1; --line) {
        size_t p = s.find_first_of('\n');
        if (p == OIIO::string_view::npos)
            return "<line not found>";
        s.remove_prefix(p + 1);
        offset += p + 1;
    }
    s                        = s.substr(0, s.find_first_of('\n'));
    m_last_sourceline_offset = offset;
    m_last_sourceline        = orig_sourceline;
    return s;
}



void
OSLCompilerImpl::push_nesting(bool isloop)
{
    ++m_total_nesting;
    if (isloop)
        ++m_loop_nesting;
    if (current_function())
        current_function()->push_nesting(isloop);
}



void
OSLCompilerImpl::pop_nesting(bool isloop)
{
    --m_total_nesting;
    if (isloop)
        --m_loop_nesting;
    if (current_function())
        current_function()->pop_nesting(isloop);
}


void
OSLCompilerImpl::struct_field_pair(Symbol* sym1, Symbol* sym2, int fieldnum,
                                   Symbol*& field1, Symbol*& field2)
{
    OSL_DASSERT(sym1 && sym2 && sym1->typespec().is_structure()
                && sym1->typespec().structure()
                && sym2->typespec().structure());
    // Find the StructSpec for the type of struct that the symbols are
    StructSpec* structspec(sym1->typespec().structspec());
    OSL_DASSERT(structspec && fieldnum < (int)structspec->numfields());
    // Find the FieldSpec for the field we are interested in
    const StructSpec::FieldSpec& field(structspec->field(fieldnum));
    // Construct mangled names that describe the symbols for the
    // individual fields
    ustring name1 = ustring::fmtformat("{}.{}", sym1->mangled(), field.name);
    ustring name2 = ustring::fmtformat("{}.{}", sym2->mangled(), field.name);
    // Retrieve the symbols
    field1 = symtab().find_exact(name1);
    field2 = symtab().find_exact(name2);
    OSL_DASSERT(field1 && field2);
}



void
OSLCompilerImpl::struct_field_pair(const StructSpec* structspec, int fieldnum,
                                   ustring sym1, ustring sym2, Symbol*& field1,
                                   Symbol*& field2)
{
    // Find the FieldSpec for the field we are interested in
    const StructSpec::FieldSpec& field(structspec->field(fieldnum));
    ustring name1 = ustring::fmtformat("{}.{}", sym1, field.name);
    ustring name2 = ustring::fmtformat("{}.{}", sym2, field.name);
    // Retrieve the symbols
    field1 = symtab().find_exact(name1);
    field2 = symtab().find_exact(name2);
    OSL_ASSERT(field1 && field2);
}



/// Verify that the given symbol (written by the given op) is legal to be
/// written. If writeable, it's writeable, We don't try to enforce
/// differences in policy per shader types.
void
OSLCompilerImpl::check_write_legality(const Opcode& op, int opnum,
                                      const Symbol* sym)
{
    // We can never write to constant symbols
    if (sym->symtype() == SymTypeConst) {
        errorfmt(op.sourcefile(), op.sourceline(),
                 "Attempted to write to a constant value");
    }

    // Params can only write if it's part of their initialization
    if (sym->symtype() == SymTypeParam
        && (opnum < sym->initbegin() || opnum >= sym->initend())) {
        errorfmt(op.sourcefile(), op.sourceline(),
                 "cannot write to non-output parameter \"{}\"", sym->name());
    }
}



void
OSLCompilerImpl::check_for_illegal_writes()
{
    // For each op, make sure any arguments it writes are legal to do so
    int opnum = 0;
    for (auto&& op : m_ircode) {
        for (int a = 0; a < op.nargs(); ++a) {
            SymbolPtr s = m_opargs[op.firstarg() + a];
            if (op.argwrite(a))
                check_write_legality(op, opnum, s);
        }
        ++opnum;
    }
}



// This has O(n^2) memory usage, so only for debugging
//#define DEBUG_SYMBOL_DEPENDENCIES

// Add to the dependency map that "A depends on B".
static void
add_dependency(SymDependencyMap& dmap, const Symbol* A, const Symbol* B)
{
    dmap[A].insert(B);

#ifdef DEBUG_SYMBOL_DEPENDENCIES
    // Perform unification -- all of B's dependencies are now
    // dependencies of A.
    for (auto&& r : dmap[B])
        dmap[A].insert(r);
#endif
}


static void
mark_symbol_derivatives(SymDependencyMap& dmap, SymPtrSet& visited,
                        const Symbol* sym)
{
    for (auto&& r : dmap[sym]) {
        if (visited.find(r) == visited.end()) {
            visited.insert(r);

            const_cast<Symbol*>(r)->has_derivs(true);

            mark_symbol_derivatives(dmap, visited, r);
        }
    }
}


/// Run through all the ops, for each one marking its 'written'
/// arguments as dependent upon its 'read' arguments (and performing
/// unification as we go), yielding a dependency map that lets us look
/// up any symbol and see the set of other symbols on which it ever
/// depends on during execution of the shader.
void
OSLCompilerImpl::track_variable_dependencies()
{
    // It's important to note that this is simplistically conservative
    // in that it overestimates dependencies.  To see why this is the
    // case, consider the following code:
    //       // inputs a,b; outputs x,y; local variable t
    //       t = a;
    //       x = t;
    //       t = b;
    //       y = t;
    // We can see that x depends on a and y depends on b.  But the
    // dependency analysis we do below thinks that y also depends on a
    // (because t depended on both a and b, but at different times).
    //
    // This naivite will never miss a dependency, but it may
    // overestimate dependencies.  (Hence we call this "conservative"
    // rather than "wrong.")  We deem this acceptable for now, since
    // it's so much easier to implement the conservative dependency
    // analysis, and it's not yet clear that getting it closer to
    // optimal will have any performance impact on final shaders. Also
    // because this is probably no worse than the "dependency slop" that
    // would happen with loops and conditionals.  But we certainly may
    // revisit with a more sophisticated algorithm if this crops up
    // a legitimate issue.
    //
    // Because of this conservative approach, it is critical that this
    // analysis is done BEFORE temporaries are coalesced (which would
    // cause them to be reassigned in exactly the way that confuses this
    // analysis).

    m_symdeps.clear();
    std::vector<Symbol*> read, written;
    int opnum = 0;
    // We define a pseudo-symbol just for tracking derivatives.  This
    // symbol "depends on" whatever things have derivs taken of them.
    if (!m_derivsym)
        m_derivsym = new Symbol(ustring("$derivs"), TypeSpec(), SymTypeGlobal);
    // Loop over all ops...
    for (OpcodeVec::const_iterator op = m_ircode.begin(); op != m_ircode.end();
         ++op, ++opnum) {
        // Gather the list of syms read and written by the op.  Reuse the
        // vectors defined outside the loop to cut down on malloc/free.
        read.clear();
        written.clear();
        syms_used_in_op_range(op, op + 1, &read, &written);

        // FIXME -- special cases here!  like if any ops implicitly read
        // or write to globals without them needing to be arguments.

        bool deriv = op->argtakesderivs_all();
        // For each sym written by the op...
        for (auto&& wsym : written) {
            // For each sym read by the op...
            for (auto&& rsym : read) {
                if (rsym->symtype() != SymTypeConst)
                    add_dependency(m_symdeps, wsym, rsym);
            }
            if (deriv) {
                // If the op takes derivs, make the pseudo-symbol m_derivsym
                // depend on those arguments.
                for (int a = 0; a < op->nargs(); ++a)
                    if (op->argtakesderivs(a))
                        add_dependency(m_symdeps, m_derivsym,
                                       m_opargs[a + op->firstarg()]);
            }
        }
    }

    // Recursively tag all symbols that need derivatives
    SymPtrSet visited;
    mark_symbol_derivatives(m_symdeps, visited, m_derivsym);

#ifdef DEBUG_SYMBOL_DEPENDENCIES
    // Helpful for debugging

    std::cerr << "track_variable_dependencies\n";
    std::cerr << "\nDependencies:\n";
    for (auto &&m, m_symdeps) {
        std::cerr << m.first->mangled() << " depends on ";
        for (auto&& d : m.second)
            std::cerr << d->mangled() << ' ';
        std::cerr << "\n";
    }
    std::cerr << "\n\n";

    // Invert the dependency
    SymDependencyMap influences;
    for (auto &&m, m_symdeps)
        for (auto&& d : m.second)
            influences[d].insert(m.first);

    std::cerr << "\nReverse dependencies:\n";
    for (auto &&m, influences) {
        std::cerr << m.first->mangled() << " contributes to ";
        for (auto&& d : m.second)
            std::cerr << d->mangled() << ' ';
        std::cerr << "\n";
    }
    std::cerr << "\n\n";
#endif
}



// Is the symbol coalescable?
inline bool
coalescable(const Symbol* s)
{
    return (s->symtype() == SymTypeTemp &&    // only coalesce temporaries
            s->everused() &&                  // only if they're used
            s->dealias() == s &&              // only if not already aliased
            !s->typespec().is_structure() &&  // only if not a struct
            s->fieldid() < 0);                //    or a struct field
}



/// Coalesce temporaries.  During code generation, we make a new
/// temporary EVERY time we need one.  Now we examine them all and merge
/// ones of identical type and non-overlapping lifetimes.
void
OSLCompilerImpl::coalesce_temporaries(SymbolPtrVec& symtab)
{
    // We keep looping until we can't coalesce any more.
    int ncoalesced = 1;
    while (ncoalesced) {
        ncoalesced = 0;  // assume we're done, unless we coalesce something

        // We use a greedy algorithm that loops over each symbol, and
        // then examines all higher-numbered symbols (in order) and
        // tries to merge the first one it can find that doesn't overlap
        // lifetimes.  The temps were created as we generated code, so
        // they are already sorted by their "first use".  Thus, for any
        // pair t1 and t2 that are merged, it is guaranteed that t2 is
        // the symbol whose first use the earliest of all symbols whose
        // lifetimes do not overlap t1.

        SymbolPtrVec::iterator s;
        for (s = symtab.begin(); s != symtab.end(); ++s) {
            // Skip syms that can't be (or don't need to be) coalesced
            if (!coalescable(*s))
                continue;

            int sfirst = (*s)->firstuse();
            int slast  = (*s)->lastuse();

            // Loop through every other symbol
            for (SymbolPtrVec::iterator t = s + 1; t != symtab.end(); ++t) {
                // Coalesce s and t if both syms are coalescable,
                // equivalent types, and have nonoverlapping lifetimes.
                if (coalescable(*t)
                    && equivalent((*s)->typespec(), (*t)->typespec())
                    && (slast < (*t)->firstuse() || sfirst > (*t)->lastuse())) {
                    // Make all future t references alias to s
                    (*t)->alias(*s);
                    // s gets union of the lifetimes
                    (*s)->union_rw((*t)->firstread(), (*t)->lastread(),
                                   (*t)->firstwrite(), (*t)->lastwrite());
                    sfirst = (*s)->firstuse();
                    slast  = (*s)->lastuse();
                    // t gets marked as unused
                    (*t)->clear_rw();
                    ++ncoalesced;
                }
            }
        }
        // std::cerr << "Coalesced " << ncoalesced << "\n";
    }
}



bool
OSLCompilerImpl::op_uses_sym(const Opcode& op, const Symbol* sym, bool read,
                             bool write)
{
    // Loop through all the op's arguments, see if one matches sym
    for (int i = 0; i < op.nargs(); ++i)
        if (m_opargs[i + op.firstarg()] == sym
            && ((read && op.argread(i)) || (write && op.argwrite(i))))
            return true;
    return false;
}



void
OSLCompilerImpl::syms_used_in_op_range(OpcodeVec::const_iterator opbegin,
                                       OpcodeVec::const_iterator opend,
                                       std::vector<Symbol*>* rsyms,
                                       std::vector<Symbol*>* wsyms)
{
    for (OpcodeVec::const_iterator op = opbegin; op != opend; ++op) {
        for (int i = 0; i < op->nargs(); ++i) {
            Symbol* s = m_opargs[i + op->firstarg()];
            if (rsyms && op->argread(i))
                if (std::find(rsyms->begin(), rsyms->end(), s) == rsyms->end())
                    rsyms->push_back(s);
            if (wsyms && op->argwrite(i))
                if (std::find(wsyms->begin(), wsyms->end(), s) == wsyms->end())
                    wsyms->push_back(s);
        }
    }
}



};  // namespace pvt

OSL_NAMESPACE_END
