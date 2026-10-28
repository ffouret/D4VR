// Independent CPU-only checks. No COM, DLL, driver or operating-system API.
#include "private_translator_runtime_owned_protocol.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace p = d4vr_private_owned;
namespace {
unsigned assertions = 0;
void check(bool condition) {
    ++assertions;
    if (!condition) throw std::runtime_error("independent protocol assertion");
}
struct Cleanup {
    unsigned& count;
    ~Cleanup() { ++count; }
};
std::string empty_args() { return "{}"; }
std::string scalar(std::uint64_t value) {
    return "{\"value\":" + std::to_string(value) + "}";
}

void healthy_void_and_scalar() {
    p::Metadata meta;
    p::Journal journal(meta);
    unsigned calls = 0;
    journal.caseOrdinal = 1;
    journal.callVoid("OwnedVoid", true, empty_args, [&] { ++calls; });
    check(calls == 1 && !meta.faulted);
    check(journal.allRows.size() == 2 && journal.projectedRows == journal.allRows);
    check(journal.last.ordinal == 101 && journal.last.entry == 1001 && journal.last.returned == 1002);
    check(journal.allRows[0].find("\"original_returned\":false,\"result\":null") != std::string::npos);
    check(journal.allRows[1].find("\"return_kind\":\"void\"") != std::string::npos);
    check(journal.allRows[1].find("\"original_returned\":true,\"result\":null") != std::string::npos);
    const auto actual = journal.value("OwnedScalar", "uint64", false, empty_args,
        [&] { ++calls; return std::uint64_t(0xfedcba9876543210ULL); }, scalar);
    check(actual == 0xfedcba9876543210ULL && calls == 2 && !meta.faulted);
    check(journal.allRows.size() == 4 && journal.projectedRows.size() == 2);
}

void before_bad_alloc_does_not_skip_original_or_cleanup() {
    p::Metadata meta;
    p::Journal journal(meta);
    unsigned original = 0, cleanup = 0, after_builder = 0;
    {
        Cleanup guard{cleanup};
        const auto actual = journal.value("OwnedHRESULT", "HRESULT", true,
            []() -> std::string { throw std::bad_alloc(); },
            [&] { ++original; return std::int32_t(-123); },
            [&](std::int32_t) { ++after_builder; return std::string("null"); });
        check(actual == -123);
    }
    check(meta.faulted && original == 1 && cleanup == 1);
    check(after_builder == 0 && journal.allRows.empty());
}

void before_nonallocation_failure_does_not_skip_void() {
    p::Metadata meta;
    p::Journal journal(meta);
    unsigned original = 0, cleanup = 0;
    {
        Cleanup guard{cleanup};
        journal.callVoid("OwnedVoid", true,
            []() -> std::string { throw 7; }, [&] { ++original; });
    }
    check(meta.faulted && original == 1 && cleanup == 1);
    check(journal.projectedRows.empty());
}

void oversized_before_row_does_not_skip_original() {
    p::Metadata meta;
    p::Journal journal(meta);
    unsigned original = 0;
    journal.callVoid("OwnedVoid", true,
        [] { return std::string(32769, 'x'); }, [&] { ++original; });
    check(meta.faulted && original == 1);
    check(journal.allRows.size() <= 1);
}

void after_bad_alloc_preserves_actual_return_and_cleanup() {
    p::Metadata meta;
    p::Journal journal(meta);
    unsigned original = 0, cleanup = 0;
    {
        Cleanup guard{cleanup};
        const auto actual = journal.value("OwnedHRESULT", "HRESULT", true, empty_args,
            [&] { ++original; return std::int32_t(-456); },
            [](std::int32_t) -> std::string { throw std::bad_alloc(); });
        check(actual == -456);
    }
    check(meta.faulted && original == 1 && cleanup == 1);
    check(journal.allRows.size() == 1);
    check(journal.allRows[0].find("\"edge\":\"before\"") != std::string::npos);
}

void sticky_fault_never_calls_later_capture_builders() {
    p::Metadata meta;
    p::Journal journal(meta);
    meta.faulted = true;
    unsigned before = 0, original = 0, after = 0;
    const auto actual = journal.value("OwnedU32", "uint32", true,
        [&] { ++before; return empty_args(); },
        [&] { ++original; return std::uint32_t(99); },
        [&](std::uint32_t) { ++after; return std::string("null"); });
    check(actual == 99 && original == 1 && before == 0 && after == 0);
    check(journal.allRows.empty() && meta.faulted);
}

void original_arguments_and_before_snapshot_are_distinct() {
    p::Metadata meta;
    p::Journal journal(meta);
    struct Payload { unsigned value; } payload{3};
    Payload* const original_pointer = &payload;
    const unsigned values[] = {4, 5, 6};
    const unsigned* const array_pointer = values;
    unsigned calls = 0;
    journal.callVoid("OwnedPointers", true,
        [&] { return "{\"value_before\":" + std::to_string(payload.value) + "}"; },
        [&] {
            ++calls;
            check(original_pointer == &payload && array_pointer == values);
            check(array_pointer[0] == 4 && array_pointer[2] == 6);
            payload.value = 9;
        });
    check(calls == 1 && payload.value == 9 && !meta.faulted);
    check(journal.allRows[0].find("\"args\":{\"value_before\":3}") != std::string::npos);
    check(journal.allRows[1].find("\"args\":{\"value_before\":3}") != std::string::npos);
    const auto held = journal.allRows;
    payload.value = 77;
    check(journal.allRows == held);
}

void null_original_pointer_and_pointer_return_are_not_rewritten() {
    p::Metadata meta;
    p::Journal journal(meta);
    unsigned value = 11;
    unsigned* const ptr = &value;
    unsigned calls = 0;
    const auto actual = journal.value("OwnedPointer", "pointer", true,
        [] { return "{\"optional\":null}"; },
        [&] { ++calls; const unsigned* optional = nullptr; check(optional == nullptr); return ptr; },
        [](const unsigned*) { return "{\"value\":11}"; });
    check(actual == ptr && calls == 1 && !meta.faulted);
}

void row_budget_does_not_stop_originals_or_cleanup() {
    p::Metadata meta;
    p::Journal journal(meta);
    unsigned original = 0, cleanup = 0;
    {
        Cleanup guard{cleanup};
        for (unsigned i = 0; i < 256; ++i)
            journal.callVoid("OwnedVoid", true, empty_args, [&] { ++original; });
        check(journal.allRows.size() == p::maxRows && !meta.faulted);
        journal.callVoid("OwnedVoid", true, empty_args, [&] { ++original; });
    }
    check(original == 257 && cleanup == 1 && meta.faulted);
    check(journal.allRows.size() == p::maxRows);
}

void ordinal_and_clock_overflow_do_not_stop_original() {
    for (unsigned mode = 0; mode < 2; ++mode) {
        p::Metadata meta;
        p::Journal journal(meta);
        if (mode == 0) journal.ordinal = std::numeric_limits<std::uint64_t>::max();
        else journal.clock = std::numeric_limits<std::uint64_t>::max();
        unsigned calls = 0;
        journal.callVoid("OwnedVoid", true, empty_args, [&] { ++calls; });
        check(meta.faulted && calls == 1 && journal.allRows.empty());
    }
}

void RETURN_clock_overflow_preserves_original_value() {
    p::Metadata meta;
    p::Journal journal(meta);
    journal.clock = std::numeric_limits<std::uint64_t>::max() - 1;
    unsigned calls = 0;
    const auto actual = journal.value("OwnedU32", "uint32", true, empty_args,
        [&] { ++calls; return std::uint32_t(17); }, scalar);
    check(actual == 17 && calls == 1 && meta.faulted);
    check(journal.allRows.size() == 1);
}

void reentrant_original_has_real_nested_entry_and_return_order() {
    p::Metadata meta;
    p::Journal journal(meta);
    unsigned outer = 0, inner = 0;
    journal.callVoid("Outer", true, empty_args, [&] {
        ++outer;
        journal.callVoid("Inner", true, empty_args, [&] { ++inner; });
    });
    check(outer == 1 && inner == 1 && !meta.faulted);
    check(journal.allRows.size() == 4);
    check(journal.allRows[0].find("\"api\":\"Outer\",\"edge\":\"before\"") != std::string::npos);
    check(journal.allRows[1].find("\"api\":\"Inner\",\"edge\":\"before\"") != std::string::npos);
    check(journal.allRows[2].find("\"api\":\"Inner\",\"edge\":\"after\"") != std::string::npos);
    check(journal.allRows[3].find("\"api\":\"Outer\",\"edge\":\"after\"") != std::string::npos);
    check(journal.last.ordinal == 101 && journal.last.entry == 1001 && journal.last.returned == 1004);
}

void pending_entry_has_no_invented_completion() {
    p::Metadata meta;
    p::Journal journal(meta);
    const auto e = journal.before("Pending", "void", true, empty_args);
    check(e.ref.ordinal == 101 && e.ref.returned == 0 && !meta.faulted);
    check(journal.allRows.size() == 1);
    check(journal.projection().find("\"original_returned\":true") == std::string::npos);
}

void duplicate_AFTER_is_not_an_extra_original() {
    p::Metadata meta;
    p::Journal journal(meta);
    auto e = journal.before("OwnedVoid", "void", true, empty_args);
    journal.after(e, [] { return std::string("null"); });
    const auto held = journal.allRows;
    const auto clock = journal.clock;
    unsigned second_builder = 0;
    journal.after(e, [&] { ++second_builder; return std::string("null"); });
    check(meta.faulted && second_builder == 0);
    check(journal.clock == clock && journal.allRows == held);
}

void copied_entry_or_wrong_journal_cannot_finish_another_original() {
    p::Metadata meta;
    p::Journal journal(meta);
    auto e = journal.before("Original", "void", true, empty_args);
    auto alias = e;
    journal.after(e, [] { return std::string("null"); });
    const auto held = journal.allRows;
    unsigned builds = 0;
    journal.after(alias, [&] { ++builds; return std::string("null"); });
    check(meta.faulted && builds == 0 && journal.allRows == held);
    p::Metadata other_meta;
    p::Journal other(other_meta);
    other.after(e, [&] { ++builds; return std::string("null"); });
    check(other_meta.faulted && builds == 0 && other.allRows.empty());
}

void original_exception_is_not_converted_to_a_false_hresult() {
    p::Metadata meta;
    p::Journal journal(meta);
    unsigned original = 0, cleanup = 0, after = 0;
    bool propagated = false;
    try {
        Cleanup guard{cleanup};
        journal.value("ThrowingOriginal", "HRESULT", true, empty_args,
            [&]() -> std::int32_t { ++original; throw std::runtime_error("actual original exception"); },
            [&](std::int32_t) { ++after; return std::string("null"); });
    } catch (const std::runtime_error& e) {
        propagated = std::string(e.what()) == "actual original exception";
    }
    check(propagated && original == 1 && cleanup == 1 && after == 0);
    check(journal.allRows.size() == 1);
}

void copies_are_detached_after_eighty_later_originals() {
    p::Metadata meta;
    p::Journal journal(meta);
    journal.callVoid("First", true, empty_args, [] {});
    const auto held = journal.projection();
    const auto held_rows = journal.allRows;
    for (unsigned i = 0; i < 80; ++i)
        journal.callVoid("Later", true, empty_args, [] {});
    check(!meta.faulted && journal.allRows.size() == 162);
    check(journal.allRows[0] == held_rows[0] && journal.allRows[1] == held_rows[1]);
    check(held.find("Later") == std::string::npos);
}

void copy_storage_has_exact_owned_guards_and_counted_bytes() {
    p::CopyStorage c;
    check(c.initialize(1, 10));
    check(c.capacity == 10 && c.written == 0x12345678 && c.required == 0x87654321);
    check(std::all_of(c.storage.begin(), c.storage.begin() + 16, [](unsigned char x) { return x == 0xc3; }));
    check(std::all_of(c.storage.begin() + 16, c.storage.begin() + 26, [](unsigned char x) { return x == 0xa5; }));
    check(std::all_of(c.storage.begin() + 26, c.storage.begin() + 42, [](unsigned char x) { return x == 0x3c; }));
    c.beforeHex = p::hex(c.storage.data(), 42);
    c.storage[16] = 0; c.storage[17] = 0xff;
    c.written = c.required = 2;
    c.afterHex = p::hex(c.storage.data(), 42);
    check(c.beforeHex.size() == 84 && c.afterHex.size() == 84);
    check(c.afterHex.substr(32, 4) == "00ff");
    const auto held = c.afterHex;
    c.storage[16] = 123;
    check(c.afterHex == held);
    const auto storage = c.storage;
    check(!c.initialize(99, std::uint32_t(p::maxCopy + 1)));
    check(c.capacity == 10 && c.id == 1 && c.storage == storage);
}

void finite_json_scalars_and_all_stronger_claims_false() {
    check(p::quote("a\"b\\c\n") == "\"a\\\"b\\\\c\\u000a\"");
    check(p::number(std::numeric_limits<std::uint64_t>::max()) == "18446744073709551615");
    const auto claims = p::claims();
    check(claims.find(":true") == std::string::npos);
    std::size_t count = 0, pos = 0;
    while ((pos = claims.find(":false", pos)) != std::string::npos) { ++count; pos += 6; }
    check(count == 25);
}
} // namespace

