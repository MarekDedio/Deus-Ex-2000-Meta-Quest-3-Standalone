#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace QuestVr::Vm {
// Whole native INT-array payload. Scalar/Struct Values own no vector container.
// Copy is deep, just like std::vector; move preserves capacity and ownership.
// This is storage only: no new script indexing, comparison or array native.
class IntArrayStorage {
public:
    using Storage=std::vector<std::int32_t>;
    using const_iterator=Storage::const_iterator;
    IntArrayStorage()=default;
    IntArrayStorage(const IntArrayStorage& other) {
        if(other.storage_)storage_=std::make_unique<Storage>(*other.storage_);
    }
    IntArrayStorage(IntArrayStorage&&) noexcept=default;
    IntArrayStorage& operator=(const IntArrayStorage& other) {
        if(this!=&other) {IntArrayStorage copied(other);storage_.swap(copied.storage_);}
        return *this;
    }
    IntArrayStorage& operator=(IntArrayStorage&&) noexcept=default;
    IntArrayStorage& operator=(Storage values) {
        if(values.capacity())storage_=std::make_unique<Storage>(std::move(values));
        else storage_.reset();
        return *this;
    }
    bool empty() const noexcept {return !storage_||storage_->empty();}
    std::size_t size() const noexcept {return storage_?storage_->size():0u;}
    std::size_t capacity() const noexcept {return storage_?storage_->capacity():0u;}
    std::size_t ContainerBytes() const noexcept {return storage_?sizeof(Storage):0u;}
    void reserve(std::size_t capacity) {if(capacity)Ensure().reserve(capacity);}
    void push_back(std::int32_t value) {Ensure().push_back(value);}
    void clear() noexcept {if(storage_)storage_->clear();}
    Storage Take() && noexcept {
        Storage result;if(storage_)result=std::move(*storage_);
        storage_.reset();return result;
    }
    const_iterator begin() const noexcept {return Read().begin();}
    const_iterator end() const noexcept {return Read().end();}
    const std::int32_t& operator[](std::size_t index) const noexcept {return Read()[index];}
    std::int32_t& operator[](std::size_t index) {return Ensure()[index];}
    operator const Storage&() const noexcept {return Read();}
    bool operator==(const IntArrayStorage& other) const {return Read()==other.Read();}
    friend bool operator==(const IntArrayStorage& a,const Storage& b) {return a.Read()==b;}
private:
    Storage& Ensure() {if(!storage_)storage_=std::make_unique<Storage>();return *storage_;}
    const Storage& Read() const noexcept {static const Storage empty;return storage_?*storage_:empty;}
    std::unique_ptr<Storage> storage_;
};
} // namespace QuestVr::Vm
