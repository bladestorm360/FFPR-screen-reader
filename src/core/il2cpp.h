#pragma once
#include <windows.h>
#include <cstdint>

// ---------------------------------------------------------------------------
// Minimal IL2CPP runtime binding.
//
// WHY THIS EXISTS AT ALL, AND WHY IT CANNOT LEAK LIKE THE OLD MOD DID
// -------------------------------------------------------------------
// The previous C# mod ran on BepInEx + Il2CppInterop. There, every value crossing
// between managed and native produced a managed WRAPPER object holding a GCHandle that
// PINNED the native object until .NET finalised it. Do that faster than finalisation
// (e.g. per frame) and the game's Boehm heap can never reclaim — which is what melted
// the user's machine.
//
// Here there is no managed runtime, no wrappers, and no GCHandles. We hold raw pointers,
// read fields at known offsets, and copy strings into our own buffers. There is nothing
// to pin and nothing to finalise, so that failure mode is structurally impossible.
//
// Everything is resolved BY NAME at runtime through the game's own exported C API, so no
// hardcoded RVAs and nothing to re-dump when the game patches.
// ---------------------------------------------------------------------------

namespace il2cpp {

// Opaque runtime handles — deliberately just pointers.
using Domain   = void;
using Assembly = void;
using Image    = void;
using Class    = void;
using Method   = void;
using Field    = void;
using Object   = void;
using String   = void;
using Thread   = void;

// --- IL2CPP object layout (x64) --------------------------------------------
// Il2CppObject { void* klass; void* monitor; }                 -> 0x10 bytes
// Il2CppString{ Il2CppObject obj; int32_t length; char16 chars[]; }
//   length @ 0x10, chars @ 0x14
constexpr size_t kObjectHeaderSize = 0x10;
constexpr size_t kStringLengthOff  = 0x10;
constexpr size_t kStringCharsOff   = 0x14;

/// True once GameAssembly.dll is in the loader's module list. This says the module EXISTS,
/// not that it is usable — the loader inserts it before snapping its imports. Never call an
/// export on the strength of this alone; use acquire_module() first.
bool module_present();

/// Block until the loader has finished with GameAssembly.dll, and pin it. Safe to call
/// while the load is still in flight — that is the point of it.
bool acquire_module();

/// Bind to GameAssembly.dll's exported API. Acquires the module if that hasn't happened.
bool bind();

/// True once the IL2CPP domain exists (i.e. the runtime has finished booting). Faults from
/// calling in too early are absorbed and reported as "not ready".
bool runtime_ready();

/// Attach the CALLING thread to the IL2CPP runtime. Mandatory before touching any
/// runtime object from a thread the game did not create.
/// Hook callbacks already run on game threads and must NOT call this.
///
/// A thread that attaches is entered into the garbage collector's thread table, and the GC
/// suspends every thread in that table on each collection. It does NOT check whether the
/// thread still exists first — so a thread that attaches and then TERMINATES kills the
/// process at the next collection. Whoever attaches must therefore either detach or never
/// exit. See dllmain.cpp.
void attach_current_thread();

/// Remove the calling thread from the runtime (and so from the GC's thread table).
void detach_current_thread();

// --- lookup (all by name, all cached by the caller) -------------------------
Class*  find_class(const char* assembly, const char* name_space, const char* name);
/// The type nested directly in `outer` whose name STARTS WITH `prefix`, or null. For the
/// classes the C# compiler generates: a coroutine's state machine is "<ItemNumCoroutine>d__121",
/// and the number is the method's position in its class - it moves when a game patch adds a
/// method above it, the part before it does not.
Class*  find_nested_prefix(Class* outer, const char* prefix);
/// The same search for when SEVERAL nested types share the prefix - two overloads of one
/// coroutine both compile to "<Coroutine>d__N" - told apart by a field: the first match that
/// has (`has` true) or lacks (`has` false) a field named `field`. An argument of the
/// coroutine is a field of its state machine under its own name.
Class*  find_nested_prefix_field(Class* outer, const char* prefix, const char* field, bool has);
Method* find_method(Class* klass, const char* name, int argc);
/// Resolve equal-argc overloads by the exact first parameter class; null on ambiguity.
/// Requires optional metadata exports. Call during installation, not during input.
Method* find_method_overload(Class* klass, const char* name, int argc, Class* first_param);
Method* find_method_overload_param(Class* klass, const char* name, int argc, Class* param_class, int param_index);
/// Byte offset of a field within its object, or -1.
int32_t field_offset(Class* klass, const char* name);

// --- reading ----------------------------------------------------------------
/// Copy an Il2CppString into `out` as UTF-16. Safe against null/garbage pointers.
/// Empty strings are valid; false and an empty output indicate a null/invalid string.
bool read_string(String* s, wchar_t* out, size_t out_chars);

/// Read a field of an object by byte offset. No allocation, no pinning.
template <typename T>
inline T read_field(void* obj, int32_t offset)
{
    if (!obj || offset < 0) return T{};
    __try {
        return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(obj) + offset);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return T{}; }
}

