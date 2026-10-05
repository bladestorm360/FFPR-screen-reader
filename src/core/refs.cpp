#include <windows.h>
#include <cstring>
#include "refs.h"
#include "log.h"

namespace ref {
namespace {
Class*  g_classes = nullptr;
Field*  g_fields  = nullptr;
Method* g_methods = nullptr;
volatile LONG g_call_faults = 0;

bool copy_chars(wchar_t* dst, const void* src, size_t n)
{
    __try { memcpy(dst, src, n * sizeof(wchar_t)); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}

Class::Class(const char* ns_, const char* name_, uint8_t games_, const char* assembly_)
    : ns(ns_), name(name_), games(games_), assembly(assembly_)
{
    next_ = g_classes;
    g_classes = this;
}

il2cpp::Class* Class::get()
{
    if (tried_) return k_;
    if (!applies()) { tried_ = true; return nullptr; }
    k_ = il2cpp::find_class(assembly, ns, name);
    tried_ = true;
    if (!k_) LOGF("ref: class not found in %s: %s.%s", game::label(), ns ? ns : "", name);
    return k_;
}

bool Class::is(void* obj)
{
    il2cpp::Class* k = get();
    return k && obj && il2cpp::is_a(obj, k);
}

Field::Field(Class& c, const char* name_, const char* alt_) : cls(c), name(name_), alt(alt_)
{
    next_ = g_fields;
    g_fields = this;
}

int32_t Field::get()
{
    if (tried_) return off_;
    il2cpp::Class* k = cls.get();
    if (k) {
        off_ = il2cpp::field_offset(k, name);
        if (off_ < 0 && alt) off_ = il2cpp::field_offset(k, alt);
        if (off_ < 0) LOGF("ref: field not found in %s: %s.%s", game::label(), cls.name, name);
    }
    tried_ = true;
    return off_;
}

Method::Method(Class& c, const char* name_, int argc_) : cls(c), name(name_), argc(argc_)
{
    next_ = g_methods;
    g_methods = this;
}

il2cpp::Method* Method::get()
{
    if (tried_) return m_;
    il2cpp::Class* k = cls.get();
    if (k) {
        m_ = il2cpp::find_method(k, name, argc);
        if (!m_) LOGF("ref: method not found in %s: %s.%s(%d)", game::label(), cls.name, name, argc);
    }
    tried_ = true;
    return m_;
}

void* Method::ptr()
{
    il2cpp::Method* m = get();
    return m ? il2cpp::method_pointer(m) : nullptr;
}

int self_check()
{
    int classes = 0, fields = 0, methods = 0, missing = 0;
    for (Class* c = g_classes; c; c = c->next_) {
        if (!c->applies()) continue;
        ++classes;
        if (!c->get()) ++missing;
    }
    for (Field* f = g_fields; f; f = f->next_) {
        if (!f->cls.applies() || !f->cls.get()) continue;
        ++fields;
        if (f->get() < 0) ++missing;
    }
    for (Method* m = g_methods; m; m = m->next_) {
        if (!m->cls.applies() || !m->cls.get()) continue;
        ++methods;
        if (!m->get()) ++missing;
    }
    LOGF("self-check (%s): %d classes, %d fields, %d methods, %d missing",
         game::label(), classes, fields, methods, missing);
    return missing;
}

std::wstring str(void* s)
{
    if (!s) return {};
    // Length first, guarded, so a long dialogue page is never cut at a fixed buffer.
    int32_t len = il2cpp::read_field<int32_t>(s, static_cast<int32_t>(il2cpp::kStringLengthOff));
    if (len <= 0 || len > (1 << 20)) return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    if (!copy_chars(out.data(), static_cast<uint8_t*>(s) + il2cpp::kStringCharsOff,
                    static_cast<size_t>(len)))
        return {};
    return out;
}

std::wstring rd_str(void* obj, Field& f)
{
    return str(rd<void*>(obj, f));
}

void* new_str(const std::wstring& s)
{
    return il2cpp::new_string(s.c_str());
}

void call_failed_note(const char* name)
{
    LONG n = InterlockedIncrement(&g_call_faults);
    if (n <= 50) LOGF("call: %s threw or faulted (absorbed; %ld so far)", name ? name : "?", n);
}

void* instance(Class& c)
{
    il2cpp::Class* k = c.get();
    if (!k) return nullptr;
    il2cpp::Method* m = il2cpp::find_method(k, "get_Instance", 0);
    if (!m) m = il2cpp::find_method(k, "get_instance", 0);
    if (!m) return nullptr;
    return il2cpp::invoke_ptr(m, nullptr, nullptr, 0);
}

} // namespace ref
