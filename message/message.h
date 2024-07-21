#pragma once

#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace bookproj {

// message struct without undefined or implementation-defined behavior
// includes following features
// 1. variable length-ed array at end
// 2. support for bit field
// 3. support for endianess conversion

// requirements for message struct
// 1. standard-layout, use composition for subobjects instead of inheritance
// 2. trivially-copyable
// 3. minimal alignment for all fields (1 byte)
// 4. no padding (implied by 3, and compiler should generate error if padding exists)
// 5. access typed fields via accessors and bit_cast

template <typename T, std::endian> struct Field;

namespace detail {

template <typename T>
concept CharLike = std::same_as<std::remove_const_t<T>, char> ||
                   std::same_as<std::remove_const_t<T>, signed char> ||
                   std::same_as<std::remove_const_t<T>, unsigned char> ||
                   std::same_as<std::remove_const_t<T>, std::byte>;

// non-array field type
template <typename T>
concept BasicField =
    std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T> && (!std::is_array_v<T>);

// A array field may be fixed size 1-dimentional array of char-like types, but not int[8] or char[]
// or char [4][4]. Support for int[8] is unnecessary since one can use Field<int>[8] instead.
// Field<char[]> is problematic since gcc rejects flexible array being the sole member of a class.
// One can use Field<char>[] instead, if not plain char[]. char[4][4] is unnecessary and
// superfluous.
template <typename T>
concept CharArrayField =
    std::is_array_v<T> && CharLike<std::remove_extent_t<T>> && std::extent_v<T> > 0;

// the given type is an instantiation of the Field class template
template <typename T> struct IsFieldTyped : std::false_type {};

template <typename T, std::endian endianess>
struct IsFieldTyped<bookproj::Field<T, endianess>> : std::true_type {};

// the given type has an integral Field typed member named size
template <typename T>
concept WithSizeField =
    IsFieldTyped<std::remove_const_t<decltype(std::declval<T>().size)>>::value &&
    std::is_integral_v<typename decltype(std::declval<T>().size)::BaseType>;

// the given type has a header member with an integral Field typed member named size
template <typename T>
concept WithSizeInHeader = WithSizeField<decltype(std::declval<T>().header)>;

// GetArraySize returns the total number of Field elements in a message field
template <typename T> struct GetArraySize {
  static_assert(false, "GetArraySize only supports one-dimensional array fields such as "
                       "Field<T>[N] or Field<T>[]");
};

// specialization for 1-d array, U[N] or U[]
template <typename T>
  requires(std::rank_v<T> == 1)
struct GetArraySize<T> : std::integral_constant<size_t, std::extent_v<T>> {};

} // namespace detail

// Field type within a message
// @tparam T type of the underlying field
template <typename T, std::endian endianess = std::endian::native> struct Field {
  static_assert(false, "Type T not supported for class template Field");
};

// specialization for basic scalar typed fields
template <detail::BasicField T, std::endian endianess> struct Field<T, endianess> {
  using BaseType = T;

  constexpr Field() noexcept = default;
  constexpr Field(const T &t) noexcept { set(t); }

  constexpr operator T() const { return get(); }

  constexpr Field &operator=(const T &t) {
    set(t);
    return *this;
  }

  constexpr T value() const { return get(); }

  constexpr T operator+() const { return get(); }

private:
  std::byte field[sizeof(T)] = {};

  constexpr T get() const {
    if constexpr (std::endian::native != endianess) {
      if constexpr (std::is_integral_v<T>) {
        return std::byteswap(std::bit_cast<T>(field));
      } else {
        std::byte tmp[sizeof(T)];
        for (size_t i = 0; i < sizeof(T); ++i) {
          tmp[i] = field[sizeof(T) - 1 - i];
        }
        return std::bit_cast<T>(tmp);
      }
    } else {
      return std::bit_cast<T>(field);
    }
  }

  constexpr void set(const T &t) {
    std::memcpy(field, &t, sizeof(T));
    if constexpr (std::endian::native != endianess) {
      for (size_t i = 0; i < sizeof(T) / 2; ++i) {
        std::swap(field[i], field[sizeof(T) - 1 - i]);
      }
    }
  }

  template <typename Msg, typename Fld> friend struct FieldReader;
};

