#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

// Nothrow versions of std::make_unique. Return nullptr on allocation failure
// instead of calling abort() (the default when exceptions are disabled on ESP32).
//
// Single object:
//   auto obj = makeUniqueNoThrow<PNG>();
//   if (!obj) { LOG_ERR("TAG", "OOM"); return false; }
//
// Array:
//   auto buf = makeUniqueNoThrow<uint8_t[]>(size);
//   if (!buf) { LOG_ERR("TAG", "OOM"); return false; }
//   buf[0] = 0xFF;
//   someApi(buf.get(), size);
//

template <typename T, typename... Args>
  requires(!std::is_array_v<T>)
std::unique_ptr<T> makeUniqueNoThrow(Args&&... args) {
  return std::unique_ptr<T>(new (std::nothrow) T(std::forward<Args>(args)...));
}

template <typename T>
  requires std::is_unbounded_array_v<T>
std::unique_ptr<T> makeUniqueNoThrow(size_t count) {
  using Elem = std::remove_extent_t<T>;
  return std::unique_ptr<T>(new (std::nothrow) Elem[count]());
}

// Helper struct to call a cleanup function on exit from any scope.
// Use with a lambda to avoid unnecessary allocations from std::function/std::bind:
// Example:
//   auto jpeg = makeUniqueNoThrow<JPEGDEC>();
//   ScopedCleanup cleanup{[&jpeg]{ jpeg->close(); }};
//
template <typename F>
struct [[nodiscard]] ScopedCleanup final {
  const F fn;
  explicit ScopedCleanup(F f) : fn{std::move(f)} {}
  ScopedCleanup(const ScopedCleanup&) = delete;
  ScopedCleanup& operator=(const ScopedCleanup&) = delete;
  ScopedCleanup(ScopedCleanup&&) = delete;
  ScopedCleanup& operator=(ScopedCleanup&&) = delete;
  ~ScopedCleanup() { fn(); }
};

template <typename F>
ScopedCleanup(F) -> ScopedCleanup<F>;

// Heap-backed bump allocator for short-lived scratch buffers that all share one
// lifetime. Allocate one bounded block, hand out aligned slices, then reset or
// destroy the arena all at once. It intentionally does not run destructors; use
// it only for trivially destructible scratch data such as byte or integer arrays.
class ScratchArena final {
 public:
  explicit ScratchArena(const size_t capacityBytes)
      : storage_(makeUniqueNoThrow<uint8_t[]>(capacityBytes)), capacity_(storage_ ? capacityBytes : 0), used_(0) {}

  ScratchArena(const ScratchArena&) = delete;
  ScratchArena& operator=(const ScratchArena&) = delete;
  ScratchArena(ScratchArena&&) = delete;
  ScratchArena& operator=(ScratchArena&&) = delete;

  bool available() const { return storage_ != nullptr; }
  size_t capacity() const { return capacity_; }
  size_t used() const { return used_; }
  size_t remaining() const { return capacity_ - used_; }

  void reset() { used_ = 0; }
  size_t mark() const { return used_; }
  void rewind(const size_t mark) {
    if (mark <= used_) used_ = mark;
  }

  void* allocateBytes(const size_t size, const size_t alignment = alignof(std::max_align_t)) {
    if (!storage_ || size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) return nullptr;

    const uintptr_t base = reinterpret_cast<uintptr_t>(storage_.get());
    const uintptr_t current = base + used_;
    const uintptr_t aligned = (current + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
    const size_t padding = static_cast<size_t>(aligned - current);

    if (padding > remaining() || size > remaining() - padding) return nullptr;

    used_ += padding + size;
    return reinterpret_cast<void*>(aligned);
  }

  template <typename T>
  T* allocateArray(const size_t count) {
    static_assert(std::is_trivially_destructible_v<T>, "ScratchArena only supports trivially destructible data");
    if (count > SIZE_MAX / sizeof(T)) return nullptr;
    return static_cast<T*>(allocateBytes(count * sizeof(T), alignof(T)));
  }

  template <typename T>
  T* allocateZeroedArray(const size_t count) {
    T* ptr = allocateArray<T>(count);
    if (ptr) memset(ptr, 0, count * sizeof(T));
    return ptr;
  }

 private:
  std::unique_ptr<uint8_t[]> storage_;
  size_t capacity_;
  size_t used_;
};
