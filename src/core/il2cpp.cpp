#include "text_util.h"
#include "il2cpp.h"
#include "log.h"
#include <cstring>
#include <cwchar>

namespace {

HMODULE g_game = nullptr;

// --- exported C API we use --------------------------------------------------
using fn_domain_get            = il2cpp::Domain*  (*)();
using fn_domain_get_assemblies = il2cpp::Assembly** (*)(const il2cpp::Domain*, size_t*);
using fn_assembly_get_image    = il2cpp::Image*   (*)(const il2cpp::Assembly*);
using fn_image_get_name        = const char*      (*)(const il2cpp::Image*);
using fn_class_from_name       = il2cpp::Class*   (*)(const il2cpp::Image*, const char*, const char*);
using fn_class_get_nested      = il2cpp::Class*   (*)(il2cpp::Class*, void**);
using fn_class_get_method      = const il2cpp::Method* (*)(il2cpp::Class*, const char*, int);
using fn_class_get_field       = il2cpp::Field*   (*)(il2cpp::Class*, const char*);
using fn_class_get_name        = const char*      (*)(il2cpp::Class*);
using fn_field_get_offset      = size_t           (*)(il2cpp::Field*);
using fn_object_get_class      = il2cpp::Class*   (*)(il2cpp::Object*);
using fn_string_chars          = uint16_t*        (*)(il2cpp::String*);
using fn_string_length         = int32_t          (*)(il2cpp::String*);
using fn_thread_attach         = il2cpp::Thread*  (*)(il2cpp::Domain*);
using fn_thread_detach         = void             (*)(il2cpp::Thread*);
using fn_thread_current        = il2cpp::Thread*  (*)();
using fn_class_get_type        = void*            (*)(il2cpp::Class*);
using fn_type_get_object       = void*            (*)(void*);
using fn_runtime_invoke        = void*            (*)(const il2cpp::Method*, void*, void**, void**);
using fn_string_new_utf16      = il2cpp::String*  (*)(const wchar_t*, int32_t);
using fn_field_static_get      = void             (*)(il2cpp::Field*, void*);
using fn_object_get_virtual    = const il2cpp::Method* (*)(il2cpp::Object*, const il2cpp::Method*);

using fn_class_get_methods = const il2cpp::Method* (*)(il2cpp::Class*, void**);
using fn_method_get_name = const char* (*)(const il2cpp::Method*);
using fn_method_get_param_count = uint32_t (*)(const il2cpp::Method*);
using fn_method_get_param = void* (*)(const il2cpp::Method*, uint32_t);
using fn_class_from_type = il2cpp::Class* (*)(void*);
using fn_class_get_parent = il2cpp::Class* (*)(il2cpp::Class*);
using fn_class_get_namespace = const char* (*)(il2cpp::Class*);
using fn_class_get_fields = il2cpp::Field* (*)(il2cpp::Class*, void**);
using fn_field_get_name = const char* (*)(il2cpp::Field*);
fn_class_get_methods p_class_get_methods=nullptr;
fn_method_get_name p_method_get_name=nullptr;
fn_method_get_param_count p_method_get_param_count=nullptr;
fn_method_get_param p_method_get_param=nullptr;
fn_class_from_type p_class_from_type=nullptr;
fn_class_get_parent p_class_get_parent=nullptr;
fn_class_get_namespace p_class_get_namespace=nullptr;
fn_class_get_fields p_class_get_fields=nullptr;
fn_field_get_name p_field_get_name=nullptr;

fn_domain_get            p_domain_get            = nullptr;
fn_domain_get_assemblies p_domain_get_assemblies = nullptr;
fn_assembly_get_image    p_assembly_get_image    = nullptr;
fn_image_get_name        p_image_get_name        = nullptr;
fn_class_from_name       p_class_from_name       = nullptr;
fn_class_get_nested      p_class_get_nested      = nullptr;
fn_class_get_method      p_class_get_method      = nullptr;
fn_class_get_field       p_class_get_field       = nullptr;
fn_class_get_name        p_class_get_name        = nullptr;
fn_field_get_offset      p_field_get_offset      = nullptr;
fn_object_get_class      p_object_get_class      = nullptr;
fn_string_chars          p_string_chars          = nullptr;
fn_string_length         p_string_length         = nullptr;
fn_thread_attach         p_thread_attach         = nullptr;
fn_thread_detach         p_thread_detach         = nullptr;
fn_thread_current        p_thread_current        = nullptr;
fn_class_get_type        p_class_get_type        = nullptr;
fn_type_get_object       p_type_get_object       = nullptr;
fn_runtime_invoke        p_runtime_invoke        = nullptr;
fn_string_new_utf16      p_string_new_utf16      = nullptr;   // optional (see bind)
fn_field_static_get      p_field_static_get      = nullptr;   // optional
fn_object_get_virtual    p_object_get_virtual    = nullptr;   // optional

bool g_bound = false;
bool g_probe_faulted = false;   // log the first early-call fault only, never per poll

template <typename T>
bool grab(T& dst, const char* name)
{
    dst = reinterpret_cast<T>(GetProcAddress(g_game, name));
    if (!dst) { LOGF("il2cpp: MISSING export %s", name); return false; }
    return true;
}

/// The MethodInfo layout is version-dependent, but `methodPointer` has been the FIRST
/// field for every IL2CPP release in use (Unity 5.3 .. 6000.x). We read it directly
/// rather than depending on a generated header.
struct MethodInfoHead { void* methodPointer; };

} // namespace