template <detail::CharArrayField Arr, std::endian endianess>
struct Field<Arr, endianess> : public std::array<std::remove_extent_t<Arr>, std::extent_v<Arr>> {
  static constexpr size_t N = std::extent_v<Arr>;
  using BaseType = Arr;
  using T = std::remove_extent_t<Arr>;

  Field() : std::array<T, N>{} {};

  Field(std::initializer_list<T> l) : std::array<T, N>(l) {}

  template <detail::CharLike U> constexpr Field(const std::array<U, N> &t) noexcept {
    std::memcpy(this->data(), t.data(), N);
  }

  template <detail::CharLike U> constexpr Field(const U (&t)[N]) noexcept {
    std::memcpy(this->data(), t, N);
  }

  constexpr std::span<const T, N> get() const {
    return std::span<const T, N>(std::array<T, N>::data(), N);
  }
};

template <typename Derived> struct MessageBase;

// read-only accessor for a single Field
// @tparam Msg message type, must be derived from MessageBase
// @tparam Fld field type, must be of Field<T, bool> form
template <typename Msg, typename Fld> struct FieldReader {
  using BaseType = typename Fld::BaseType;
  using T = std::remove_extent_t<BaseType>;
  static constexpr size_t N = std::extent_v<BaseType>;
  static_assert(std::derived_from<Msg, MessageBase<Msg>>);
  static_assert(detail::IsFieldTyped<Fld>::value);

  constexpr auto value() const {
    if (has_value()) {
      return ptr->get();
    }
    throw std::bad_optional_access{};
  }

  constexpr T operator[](size_t i) const
    requires(std::is_array_v<BaseType>)
  {
    return value()[i];
  }

  constexpr auto operator*() const { return value(); }
  constexpr operator bool() const { return has_value(); }

  constexpr bool has_value() const { return ptr != nullptr; }

  template <typename U>
  constexpr T value_or(U &&default_value) const
    requires(!std::is_array_v<BaseType>)
  {
    return has_value() ? value() : static_cast<T>(std::forward<U>(default_value));
  }

  template <typename U>
  constexpr T value_at_or(size_t i, U &&default_value) const
    requires(std::is_array_v<BaseType>)
  {
    return (has_value() && i < N) ? value()[i] : static_cast<T>(std::forward<U>(default_value));
  }

  // constructor is private so that we can check if the field is available here
  static constexpr FieldReader<Msg, Fld> getReader(Msg &msg, size_t offset) {
    return FieldReader<Msg, Fld>{
        offset + sizeof(BaseType) <= msg.getMsgSize() ? msg.asBytes().data() + offset : nullptr};
  }

private:
  const Fld *ptr;

  constexpr FieldReader(const std::byte *ptr_) noexcept
      : ptr(std::launder(reinterpret_cast<const Fld *>(ptr_))) {}

  template <typename M, typename F> friend struct FieldRange;
};

