#pragma once
#include "descriptor_query_observer.hpp"
namespace d4vr::query_observer {
// Unix owned-run transport; claim files are never removed or reused.
class ScopedTransport {
public:
    ScopedTransport(const char* directory,const char* runId) noexcept;
    ~ScopedTransport();
    ScopedTransport(const ScopedTransport&)=delete;
    ScopedTransport& operator=(const ScopedTransport&)=delete;
    bool ownerMatches() noexcept;
    bool writeHeader(const std::string& jsonl) noexcept;
    bool finish(const std::string& jsonl) noexcept;
    const ScopeIdentity& identity() const noexcept {return identity_;}
    bool valid() const noexcept {return output_>=0&&!failed_;}
    bool failed() const noexcept {return failed_;}
private:
    ScopeIdentity identity_{};
    int output_=-1;
    bool failed_=false,headerWritten_=false;
    bool write(const char*,std::size_t) noexcept;
};
} // namespace d4vr::query_observer