namespace il2cpp {

void* resolve_icall(const char* name)
{
    using Resolve = void* (*)(const char*);
    Resolve resolve = g_game ? reinterpret_cast<Resolve>(GetProcAddress(g_game, "il2cpp_resolve_icall")) : nullptr;
    if (!resolve || !name) return nullptr;
    __try { return resolve(name); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

bool module_present()
{
    // Cheap and side-effect free: this only walks the loader's module list. It does NOT
    // mean the module is usable — see acquire_module().
    return GetModuleHandleW(L"GameAssembly.dll") != nullptr;
}

bool acquire_module()
{
    if (g_game) return true;

    // GetModuleHandleW starts succeeding the moment the loader inserts GameAssembly.dll
    // into the module list, which is BEFORE its imports are snapped and before its DllMain
    // runs. Calling an export in that window is calling into a module whose IAT thunks are
    // still unresolved — that is what killed this thread on 2026-08-07 and 2026-08-12 (see
    // dllmain.cpp).
    //
    // LoadLibraryW on a module the loader is still working on blocks until that load has
    // completed, so it is our barrier. It also takes a reference, which pins the module so
    // it cannot be unloaded while we hold code pointers into it. GameAssembly.dll is 378 MB
    // and takes ~11 s to map, so the window this closes is a wide one.
    g_game = LoadLibraryW(L"GameAssembly.dll");
    if (!g_game) {
        LOGF("il2cpp: LoadLibrary(GameAssembly.dll) failed (err=%lu)", GetLastError());
        return false;
    }
    return true;
}

bool bind()
{
    if (g_bound) return true;
    if (!g_game && !acquire_module()) return false;

    bool ok = true;
    ok &= grab(p_domain_get,            "il2cpp_domain_get");
    ok &= grab(p_domain_get_assemblies, "il2cpp_domain_get_assemblies");
    ok &= grab(p_assembly_get_image,    "il2cpp_assembly_get_image");
    ok &= grab(p_image_get_name,        "il2cpp_image_get_name");
    ok &= grab(p_class_from_name,       "il2cpp_class_from_name");
    // Optional: only nested-type lookups ("Outer.Inner") need it, and a miss must cost us
    // those readers rather than the whole mod.
    grab(p_class_get_nested,            "il2cpp_class_get_nested_types");
    ok &= grab(p_class_get_method,      "il2cpp_class_get_method_from_name");
    // Optional: a missing typed-overload API only silences the readers needing it.
    grab(p_class_get_methods,           "il2cpp_class_get_methods");
    grab(p_method_get_name,             "il2cpp_method_get_name");
    grab(p_method_get_param_count,      "il2cpp_method_get_param_count");
    grab(p_method_get_param,            "il2cpp_method_get_param");
    grab(p_class_from_type,             "il2cpp_class_from_type");
    ok &= grab(p_class_get_field,       "il2cpp_class_get_field_from_name");
    ok &= grab(p_class_get_name,        "il2cpp_class_get_name");
    ok &= grab(p_field_get_offset,      "il2cpp_field_get_offset");
    ok &= grab(p_object_get_class,      "il2cpp_object_get_class");
    ok &= grab(p_string_chars,          "il2cpp_string_chars");
    ok &= grab(p_string_length,         "il2cpp_string_length");
    ok &= grab(p_thread_attach,         "il2cpp_thread_attach");
    ok &= grab(p_thread_current,        "il2cpp_thread_current");
    // Optional: if this one is ever missing we simply never detach. That is survivable
    // because the boot thread parks instead of exiting (see dllmain.cpp) — but a missing
    // export must not take the whole mod down with it.
    grab(p_thread_detach,               "il2cpp_thread_detach");
    ok &= grab(p_class_get_type,        "il2cpp_class_get_type");
    ok &= grab(p_type_get_object,       "il2cpp_type_get_object");
    ok &= grab(p_runtime_invoke,        "il2cpp_runtime_invoke");
    // Optional: only Transform.Find(name) and static-singleton reads need these. A miss
    // costs those readers, never the mod.
    grab(p_string_new_utf16,            "il2cpp_string_new_utf16");
    grab(p_field_static_get,            "il2cpp_field_static_get_value");
    grab(p_object_get_virtual,          "il2cpp_object_get_virtual_method");
    grab(p_class_get_parent,            "il2cpp_class_get_parent");
    grab(p_class_get_namespace,         "il2cpp_class_get_namespace");
    grab(p_class_get_fields,            "il2cpp_class_get_fields");
    grab(p_field_get_name,              "il2cpp_field_get_name");

    if (!ok) { LOGF("il2cpp: bind FAILED"); return false; }
    g_bound = true;
    LOGF("il2cpp: bound to GameAssembly.dll at %p", (void*)g_game);
    return true;
}

bool runtime_ready()
{
    if (!g_bound || !p_domain_get) return false;

    // il2cpp_init() has not necessarily been called yet, so this is a call into a runtime
    // that may still be half-built. Treat a fault as "not ready", never as a reason to die:
    // the caller retries, and the process stays up.
    //
    // The __except here is a filter, NOT a licence to leave the thread. Unwinding out of a
    // game frame can skip whatever that frame would have done on the way out — including,
    // in this runtime, un-registering the calling thread from the garbage collector. That
    // is exactly why the thread that runs this must never terminate (see dllmain.cpp).
    __try {
        return p_domain_get() != nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (!g_probe_faulted) {
            g_probe_faulted = true;
            LOGF("il2cpp: domain probe faulted (runtime still booting) — will retry");
        }
        return false;
    }
}

void attach_current_thread()
{
    if (!g_bound) return;
    if (p_thread_current && p_thread_current()) return;   // already attached
    if (auto* d = p_domain_get()) {
        p_thread_attach(d);
        LOGF("il2cpp: attached thread %lu", GetCurrentThreadId());
    }
}

void detach_current_thread()
{
    if (!g_bound || !p_thread_detach || !p_thread_current) return;
    __try {
        Thread* t = p_thread_current();
        if (!t) return;
        p_thread_detach(t);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    LOGF("il2cpp: detached thread %lu", GetCurrentThreadId());
}

namespace {

/// Nested type named `name` directly inside `outer`, or null.
il2cpp::Class* nested_in(il2cpp::Class* outer, const char* name)
{
    if (!outer || !p_class_get_nested || !p_class_get_name) return nullptr;
    __try {
        void* iter = nullptr;
        while (il2cpp::Class* n = p_class_get_nested(outer, &iter)) {
            const char* nn = p_class_get_name(n);
            if (nn && strcmp(nn, name) == 0) return n;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return nullptr;
}

/// Resolve "Outer.Inner" (or "Outer.Mid.Inner") within one image.
///
/// il2cpp_class_from_name only ever sees TOP-LEVEL types: a nested type's metadata name is
/// just its own short name, with the nesting recorded on the declaring type. So asking it
/// for "MainMenu.Dialog" fails, silently — which is exactly how the MainMenu.Dialog and
/// CommandWindow.SubWindow readers came to be installed-but-absent. Namespaces are passed
/// separately here, so a '.' inside `name` can only ever mean nesting.
il2cpp::Class* resolve_nested(il2cpp::Image* img, const char* name_space, const char* name)
{
    const char* dot = strchr(name, '.');
    if (!dot) return nullptr;

    char seg[192];
    size_t len = static_cast<size_t>(dot - name);
    if (len == 0 || len >= sizeof(seg)) return nullptr;
    memcpy(seg, name, len);
    seg[len] = 0;

    il2cpp::Class* k = p_class_from_name(img, name_space ? name_space : "", seg);

    for (const char* rest = dot + 1; k && rest && *rest; ) {
        const char* next = strchr(rest, '.');
        len = next ? static_cast<size_t>(next - rest) : strlen(rest);
        if (len == 0 || len >= sizeof(seg)) return nullptr;
        memcpy(seg, rest, len);
        seg[len] = 0;
        k    = nested_in(k, seg);
        rest = next ? next + 1 : nullptr;
    }
    return k;
}

} // namespace

Class* find_class(const char* assembly, const char* name_space, const char* name)
{
    if (!g_bound) return nullptr;
    auto* domain = p_domain_get();
    if (!domain) return nullptr;

    size_t count = 0;
    Assembly** list = p_domain_get_assemblies(domain, &count);
    if (!list) return nullptr;

    for (size_t i = 0; i < count; ++i) {
        Image* img = p_assembly_get_image(list[i]);
        if (!img) continue;
        if (assembly) {
            const char* iname = p_image_get_name(img);
            if (!iname || _stricmp(iname, assembly) != 0) continue;
        }
        if (Class* k = p_class_from_name(img, name_space ? name_space : "", name))
            return k;
        if (Class* k = resolve_nested(img, name_space, name))
            return k;
        if (assembly) break;   // named assembly checked, don't scan the rest
    }
    return nullptr;
}

Class* find_nested_prefix(Class* outer, const char* prefix)
{
    if (!g_bound || !outer || !prefix || !p_class_get_nested || !p_class_get_name) return nullptr;
    const size_t n = strlen(prefix);
    __try {
        void* iter = nullptr;
        while (Class* k = p_class_get_nested(outer, &iter)) {
            const char* name = p_class_get_name(k);
            if (name && strncmp(name, prefix, n) == 0) return k;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return nullptr;
}

Class* find_nested_prefix_field(Class* outer, const char* prefix, const char* field, bool has)
{
    if (!g_bound || !outer || !prefix || !field || !p_class_get_nested || !p_class_get_name) return nullptr;
    const size_t n = strlen(prefix);
    __try {
        void* iter = nullptr;
        while (Class* k = p_class_get_nested(outer, &iter)) {
            const char* name = p_class_get_name(k);
            if (!name || strncmp(name, prefix, n) != 0) continue;
            if ((p_class_get_field(k, field) != nullptr) == has) return k;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return nullptr;
}

Method* find_method(Class* klass, const char* name, int argc)
{
    if (!g_bound || !klass) return nullptr;
    return const_cast<Method*>(p_class_get_method(klass, name, argc));
}


Method* find_method_overload(Class* klass,const char* name,int argc,Class* first_param)
{
    return find_method_overload_param(klass,name,argc,first_param,0);
}

Method* find_method_overload_param(Class* klass, const char* name, int argc, Class* param_class, int param_index)
{
    if (!g_bound || !klass || !name || argc<1 || !param_class || param_index<0 || param_index>=argc || !p_class_get_methods
        || !p_method_get_name || !p_method_get_param_count || !p_method_get_param || !p_class_from_type)
        return nullptr;
    // Metadata enumeration happens once at install, never in a reader/hotkey path.
    __try {
        void* iter=nullptr;
        Method* found=nullptr;
        while (const Method* m=p_class_get_methods(klass,&iter)) {
            const char* methodName=p_method_get_name(m);
            if (!methodName || strcmp(methodName,name)!=0 || p_method_get_param_count(m)!=static_cast<uint32_t>(argc))
                continue;
            void* type=p_method_get_param(m,static_cast<uint32_t>(param_index));
            if (!type || p_class_from_type(type)!=param_class) continue;
            if (found) return nullptr; // still ambiguous: fail silent instead of guessing
            found=const_cast<Method*>(m);
        }
        return found;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

int32_t field_offset(Class* klass, const char* name)
{
    if (!g_bound || !klass) return -1;
    Field* f = p_class_get_field(klass, name);
    if (!f) return -1;
    return static_cast<int32_t>(p_field_get_offset(f));
}

void* method_pointer(Method* m)
{
    if (!m) return nullptr;
    return reinterpret_cast<MethodInfoHead*>(m)->methodPointer;
}

void* class_type_object(Class* k)
{
    if (!g_bound || !k || !p_class_get_type || !p_type_get_object) return nullptr;
    void* t = p_class_get_type(k);
    return t ? p_type_get_object(t) : nullptr;
}

void* invoke_raw(Method* m, void* obj, void** args, int argc)
{
    if (!g_bound || !m || !p_runtime_invoke) return nullptr;
    (void)argc;
    void* exc = nullptr;
    void* result = nullptr;
    __try {
        result = p_runtime_invoke(m, obj, args, &exc);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    if (exc) return nullptr;   // managed exception: treat as "no value", never propagate
    return result;
}

void* invoke_ptr(Method* m, void* obj, void** args, int argc)
{
    // Reference returns come back as the object pointer itself.
    return invoke_raw(m, obj, args, argc);
}

int invoke_int(Method* m, void* obj, void** args, int argc)
{
    void* boxed = invoke_raw(m, obj, args, argc);
    if (!boxed) return 0;
    __try {
        return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(boxed) + kObjectHeaderSize);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

bool invoke_bool(Method* m, void* obj, void** args, int argc)
{
    void* boxed = invoke_raw(m, obj, args, argc);
    if (!boxed) return false;
    __try {
        return *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(boxed) + kObjectHeaderSize) != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

const char* object_class_name(Object* o)
{
    if (!g_bound || !o) return "<null>";
    __try {
        Class* k = p_object_get_class(o);
        if (!k) return "<noclass>";
        const char* n = p_class_get_name(k);
        return n ? n : "<noname>";
    } __except (EXCEPTION_EXECUTE_HANDLER) { return "<invalid>"; }
}

Class* object_class(Object* o)
{
    if (!g_bound || !o) return nullptr;
    __try {
        return p_object_get_class(o);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool read_string(String* s, wchar_t* out, size_t out_chars)
{
    if (!out || out_chars == 0) return false;
    out[0] = 0;
    if (!s || !g_bound) return false;

    // Read the length/char buffer straight out of the object. We deliberately do NOT
    // call il2cpp_string_chars/length here: those are fine, but reading the fixed layout
    // avoids two cross-module calls per string and behaves identically.
    __try {
        int32_t len = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(s) + kStringLengthOff);
        if (len < 0 || len > 0x20000) return false;          // sanity: reject garbage
        auto* chars = reinterpret_cast<const wchar_t*>(
            reinterpret_cast<uint8_t*>(s) + kStringCharsOff);

        text_util::copy(out,out_chars,chars,static_cast<size_t>(len));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // A bad pointer from the game is not worth crashing the process over.
        out[0] = 0;
        return false;
    }
}

String* new_string(const wchar_t* text)
{
    if (!g_bound || !p_string_new_utf16 || !text) return nullptr;
    __try {
        return p_string_new_utf16(text, static_cast<int32_t>(wcslen(text)));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

void* read_static_ptr(Class* klass, const char* field)
{
    if (!g_bound || !klass || !p_field_static_get || !field) return nullptr;
    Field* f = p_class_get_field(klass, field);
    if (!f) return nullptr;
    void* value = nullptr;
    __try {
        p_field_static_get(f, &value);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    return value;
}

bool read_static_int(Class* klass, const char* field, int& out)
{
    out=0;
    if (!g_bound || !klass || !p_field_static_get || !field) return false;
    __try {
        Field* f=p_class_get_field(klass,field);
        if (!f) return false;
        p_field_static_get(f,&out);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { out=0; return false; }
}

bool read_static_float(Class* klass, const char* field, float& out)
{
    out=0;
    if (!g_bound || !klass || !p_field_static_get || !field) return false;
    __try {
        Field* f=p_class_get_field(klass,field);
        if (!f) return false;
        p_field_static_get(f,&out);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { out=0; return false; }
}

bool read_static_vec2(Class* klass, const char* field, float& x, float& y)
{
    x=y=0;
    if (!g_bound || !klass || !p_field_static_get || !field) return false;
    __try {
        Field* f=p_class_get_field(klass,field);
        if (!f) return false;
        float value[2]{};
        p_field_static_get(f,value);
        x=value[0]; y=value[1]; return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { x=y=0; return false; }
}

Method* virtual_method(Object* obj, Method* m)
{
    if (!g_bound || !obj || !m) return m;
    if (!p_object_get_virtual) return m;
    __try {
        const Method* v = p_object_get_virtual(obj, m);
        return v ? const_cast<Method*>(v) : m;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return m; }
}

bool invoke_str(Method* m, void* obj, void** args, int argc, wchar_t* out, size_t out_chars)
{
    if (out && out_chars) out[0] = 0;
    void* s = invoke_raw(m, obj, args, argc);
    return read_string(s, out, out_chars);
}

Class* class_parent(Class* k)
{
    if (!g_bound || !k || !p_class_get_parent) return nullptr;
    __try { return p_class_get_parent(k); } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

const char* class_name(Class* k)
{
    if (!g_bound || !k) return "";
    __try { const char* n = p_class_get_name(k); return n ? n : ""; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return ""; }
}

const char* class_namespace(Class* k)
{
    if (!g_bound || !k || !p_class_get_namespace) return "";
    __try { const char* n = p_class_get_namespace(k); return n ? n : ""; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return ""; }
}

bool is_a(Object* o, Class* k)
{
    if (!o || !k) return false;
    for (Class* c = object_class(o); c; c = class_parent(c))
        if (c == k) return true;
    return false;
}

bool class_derives(Class* c, Class* base)
{
    for (; c && base; c = class_parent(c))
        if (c == base) return true;
    return false;
}

int32_t field_offset_any(Class* klass, const char* const* names, int count)
{
    for (int i = 0; i < count; ++i) {
        int32_t off = field_offset(klass, names[i]);
        if (off >= 0) return off;
    }
    return -1;
}

void list_fields(Class* klass, void (*fn)(const char* name, int32_t offset, void* state), void* state)
{
    if (!g_bound || !klass || !p_class_get_fields || !p_field_get_name) return;
    __try {
        void* iter = nullptr;
        while (Field* f = p_class_get_fields(klass, &iter))
            fn(p_field_get_name(f), static_cast<int32_t>(p_field_get_offset(f)), state);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

} // namespace il2cpp