// range accessor for array of fields, e.g. Field<T>[10] or Fld<T>[]
template <typename Msg, typename Fld> struct FieldRange {
  using BaseType = typename Fld::BaseType;
  static_assert(std::derived_from<Msg, MessageBase<Msg>>);

  struct Iterator {
    using value_type = FieldReader<Msg, Fld>;
    using difference_type = ptrdiff_t;
    using pointer = void;
    using reference = FieldReader<Msg, Fld>;
    using iterator_category = std::random_access_iterator_tag;

    constexpr Iterator() noexcept = default;
    constexpr Iterator(const std::byte *ptr_) noexcept : ptr(ptr_) {}

    constexpr reference operator*() const noexcept { return {ptr}; }

    constexpr Iterator &operator++() noexcept {
      ptr += sizeof(BaseType);
      return *this;
    }

    constexpr Iterator operator++(int) noexcept {
      auto copy = *this;
      ptr += sizeof(BaseType);
      return copy;
    }

    constexpr Iterator &operator--() noexcept {
      ptr -= sizeof(BaseType);
      return *this;
    }

    constexpr Iterator operator--(int) noexcept {
      auto copy = *this;
      ptr -= sizeof(BaseType);
      return copy;
    }

    constexpr Iterator &operator+=(difference_type n) noexcept {
      ptr += n * sizeof(BaseType);
      return *this;
    }

    constexpr Iterator &operator-=(difference_type n) noexcept {
      ptr -= n * sizeof(BaseType);
      return *this;
    }

    constexpr Iterator operator+(difference_type n) const noexcept {
      return Iterator{ptr + n * sizeof(BaseType)};
    }

    constexpr Iterator operator-(difference_type n) const noexcept {
      return Iterator{ptr - n * sizeof(BaseType)};
    }

    constexpr difference_type operator-(const Iterator &rhs) const noexcept {
      return (ptr - rhs.ptr) / sizeof(BaseType);
    }

    constexpr auto operator<=>(const Iterator &rhs) const noexcept = default;

  private:
    const std::byte *ptr;
  };

  constexpr FieldRange(Msg &msg_, size_t offset_, size_t extent_) noexcept
      : ptr(msg_.asBytes().data() + offset_),
        extent(calcExtent(msg_.getMsgSize(), offset_, extent_)) {}

  constexpr Iterator begin() const { return {ptr}; }
  constexpr Iterator end() const { return {ptr + extent * sizeof(BaseType)}; }

  constexpr size_t size() const { return extent; }

  constexpr bool empty() const { return extent == 0; }
  constexpr auto operator[](size_t i) const {
    if (i < extent) {
      return *(begin() + i);
    }
    throw std::out_of_range{"FieldRange index out of range"};
  }

private:
  static constexpr size_t calcExtent(size_t size, size_t offset, size_t extent_) {
    if (extent_ == 0 && offset < size) {
      extent_ = (size - offset) / sizeof(BaseType);
    }
    return extent_ * sizeof(BaseType) + offset <= size ? extent_ : 0;
  }

  const std::byte *ptr;
  size_t extent;
};

// FIELD_RANGE macro is for read access of array fields, such as Field<T> f[size].  It returns a
// FiendRange object, which is a range with proper size based on the extend of the field array. If
// the field is flexible lengthed, it further checks the size of the message and
// adjusts the range accordingly.  The range is iterable, and each element is a FieldReader object.
#define FIELD_RANGE(msg, field)                                                                   \
  FieldRange<std::remove_reference_t<decltype(msg)>,                                              \
             std::remove_all_extents_t<std::remove_reference_t<decltype(msg.field)>>> {           \
    msg, reinterpret_cast<uintptr_t>(&msg.field) - reinterpret_cast<uintptr_t>(&msg),             \
        detail::GetArraySize<std::remove_reference_t<decltype(msg.field)>>::value                 \
  }

// Base message class using CRTP.  This class is useful for messages with non-primeval fields,
// and potentially for those with nested fields.
// This should not be used for a project without primeval fields or nested fields.
template <typename Derived> struct MessageBase {
  static constexpr size_t StaticSize = sizeof(Derived);

  constexpr MessageBase() noexcept {
    // Cannot use these as constraints for class template parameter type,
    // because at the point of declaration "struct Derived :
    // MessageBase<Derived>", Derived is not complete, therefore
    // constraints cannot be applied to it
    // derived should have a size field, or has a header member with a size field
    static_assert((std::derived_from<Derived, MessageBase> &&
                   (detail::WithSizeField<Derived> || detail::WithSizeInHeader<Derived>)) &&
                  std::alignment_of_v<Derived> == 1 && std::is_standard_layout_v<Derived> &&
                  std::is_trivially_copyable_v<Derived>);
  }

  constexpr size_t getMsgSize() const {
    if constexpr (detail::WithSizeField<Derived>) {
      return static_cast<const Derived *>(this)->size;
    } else {
      return static_cast<const Derived *>(this)->header.size;
    }
  }

  constexpr void setMsgSize(size_t size) {
    if constexpr (detail::WithSizeField<Derived>) {
      static_cast<Derived *>(this)->size = size;
    } else {
      static_cast<Derived *>(this)->header.size = size;
    }
  }

  std::span<const std::byte> asBytes() const {
    return std::span(reinterpret_cast<const std::byte *>(this), getMsgSize());
  }

  std::span<std::byte> asWritableBytes() {
    return std::span(reinterpret_cast<std::byte *>(this), getMsgSize());
  }
};

} // namespace bookproj
