#pragma once

#include "quest_config_file_backend.h"

namespace QuestVr::ConfigFile {
namespace ReadOnlyDetail {

struct DirectoryIdentity {
    std::uint64_t device{},index{};
    bool operator==(const DirectoryIdentity&) const=default;
};
inline DirectoryIdentity Identity(const Detail::File& directory) {
#ifdef _WIN32
    BY_HANDLE_FILE_INFORMATION info{};
    if(!directory||!GetFileInformationByHandle(directory.Get(),&info)||
        !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))
        Detail::Fail("read-only source directory is not pinned");
    return {info.dwVolumeSerialNumber,(std::uint64_t(info.nFileIndexHigh)<<32u)|info.nFileIndexLow};
#else
    struct stat info{};
    if(!directory||fstat(directory.Get(),&info)!=0||!S_ISDIR(info.st_mode))
        Detail::Fail("read-only source directory is not pinned");
    return {static_cast<std::uint64_t>(info.st_dev),static_cast<std::uint64_t>(info.st_ino)};
#endif
}
inline void CheckExplicit(const std::filesystem::path& supplied) {
    if(supplied.empty()||supplied.native().size()>32000u||
        supplied.native().find(std::filesystem::path::value_type{})!=supplied.native().npos||!supplied.is_absolute())
        Detail::Fail("read-only source requires a bounded explicit absolute path without NUL");
}
inline std::size_t Components(const std::filesystem::path& path) noexcept {
    std::size_t count=2u;
    for(const auto c:path.native())if(c=='/'||c=='\\')++count;
    return count;
}
// Includes simultaneous normalized/path-component temporaries in the pinned
// filesystem helpers. No file payload or already retained binding is included.
inline std::size_t PathWorkspace(const std::filesystem::path& path) noexcept {
    return 4096u+8u*((path.native().size()+1u)*sizeof(std::filesystem::path::value_type)+
        Components(path)*sizeof(std::filesystem::path));
}
struct State {
    std::filesystem::path path,name;
    Detail::File parent,file;
    DirectoryIdentity parentIdentity;
    Detail::Stamp stamp;
    Bytes bytes;
    std::size_t maximum{};
    mutable std::mutex mutex;
#ifdef _WIN32
    std::vector<Detail::File> ancestors;
#endif
};
inline Detail::File Open(const State& state) {
#ifdef _WIN32
    // Original source is never writable and cannot be renamed/deleted while
    // bound. Ancestors separately deny DELETE sharing, as the seed backend does.
    return Detail::File(CreateFileW(state.path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
        OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
#else
    // NONBLOCK prevents a substituted FIFO from blocking before FileStamp's
    // regular-file rejection. Every operation remains read-only.
    return Detail::File(openat(state.parent.Get(),state.name.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK));
#endif
}
inline void ValidateIdentity(const State& state) {
    Detail::CheckComponents(state.path);
    auto currentParent=Detail::PinDirectory(state.path.parent_path());
    if(Identity(currentParent)!=state.parentIdentity||Identity(state.parent)!=state.parentIdentity)
        Detail::Fail("read-only source parent identity changed");
    auto named=Open(state);
    if(!named||Detail::FileStamp(named,state.maximum)!=state.stamp||
        Detail::FileStamp(state.file,state.maximum)!=state.stamp)
        Detail::Fail("read-only source name, identity or stamp changed");
}
inline std::size_t Retained(const State& state) noexcept {
    auto bytes=sizeof(State)+4u*sizeof(void*)+state.bytes.capacity();
    for(const auto* path:{&state.path,&state.name}) {
        // Conservative path component/cache storage, consistent across the
        // supported filesystem implementations, not just native characters.
        bytes+=4u*((path->native().capacity()+1u)*sizeof(std::filesystem::path::value_type)+
            Components(*path)*sizeof(std::filesystem::path));
    }
#ifdef _WIN32
    bytes+=state.ancestors.capacity()*sizeof(Detail::File);
#endif
    return bytes;
}
} // namespace ReadOnlyDetail

// A separately pinned original/default source, NOT an app-owned writable
// Backend. No directory creation, lock/stage files, publication or repair API.
// Copies share immutable cached bytes and the same nonblocking read lock.
class ReadOnlySource {
public:
    template<class Admission>
    static ReadOnlySource BindAdmitted(const std::filesystem::path& supplied,Admission&& admitAbsolutePeakBytes,
        std::size_t maximum=MaximumBytes) {
        ReadOnlyDetail::CheckExplicit(supplied);
        if(!maximum||maximum>MaximumBytes)Detail::Fail("read-only source callers may only tighten the file bound");
        if(ReadOnlyDetail::Components(supplied)>128u)Detail::Fail("read-only source path depth exceeds its bound");
        const auto pathScratch=ReadOnlyDetail::PathWorkspace(supplied);
        auto initial=sizeof(ReadOnlySource)+sizeof(ReadOnlyDetail::State)+4u*sizeof(void*)+pathScratch;
#ifdef _WIN32
        initial+=ReadOnlyDetail::Components(supplied)*sizeof(Detail::File);
#endif
        // First callback precedes path normalization, control/state allocation
        // and Windows ancestor-vector allocation. Callback itself is borrowed,
        // never copied into or retained by the binding.
        admitAbsolutePeakBytes(initial);
        auto state=std::make_shared<ReadOnlyDetail::State>();state->maximum=maximum;
        state->path=Detail::Absolute(supplied);state->name=state->path.filename();
        if(state->name.empty()||state->path==state->path.root_path())Detail::Fail("read-only source has no file leaf");
        Detail::CheckComponents(state->path);
#ifdef _WIN32
        state->ancestors.reserve(ReadOnlyDetail::Components(supplied));
        auto current=state->path.root_path();state->ancestors.push_back(Detail::PinDirectory(current));
        for(const auto& component:state->path.parent_path().relative_path()) {
            current/=component;state->ancestors.push_back(Detail::PinDirectory(current));
        }
#endif
        state->parent=Detail::PinDirectory(state->path.parent_path());
        state->parentIdentity=ReadOnlyDetail::Identity(state->parent);state->file=ReadOnlyDetail::Open(*state);
        state->stamp=Detail::FileStamp(state->file,maximum);
        ReadOnlyDetail::ValidateIdentity(*state);
        auto bytes=Detail::Read(state->file,maximum,[&](std::size_t actual) {
            admitAbsolutePeakBytes(sizeof(ReadOnlySource)+ReadOnlyDetail::Retained(*state)+pathScratch+sizeof(Bytes)+actual);
        });
        state->bytes=std::move(bytes);
        ReadOnlyDetail::ValidateIdentity(*state);
        ReadOnlySource result;result.state_=std::move(state);return result;
    }
    // Each successful use revalidates both held/named identities and exact
    // immutable contents. Returned bytes borrow this binding's lifetime; a
    // caller's already retained binding/cache is EXCLUDED from these absolute
    // extra scratch peaks. No payload copy escapes this operation.
    template<class Admission>
    const Bytes& ReadAdmitted(Admission&& admitAbsolutePeakBytes) const & {
        if(!state_)Detail::Fail("read-only source is not bound");
        std::unique_lock lock(state_->mutex,std::try_to_lock);
        if(!lock.owns_lock())Detail::Fail("another read owns the read-only source");
        const auto pathScratch=ReadOnlyDetail::PathWorkspace(state_->path);
        admitAbsolutePeakBytes(pathScratch+sizeof(Bytes));
        ReadOnlyDetail::ValidateIdentity(*state_);
        const auto bytes=Detail::Read(state_->file,state_->maximum,[&](std::size_t actual) {
            admitAbsolutePeakBytes(pathScratch+sizeof(Bytes)+actual);
        });
        if(bytes!=state_->bytes)Detail::Fail("read-only source immutable bytes changed");
        ReadOnlyDetail::ValidateIdentity(*state_);return state_->bytes;
    }
    template<class Admission>const Bytes& ReadAdmitted(Admission&&) const &&=delete;
    template<class Admission>
    void RevalidateAdmitted(const std::filesystem::path& supplied,Admission&& admitAbsolutePeakBytes) const {
        if(!state_)Detail::Fail("read-only source is not bound");
        ReadOnlyDetail::CheckExplicit(supplied);
        const auto pathScratch=ReadOnlyDetail::PathWorkspace(supplied);
        admitAbsolutePeakBytes(pathScratch);
        const auto normalized=Detail::Absolute(supplied);
        if(!Detail::SamePath(normalized,state_->path))Detail::Fail("read-only source cannot replace its binding");
        static_cast<void>(ReadAdmitted([&](std::size_t bytes){admitAbsolutePeakBytes(pathScratch+bytes);}));
    }
    const std::filesystem::path& Path() const {
        if(!state_)Detail::Fail("read-only source is not bound");
        return state_->path;
    }
    bool Bound() const noexcept{return bool(state_);}
    std::size_t RetainedBytes() const noexcept{return sizeof(*this)+(state_?ReadOnlyDetail::Retained(*state_):0u);}
private:
    std::shared_ptr<ReadOnlyDetail::State> state_;
};
} // namespace QuestVr::ConfigFile