int main() {
    const std::pair<const char*, void(*)()> cases[] = {
        {"healthy_void_scalar", healthy_void_and_scalar},
        {"before_badalloc", before_bad_alloc_does_not_skip_original_or_cleanup},
        {"before_throw_void", before_nonallocation_failure_does_not_skip_void},
        {"oversized_before", oversized_before_row_does_not_skip_original},
        {"after_badalloc", after_bad_alloc_preserves_actual_return_and_cleanup},
        {"sticky_fault", sticky_fault_never_calls_later_capture_builders},
        {"before_arguments", original_arguments_and_before_snapshot_are_distinct},
        {"null_pointer_return", null_original_pointer_and_pointer_return_are_not_rewritten},
        {"row_budget", row_budget_does_not_stop_originals_or_cleanup},
        {"entry_overflow", ordinal_and_clock_overflow_do_not_stop_original},
        {"return_overflow", RETURN_clock_overflow_preserves_original_value},
        {"reentrant", reentrant_original_has_real_nested_entry_and_return_order},
        {"pending", pending_entry_has_no_invented_completion},
        {"duplicate_after", duplicate_AFTER_is_not_an_extra_original},
        {"entry_alias_owner", copied_entry_or_wrong_journal_cannot_finish_another_original},
        {"original_exception", original_exception_is_not_converted_to_a_false_hresult},
        {"eighty_later", copies_are_detached_after_eighty_later_originals},
        {"copy_guards", copy_storage_has_exact_owned_guards_and_counted_bytes},
        {"JSON_false_claims", finite_json_scalars_and_all_stronger_claims_false},
    };
    unsigned failed = 0;
    for (const auto& item : cases) {
        try { item.second(); }
        catch (const std::exception& e) { ++failed; std::cerr << item.first << ": " << e.what() << '\n'; }
        catch (...) { ++failed; std::cerr << item.first << ": unexpected exception\n"; }
    }
    std::cout << "{\"CPU_protocol_only\":true,\"cases\":" << (sizeof(cases) / sizeof(cases[0]))
              << ",\"assertions\":" << assertions << ",\"failures\":" << failed
              << ",\"driver_or_renderer_calls\":0}\n";
    return failed ? 1 : 0;
}
