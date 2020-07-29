#ifndef FEM_STR_HPP
#define FEM_STR_HPP

#include <fem/str_ref.hpp>
#include <fem/zero.hpp>

namespace fem {

  template <int StrLen>
  struct str
  {
    char elems[StrLen+1]; //w add one space for '\0'

    str()
    {
      std::memset(elems, '\0', StrLen);
      elems[StrLen] = '\0';
    }

    str(
      char c)
    {
      std::memset(elems, c, StrLen);
      elems[StrLen] = '\0';
    }

    str(
      char const* s)
    {
      utils::copy_with_blank_padding(s, elems, StrLen);
      elems[StrLen] = '\0';
    }

    str(
      str_cref const& other)
    {
      utils::copy_with_blank_padding(
        other.elems(), other.len(), elems, StrLen);
      elems[StrLen] = '\0';
    }

    template <int OtherStrLen>
    str(
      str<OtherStrLen> const& other)
    {
      utils::copy_with_blank_padding(
        other.elems, OtherStrLen, elems, StrLen);
      elems[StrLen] = '\0';
    }

    str(
      str_addends const& addends)
    {
      str_ref(*this) = addends;
    }

    //////////////w
    int len() const   
    {
      return StrLen;
    }
    std::string std_str() const
    {
      return std::string(elems, StrLen);
    }
    str_cref cref() const
    {
      return str_cref(elems, StrLen);
    }
    str_ref ref() const
    {
      return str_ref(elems, StrLen);
    }
	
    //////////////

    operator
    str_cref() const
    {
      return str_cref(elems, StrLen);
    }

    operator
    str_ref()
    {
      return str_ref(elems, StrLen);
    }

    operator
      std::string() const
    {
      return std::string(elems, StrLen);
    }

    void
    operator=(
      char const* rhs)
    {
      str_ref(*this) = rhs;
    }

    void
    operator=(
      std::string const& rhs)
    {
      str_ref(*this) = rhs;
    }

    void
    operator=(
      str_cref const& rhs)
    {
      str_ref(*this) = rhs;
    }

    template <int OtherStrLen>
    void
    operator=(
      str<OtherStrLen> const& rhs)
    {
      str_ref(*this) = str_cref(rhs);
    }

    void
    operator=(
      str_addends const& addends)
    {
      str_ref(*this) = addends;
    }

    bool
    operator==(
      char const* rhs) const
    {
      return (str_cref(*this) == rhs);
    }

    bool
    operator!=(
      char const* rhs) const
    {
      return (str_cref(*this) != rhs);
    }

    bool
    operator==(
      str_cref const& rhs) const
    {
      return (str_cref(*this) == rhs);
    }

    bool
    operator!=(
      str_cref const& rhs) const
    {
      return (str_cref(*this) != rhs);
    }

    bool
    operator<(
      str_cref const& rhs) const
    {
      return (str_cref(*this) < rhs);
    }

    bool
    operator<=(
      str_cref const& rhs) const
    {
      return (str_cref(*this) <= rhs);
    }

    bool
    operator>(
      str_cref const& rhs) const
    {
      return (str_cref(*this) > rhs);
    }

    bool
    operator>=(
      str_cref const& rhs) const
    {
      return (str_cref(*this) >= rhs);
    }

    str_cref
    operator()(
      int first,
      int last) const
    {
      return str_cref(*this)(first, last);
    }

    str_ref
    operator()(
      int first,
      int last)
    {
      return str_ref(*this)(first, last);
    }
    str_cref
    operator()(
      int first) const
    {
      return str_cref(*this)(first, StrLen);
    }

    str_ref
    operator()(
      int first)
    {
      return str_ref(*this)(first, StrLen);
    }

  };

  template <int StrLen>
  inline
  void
  str_ref::operator=(
    str<StrLen> const& rhs)
  {
    utils::copy_with_blank_padding(rhs.elems, StrLen, elems(), len());
  }

  template <int StrLen>
  struct zero_impl<str<StrLen> >
  {
    static str<StrLen> get() { return str<StrLen>(); }
  };

} // namespace fem

#endif // GUARD
