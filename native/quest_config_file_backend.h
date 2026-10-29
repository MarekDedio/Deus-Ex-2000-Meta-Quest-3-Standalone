#pragma once

#include "quest_save_bundle.h"

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>
#ifndef _WIN32
#include <sys/file.h>
#endif

namespace QuestVr::ConfigFile {

using Bytes=std::vector<std::uint8_t>;
inline constexpr std::size_t MaximumBytes=1u<<20u;
enum class PublicationMode {RollbackOnly,CommitAllowed};
enum class Disposition {NotPublished,PublishedDurabilityUnknown,Durable};
struct Result {
    Disposition disposition{Disposition::NotPublished};
    std::uint64_t generation{};
    bool Published() const noexcept {return disposition!=Disposition::NotPublished;}
};
enum class FaultPoint {CreateStage,WriteStage,FlushStage,Readback,BeforePublish,AfterPublish,DirectoryFlush};
// Explicit test/I/O fault boundary. A post-rename failure NEVER means rollback.
struct Faults {
    void* context{};
    bool (*allow)(void*,FaultPoint) noexcept{};
    bool Allow(FaultPoint point) const noexcept {return !allow||allow(context,point);}
};

namespace Detail {
[[noreturn]] inline void Fail(const char* message) {throw std::runtime_error(std::string("Owned config: ")+message);}
#ifdef _WIN32
using Handle=HANDLE;
inline const Handle Invalid=INVALID_HANDLE_VALUE;
#else
using Handle=int;
inline constexpr Handle Invalid=-1;
#endif
class File {
public:
    File()=default;explicit File(Handle value):value_(value){}
    File(const File&)=delete;File& operator=(const File&)=delete;
    File(File&& other) noexcept:value_(std::exchange(other.value_,Invalid)){}
    File& operator=(File&& other) noexcept {if(this!=&other){Close();value_=std::exchange(other.value_,Invalid);}return *this;}
    ~File(){Close();}
    Handle Get() const noexcept {return value_;}
    explicit operator bool() const noexcept {return value_!=Invalid;}
    bool Close() noexcept {
        if(!*this)return true;
        const auto value=std::exchange(value_,Invalid);
#ifdef _WIN32
        return CloseHandle(value)!=0;
#else
        return close(value)==0; // Never retry close(EINTR) against a reused fd.
#endif
    }
private:Handle value_{Invalid};
};
struct Stamp {
    std::uint64_t device{},index{},size{},modified{},modifiedPart{},changed{},changedPart{};
    bool operator==(const Stamp&) const=default;
};
inline std::filesystem::path Absolute(const std::filesystem::path& supplied) {
    if(supplied.empty()||supplied.native().size()>32000u||supplied.native().find(std::filesystem::path::value_type{})!=supplied.native().npos)
        Fail("requires a bounded explicit path without NUL");
    return std::filesystem::absolute(supplied).lexically_normal();
}
inline void CheckComponents(const std::filesystem::path& path) {
    auto current=path.root_path();std::size_t depth{};
    const auto check=[&] {
        if(++depth>128u)Fail("path component depth exceeds its bound");
        std::error_code error;
        const auto status=std::filesystem::symlink_status(current,error);
        if(error||!std::filesystem::exists(status)||std::filesystem::is_symlink(status))Fail("path component is missing or a symbolic link");
#ifdef _WIN32
        const auto attributes=GetFileAttributesW(current.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))Fail("path component is a reparse point");
#endif
    };
    check();for(const auto& component:path.relative_path()){current/=component;check();}
}
inline bool SamePath(const std::filesystem::path& left,const std::filesystem::path& right) {
#ifdef _WIN32
    const auto& a=left.native();const auto& b=right.native();
    return CompareStringOrdinal(a.data(),static_cast<int>(a.size()),b.data(),static_cast<int>(b.size()),TRUE)==CSTR_EQUAL;
#else
    return left==right;
#endif
}
inline bool Beneath(const std::filesystem::path& child,const std::filesystem::path& parent) {
    auto a=child.begin(),b=parent.begin();
    for(;b!=parent.end();++a,++b)if(a==child.end()||!SamePath(*a,*b))return false;
    return true;
}
inline Stamp FileStamp(const File& file,std::size_t maximum) {
    Stamp stamp;
#ifdef _WIN32
    BY_HANDLE_FILE_INFORMATION info{};
    if(!file||GetFileType(file.Get())!=FILE_TYPE_DISK||!GetFileInformationByHandle(file.Get(),&info)||info.nNumberOfLinks!=1u||
        (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))Fail("file is not a single-link regular file");
    stamp.device=info.dwVolumeSerialNumber;stamp.index=(std::uint64_t(info.nFileIndexHigh)<<32u)|info.nFileIndexLow;
    stamp.size=(std::uint64_t(info.nFileSizeHigh)<<32u)|info.nFileSizeLow;
    stamp.modified=(std::uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32u)|info.ftLastWriteTime.dwLowDateTime;
    stamp.changed=(std::uint64_t(info.ftCreationTime.dwHighDateTime)<<32u)|info.ftCreationTime.dwLowDateTime;
#else
    struct stat info{};
    if(!file||fstat(file.Get(),&info)!=0||!S_ISREG(info.st_mode)||info.st_nlink!=1||info.st_size<0)Fail("file is not a single-link regular file");
    stamp.device=static_cast<std::uint64_t>(info.st_dev);stamp.index=static_cast<std::uint64_t>(info.st_ino);stamp.size=static_cast<std::uint64_t>(info.st_size);
#if defined(__APPLE__)
    stamp.modified=static_cast<std::uint64_t>(info.st_mtimespec.tv_sec);stamp.modifiedPart=static_cast<std::uint64_t>(info.st_mtimespec.tv_nsec);
    stamp.changed=static_cast<std::uint64_t>(info.st_ctimespec.tv_sec);stamp.changedPart=static_cast<std::uint64_t>(info.st_ctimespec.tv_nsec);
#else
    stamp.modified=static_cast<std::uint64_t>(info.st_mtim.tv_sec);stamp.modifiedPart=static_cast<std::uint64_t>(info.st_mtim.tv_nsec);
    stamp.changed=static_cast<std::uint64_t>(info.st_ctim.tv_sec);stamp.changedPart=static_cast<std::uint64_t>(info.st_ctim.tv_nsec);
#endif
#endif
    if(stamp.size>maximum)Fail("file byte bound exceeded");
    return stamp;
}
// Admission receives the actual held-descriptor payload size before its
// vector allocation. It must throw to refuse; no caller-supplied cached length
// or pathname size can authorize this allocation. The same before/after stamp
// and explicit truncation/growth checks apply when the callback changes a file.
template<class Admission>
Bytes Read(const File& file,std::size_t maximum,Admission&& admit) {
    const auto before=FileStamp(file,maximum);admit(static_cast<std::size_t>(before.size));
    Bytes result(static_cast<std::size_t>(before.size));
    std::size_t offset{};
#ifdef _WIN32
    LARGE_INTEGER zero{};if(!SetFilePointerEx(file.Get(),zero,nullptr,FILE_BEGIN))Fail("cannot seek file");
    while(offset<result.size()) {
        DWORD count{};if(!ReadFile(file.Get(),result.data()+offset,static_cast<DWORD>(result.size()-offset),&count,nullptr)||!count)Fail("file read failed/truncated");
        offset+=count;
    }
    std::uint8_t extra{};DWORD count{};
    if(!ReadFile(file.Get(),&extra,1u,&count,nullptr)||count)Fail("file grew while reading");
#else
    while(offset<result.size()) {
        const auto count=pread(file.Get(),result.data()+offset,result.size()-offset,static_cast<off_t>(offset));
        if(count<0&&errno==EINTR)continue;
        if(count<=0)Fail("file read failed/truncated");
        offset+=static_cast<std::size_t>(count);
    }
    std::uint8_t extra{};ssize_t count;
    do {count=pread(file.Get(),&extra,1u,static_cast<off_t>(result.size()));}while(count<0&&errno==EINTR);
    if(count!=0)Fail("file grew/unreadable while reading");
#endif
    if(FileStamp(file,maximum)!=before)Fail("file changed while reading");
    return result;
}
inline Bytes Read(const File& file,std::size_t maximum) {
    return Read(file,maximum,[](std::size_t){});
}
inline bool Write(const File& file,const Bytes& bytes) noexcept {
    std::size_t offset{};
    while(offset<bytes.size()) {
#ifdef _WIN32
        DWORD count{};
        if(!WriteFile(file.Get(),bytes.data()+offset,static_cast<DWORD>(bytes.size()-offset),&count,nullptr)||!count)return false;
        offset+=count;
#else
        const auto count=write(file.Get(),bytes.data()+offset,bytes.size()-offset);
        if(count<0&&errno==EINTR)continue;
        if(count<=0)return false;
        offset+=static_cast<std::size_t>(count);
#endif
    }
    return true;
}
inline bool Flush(const File& file) noexcept {
#ifdef _WIN32
    return FlushFileBuffers(file.Get())!=0;
#else
    return fsync(file.Get())==0;
#endif
}
struct State {
    std::filesystem::path application,seedPath,seedName,directory,target,stage,lockPath;
    File applicationDirectory,directoryHandle,seedDirectory,seed;
#ifdef _WIN32
    // Deny DELETE sharing on every directory component so full-path Win32 leaf
    // operations cannot be redirected by renaming/replacing a pinned ancestor.
    std::vector<File> ancestors;
#endif
    Bytes seedBytes;Stamp seedStamp;std::size_t maximum{};std::uint64_t generation{};std::mutex mutex;
};
#ifdef _WIN32
inline File PinDirectory(const std::filesystem::path& path,bool writable=false) {
    File file(CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES|FILE_TRAVERSE|(writable?FILE_ADD_FILE|FILE_ADD_SUBDIRECTORY:0u),
        FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    BY_HANDLE_FILE_INFORMATION info{};
    if(!file||!GetFileInformationByHandle(file.Get(),&info)||!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||
        (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))Fail("cannot pin a non-reparse directory");
    return file;
}
inline File OpenLeaf(const State& state,const std::filesystem::path& path,bool create=false,bool writable=false,bool lock=false,bool stageReader=false) {
    const auto access=GENERIC_READ|(writable?GENERIC_WRITE:0u);
    File file(CreateFileW(path.c_str(),access,lock?0u:FILE_SHARE_READ|FILE_SHARE_DELETE|(stageReader?FILE_SHARE_WRITE:0u),nullptr,
        create?(lock?OPEN_ALWAYS:CREATE_NEW):OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|(writable?FILE_FLAG_WRITE_THROUGH:0u),nullptr));
    if(!file) {
        const auto error=GetLastError();if(!create&&(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND))return {};
        Fail("cannot open fixed owned leaf");
    }
    static_cast<void>(FileStamp(file,state.maximum));
    return file;
}
inline File OpenTarget(const State& state){return OpenLeaf(state,state.target);}
inline File OpenStage(const State& state,bool create=false){return OpenLeaf(state,state.stage,create,create,false,!create);}
inline File AcquireLock(const State& state){return OpenLeaf(state,state.lockPath,true,true,true);}
#else
inline File PinDirectory(const std::filesystem::path& path,bool writable=false) {
    File current(open(path.root_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
    if(!current)Fail("cannot pin filesystem root");
    for(const auto& component:path.relative_path()) {
        File next(openat(current.Get(),component.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        if(!next)Fail("cannot pin a non-symlink directory");
        current=std::move(next);
    }
    struct stat info{};
    if(fstat(current.Get(),&info)!=0||!S_ISDIR(info.st_mode)||(writable&&(info.st_uid!=geteuid()||(info.st_mode&0022u)||!(info.st_mode&S_IWUSR))))
        Fail("owned directory is not private to its current owner");
    return current;
}
inline File OpenLeaf(const State& state,const char* name,bool create=false,bool writable=false,bool lock=false) {
    const auto flags=(writable?O_RDWR:O_RDONLY)|O_CLOEXEC|O_NOFOLLOW|(create?(lock?O_CREAT:O_CREAT|O_EXCL):0);
    File file(openat(state.directoryHandle.Get(),name,flags,0600));
    if(!file){if(!create&&errno==ENOENT)return {};Fail("cannot open fixed owned leaf");}
    static_cast<void>(FileStamp(file,state.maximum));
    struct stat info{};if(fstat(file.Get(),&info)!=0||info.st_uid!=geteuid()||(info.st_mode&0022u))Fail("owned leaf is writable by another principal");
    if(lock&&flock(file.Get(),LOCK_EX|LOCK_NB)!=0)Fail("another config publication owns the lock");
    return file;
}
inline File OpenTarget(const State& state){return OpenLeaf(state,"User.ini");}
inline File OpenStage(const State& state,bool create=false){return OpenLeaf(state,"User.ini.stage",create,create);}
inline File AcquireLock(const State& state){return OpenLeaf(state,"User.ini.lock",true,true,true);}
#endif
template<class Admission>
void ValidateSeed(const State& state,Admission&& admit) {
#ifndef _WIN32
    File named(openat(state.seedDirectory.Get(),state.seedName.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW));
    if(!named||FileStamp(named,state.maximum)!=state.seedStamp)Fail("immutable seed name/identity changed");
#endif
    if(FileStamp(state.seed,state.maximum)!=state.seedStamp||Read(state.seed,state.maximum,admit)!=state.seedBytes)Fail("immutable seed changed");
}
inline void ValidateSeed(const State& state) {
    ValidateSeed(state,[](std::size_t){});
}
inline bool RemoveStage(const State& state,const Stamp& stamp) noexcept {
    try {
        auto file=OpenStage(state);
        if(!file)return false;
        const auto actual=FileStamp(file,state.maximum);
        if(actual.device!=stamp.device||actual.index!=stamp.index)return false;
        if(!file.Close())return false;
#ifdef _WIN32
        return DeleteFileW(state.stage.c_str())!=0;
#else
        return unlinkat(state.directoryHandle.Get(),"User.ini.stage",0)==0;
#endif
    }catch(...){return false;}
}
} // namespace Detail

class Backend;
struct Snapshot {
    const Bytes& Data() const noexcept {static const Bytes empty;return bytes_?*bytes_:empty;}
    bool Owned() const noexcept {return owned_;}
    std::uint64_t Generation() const noexcept {return generation_;}
    std::size_t RetainedBytes() const noexcept {return sizeof(*this)+(bytes_?sizeof(Bytes)+bytes_->capacity()+4u*sizeof(void*):0u);}
private:
    friend class Backend;
    friend class Prepared;
    std::shared_ptr<const Detail::State> authority_;
    std::shared_ptr<const Bytes> bytes_;
    Detail::Stamp stamp_;
    std::uint64_t generation_{};bool owned_{};
};

class Prepared {
public:
    Prepared()=default;Prepared(const Prepared&)=delete;Prepared& operator=(const Prepared&)=delete;
    Prepared(Prepared&&)=default;
    Prepared& operator=(Prepared&& other) noexcept {if(this!=&other){Cancel();data_=std::move(other.data_);}return *this;}
    ~Prepared(){Cancel();}
    // No allocation and no throwing rollback decision after a successful rename.
    Result Publish() noexcept {
        if(!data_)return {};
        if(!data_->stageOwned)return data_->lastResult;
        auto& value=*data_;Result result{Disposition::NotPublished,value.state->generation};
        try {
            Detail::ValidateSeed(*value.state);
            auto current=Detail::OpenTarget(*value.state);
            if(bool(current)!=value.expected.owned_||value.expected.generation_!=value.state->generation||
                (current&&(Detail::FileStamp(current,value.state->maximum)!=value.expected.stamp_||Detail::Read(current,value.state->maximum)!=value.expected.Data())))return result;
            // Release the validation handle before replacing the destination
            // on Windows. The held writer lock still excludes cooperating
            // writers; descriptor/content checks above remain pre-publication.
            if(!current.Close())return result;
            auto stage=Detail::OpenStage(*value.state);
            if(!stage||Detail::FileStamp(stage,value.state->maximum)!=value.stageStamp||!stage.Close()||!value.stageFile.Close()||
                !value.faults.Allow(FaultPoint::BeforePublish))return result;
#ifdef _WIN32
            const bool renamed=MoveFileExW(value.state->stage.c_str(),value.state->target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
            const bool renamed=renameat(value.state->directoryHandle.Get(),"User.ini.stage",value.state->directoryHandle.Get(),"User.ini")==0;
#endif
            if(!renamed)return result;
            // Publication is irreversible here. All subsequent paths MUST report
            // Published, including simulated/real directory-flush failures.
            value.stageOwned=false;++value.state->generation;result.generation=value.state->generation;
            result.disposition=Disposition::PublishedDurabilityUnknown;
            value.lastResult=result;
            if(!value.faults.Allow(FaultPoint::AfterPublish)||!value.faults.Allow(FaultPoint::DirectoryFlush))return result;
#ifndef _WIN32
            if(fsync(value.state->directoryHandle.Get())!=0||fsync(value.state->applicationDirectory.Get())!=0)return result;
#endif
            result.disposition=Disposition::Durable;value.lastResult=result;return result;
        }catch(...){return result;}
    }
    void Cancel() noexcept {
        if(!data_)return;
        data_->stageFile.Close();if(data_->stageOwned)Detail::RemoveStage(*data_->state,data_->stageStamp);
        data_.reset(); // Release process/native writer locks, never the target.
    }
    std::size_t RetainedBytes() const noexcept {return data_?sizeof(Data):0u;}
private:
    friend class Backend;
    struct Data {
        std::shared_ptr<Detail::State> state;std::unique_lock<std::mutex> mutex;
        Detail::File writerLock,stageFile;Snapshot expected;Detail::Stamp stageStamp;Faults faults;Result lastResult;bool stageOwned{};
        Data(std::shared_ptr<Detail::State> supplied,Snapshot snapshot,Faults injected):state(std::move(supplied)),
            mutex(state->mutex,std::try_to_lock),expected(std::move(snapshot)),faults(injected) {
            if(!mutex.owns_lock())Detail::Fail("another local config publication owns the backend");
        }
    };
    std::unique_ptr<Data> data_;
};

// Explicit app-private ownership binding. It creates only the fixed Config leaf
// under a supplied existing application-data root. It NEVER changes the seed.
// Backend objects may share a binding; snapshots from another binding refuse.
// Cooperating processes use the fixed lock file. Untrusted code with app-private
// write access must not ignore that contract; no filesystem offers rename-CAS
// against a malicious same-principal writer on both supported platforms.
class Backend {
public:
    static Backend Bind(const std::filesystem::path& applicationDataDirectory,const std::filesystem::path& seedPath,
        std::size_t maximum=MaximumBytes) {
        if(!maximum||maximum>MaximumBytes)Detail::Fail("callers may only tighten the file bound");
        const auto application=Detail::Absolute(applicationDataDirectory),seed=Detail::Absolute(seedPath);
        if(application==application.root_path()||application.filename().empty())Detail::Fail("application root is not a dedicated directory");
        Detail::CheckComponents(application);Detail::CheckComponents(seed);
        auto state=std::make_shared<Detail::State>();state->maximum=maximum;
        // Absolute normalized paths with every component checked/pinned, not a
        // canonical() call that could follow a newly substituted symlink.
        state->application=application;state->seedPath=seed;
        state->seedName=seed.filename();
        state->directory=state->application/"Config";state->target=state->directory/"User.ini";
        state->stage=state->directory/"User.ini.stage";state->lockPath=state->directory/"User.ini.lock";
        if(Detail::Beneath(state->seedPath,state->directory))Detail::Fail("immutable seed cannot be in the writable config directory");
#ifdef _WIN32
        const auto pinAncestors=[&](const std::filesystem::path& supplied) {
            auto current=supplied.root_path();state->ancestors.push_back(Detail::PinDirectory(current));
            for(const auto& component:supplied.relative_path()){current/=component;state->ancestors.push_back(Detail::PinDirectory(current));}
        };
        pinAncestors(state->application);pinAncestors(state->seedPath.parent_path());
        state->applicationDirectory=Detail::PinDirectory(state->application,true);
        state->seed=Detail::File(CreateFileW(state->seedPath.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        if(!CreateDirectoryW(state->directory.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)Detail::Fail("cannot create owned Config leaf");
        state->directoryHandle=Detail::PinDirectory(state->directory,true);
#else
        state->applicationDirectory=Detail::PinDirectory(state->application,true);
        state->seedDirectory=Detail::PinDirectory(state->seedPath.parent_path());
        state->seed=Detail::File(openat(state->seedDirectory.Get(),state->seedName.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW));
        if(mkdirat(state->applicationDirectory.Get(),"Config",0700)!=0&&errno!=EEXIST)Detail::Fail("cannot create owned Config leaf");
        state->directoryHandle=Detail::File(openat(state->applicationDirectory.Get(),"Config",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        struct stat info{};
        if(!state->directoryHandle||fstat(state->directoryHandle.Get(),&info)!=0||!S_ISDIR(info.st_mode)||info.st_uid!=geteuid()||
            (info.st_mode&0022u)||!(info.st_mode&S_IWUSR))Detail::Fail("Config leaf is not a private owned directory");
#endif
        state->seedStamp=Detail::FileStamp(state->seed,maximum);state->seedBytes=Detail::Read(state->seed,maximum);
        auto target=Detail::OpenTarget(*state);
        if(target) {
            const auto stamp=Detail::FileStamp(target,maximum);
            if(stamp.device==state->seedStamp.device&&stamp.index==state->seedStamp.index)Detail::Fail("owned target aliases the immutable seed");
        }
        Backend result;result.state_=std::move(state);return result;
    }
    // Explicit additional-peak admission, excluding this Backend's retained
    // state and the caller's already owned baseline/Snapshots. The callback
    // runs under the same nonblocking backend mutex as validation and reading;
    // it must not recursively acquire this binding. Seed-validation scratch
    // is destroyed before the target or fallback Snapshot bytes are allocated,
    // so successive callbacks are absolute peaks, not additive reservations.
    // Returned Snapshot/control accounting matches Snapshot::RetainedBytes;
    // a target read also covers the temporary Bytes header that coexists with
    // the shared Bytes object while its buffer is moved (never duplicated).
    // Read remains the same unbudgeted API by supplying a no-op callback.
    template<class Admission>
    Snapshot ReadAdmitted(Admission&& admitAbsolutePeakBytes) const {
        if(!state_)Detail::Fail("backend is not bound");
        std::unique_lock lock(state_->mutex,std::try_to_lock);
        if(!lock.owns_lock())Detail::Fail("a config publication owns the backend");
        Detail::ValidateSeed(*state_,[&](std::size_t bytes){
            admitAbsolutePeakBytes(sizeof(Snapshot)+sizeof(Bytes)+bytes);
        });
        Snapshot result;result.authority_=state_;result.generation_=state_->generation;
        auto target=Detail::OpenTarget(*state_);result.owned_=bool(target);
        constexpr auto snapshotControl=sizeof(Snapshot)+sizeof(Bytes)+4u*sizeof(void*);
        if(target) {
            result.stamp_=Detail::FileStamp(target,state_->maximum);
            auto bytes=Detail::Read(target,state_->maximum,[&](std::size_t count){
                admitAbsolutePeakBytes(snapshotControl+sizeof(Bytes)+count);
            });
            // The Snapshot stamp and its bytes must describe the same held
            // file, including a mutation between this stamp and Read's stamp.
            if(Detail::FileStamp(target,state_->maximum)!=result.stamp_)Detail::Fail("target changed before snapshot completion");
            result.bytes_=std::make_shared<const Bytes>(std::move(bytes));
        }else {
            result.stamp_=state_->seedStamp;
            admitAbsolutePeakBytes(snapshotControl+state_->seedBytes.size());
            result.bytes_=std::make_shared<const Bytes>(state_->seedBytes);
        }
        return result;
    }
    Snapshot Read() const {return ReadAdmitted([](std::size_t){});}
    // Explicit startup recovery, never implicit Bind/destructor cleanup. Under
    // both writer locks an abandoned fixed stage is known to be uncommitted;
    // removing it leaves the prior target/seed unchanged. true means currently
    // absent, not a durability promise for this non-authoritative scratch leaf.
    bool DiscardAbandonedStage(PublicationMode mode=PublicationMode::RollbackOnly) const {
        if(mode!=PublicationMode::CommitAllowed)Detail::Fail("rollback-only roots cannot discard a stage");
        if(!state_)Detail::Fail("backend is not bound");
        std::unique_lock lock(state_->mutex,std::try_to_lock);if(!lock.owns_lock())Detail::Fail("a config publication owns the backend");
        auto writer=Detail::AcquireLock(*state_);Detail::ValidateSeed(*state_);auto stage=Detail::OpenStage(*state_);
        if(!stage)return true;
        const auto stamp=Detail::FileStamp(stage,state_->maximum);
        if(!stage.Close()||!Detail::RemoveStage(*state_,stamp))return false;
#ifndef _WIN32
        static_cast<void>(fsync(state_->directoryHandle.Get()));
#endif
        return true;
    }
    // Only call CommitAllowed after the outer VM has accepted its transaction.
    // Rollback probes must never opt in. While Prepared lives it owns both writer
    // locks: do not recursively call Read/Prepare on this binding.
    Prepared Prepare(const Bytes& bytes,const Snapshot& expected,PublicationMode mode=PublicationMode::RollbackOnly,
        Faults faults={}) const {
        if(mode!=PublicationMode::CommitAllowed)Detail::Fail("rollback-only roots cannot stage or publish config");
        if(!state_||expected.authority_.get()!=state_.get()||!expected.bytes_)Detail::Fail("snapshot has no authority for this backend");
        if(bytes.size()>state_->maximum)Detail::Fail("staged bytes exceed their bound");
        Prepared result;result.data_=std::make_unique<Prepared::Data>(state_,expected,faults);auto& value=*result.data_;
        value.writerLock=Detail::AcquireLock(*state_);Detail::ValidateSeed(*state_);
        if(state_->generation==std::numeric_limits<std::uint64_t>::max())Detail::Fail("config generation exceeds its bound");
        if(expected.generation_!=state_->generation)Detail::Fail("stale in-process config generation");
        auto target=Detail::OpenTarget(*state_);
        if(bool(target)!=expected.owned_||(target&&(Detail::FileStamp(target,state_->maximum)!=expected.stamp_||Detail::Read(target,state_->maximum)!=expected.Data())))
            Detail::Fail("owned target changed since its snapshot");
        if(!faults.Allow(FaultPoint::CreateStage))Detail::Fail("injected stage creation failure");
        value.stageFile=Detail::OpenStage(*state_,true);value.stageOwned=true;value.stageStamp=Detail::FileStamp(value.stageFile,state_->maximum);
        if(!faults.Allow(FaultPoint::WriteStage)||!Detail::Write(value.stageFile,bytes))Detail::Fail("stage write failed");
        value.stageStamp=Detail::FileStamp(value.stageFile,state_->maximum);
        if(!faults.Allow(FaultPoint::FlushStage)||!Detail::Flush(value.stageFile))Detail::Fail("stage durability flush failed");
        value.stageStamp=Detail::FileStamp(value.stageFile,state_->maximum);
        if(!faults.Allow(FaultPoint::Readback)||Detail::Read(value.stageFile,state_->maximum)!=bytes)Detail::Fail("stage readback failed");
        return result;
    }
    std::size_t PreparePeakBytes(std::size_t proposed,const Snapshot& expected) const {
        if(!state_||proposed>state_->maximum||expected.authority_.get()!=state_.get())Detail::Fail("invalid preparation budget authority");
        // Sequential seed/current/stage read buffers do not coexist. The
        // caller's proposed vector and Snapshot/Backend retained storage are
        // accounted separately, before entering this fallible preparation.
        return sizeof(Prepared::Data)+2u*std::max({state_->seedBytes.size(),expected.Data().size(),proposed})+4096u;
    }
    std::size_t RetainedBytes() const noexcept {
        if(!state_)return sizeof(*this);
        std::size_t bytes=sizeof(*this)+sizeof(Detail::State)+state_->seedBytes.capacity()+4u*sizeof(void*);
        for(const auto* path:{&state_->application,&state_->seedPath,&state_->seedName,&state_->directory,&state_->target,&state_->stage,&state_->lockPath})
            bytes+=(path->native().capacity()+1u)*sizeof(std::filesystem::path::value_type);
#ifdef _WIN32
        bytes+=state_->ancestors.capacity()*sizeof(Detail::File);
#endif
        return bytes;
    }
    const std::filesystem::path& Directory() const {if(!state_)Detail::Fail("backend is not bound");return state_->directory;}
private:std::shared_ptr<Detail::State> state_;
};

} // namespace QuestVr::ConfigFile