/// Native code address of a resolved method — this is what we hand to MinHook.
void* method_pointer(Method* m);

/// The System.Type object for a class (needed to call non-generic GetComponent(Type)).
void* class_type_object(Class* k);

// --- calling game methods ---------------------------------------------------
// Thin wrappers over il2cpp_runtime_invoke. Any managed exception is swallowed and
// reported as a null/zero result: a misbehaving game method must never unwind through a
// hook and take the process with it.
void* invoke_raw (Method* m, void* obj, void** args, int argc);
void* invoke_ptr (Method* m, void* obj, void** args, int argc);
int   invoke_int (Method* m, void* obj, void** args, int argc);
bool  invoke_bool(Method* m, void* obj, void** args, int argc);

/// Managed string from a UTF-16 buffer (for passing names to Transform.Find etc.). The
/// object is GC-owned; never retain it past the call it was made for. Null if the export
/// is missing.
String* new_string(const wchar_t* text);

/// Value of a STATIC reference field (a singleton `instance`, a static GameObject). Null
/// if the class/field is unknown or the export is missing.
void* read_static_ptr(Class* klass, const char* field);
/// Copy a named static float; false on missing metadata/export or a guarded read fault.
bool read_static_float(Class* klass, const char* field, float& out);
bool read_static_int(Class* klass, const char* field, int& out);
bool read_static_vec2(Class* klass, const char* field, float& x, float& y);

/// The override of virtual `m` that `obj` actually runs. il2cpp_runtime_invoke calls the
/// MethodInfo it is given, NOT the object's override, so a virtual (BtlChara.GetBP,
/// IsPlayer) must be resolved through this first. Falls back to `m` itself.
Method* virtual_method(Object* obj, Method* m);

/// Invoke a method that returns a string and copy the result out. False on any miss.
bool invoke_str(Method* m, void* obj, void** args, int argc, wchar_t* out, size_t out_chars);

/// Class name of an object, for diagnostics. Never returns null.
const char* object_class_name(Object* o);

/// The class of a live object. This is the ONLY way to reach a NESTED type's fields:
/// il2cpp_class_from_name cannot see nested types and silently returns null for
/// "Outer.Inner", so find_class() is useless for e.g. CollisionCtrl.CollisionUnit. Given
/// an instance we already hold, the runtime hands us its class directly.
Class* object_class(Object* o);

// --- class hierarchy (FFPR additions) ------------------------------------------
Class* class_parent(Class* k);
/// Class name / namespace; "" (never null) when unknown.
const char* class_name(Class* k);
const char* class_namespace(Class* k);
/// True when `o`'s class is `k` or derives from it. This is how a detour on a FOLDED method
/// body tells its own class from the others that share the address.
bool is_a(Object* o, Class* k);
bool class_derives(Class* c, Class* base);
/// The first of several candidate field names that exists (games renamed a few fields).
int32_t field_offset_any(Class* klass, const char* const* names, int count);
/// Every field declared on `klass` itself (not its parents), for the startup self-check.
void list_fields(Class* klass, void (*fn)(const char* name, int32_t offset, void* state), void* state);

} // namespace il2cpp
