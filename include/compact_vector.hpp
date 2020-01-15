
// MIT License
//
// Copyright (c) 2019 degski
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <sax/iostream.hpp>
#include <type_traits>
#include <utility>

// Costumization point.
#ifndef USE_MIMALLOC
#    define USE_MIMALLOC true
#    ifndef USE_MIMALLOC_LTO
#        if defined( _DEBUG )
#            define USE_MIMALLOC_LTO false
#        else
#            define USE_MIMALLOC_LTO true
#        endif
#    endif
#    include <mimalloc.h>
#endif

namespace sax {

namespace detail::cv {

#if USE_MIMALLOC
[[nodiscard]] inline void * malloc ( std::size_t size_ ) noexcept { return mi_malloc ( size_ ); }
[[nodiscard]] inline void * zalloc ( std::size_t size_ ) noexcept { return mi_zalloc ( size_ ); }
[[nodiscard]] inline void * calloc ( std::size_t num_, std::size_t size_ ) noexcept { return mi_calloc ( num_, size_ ); }
[[nodiscard]] inline void * realloc ( void * ptr_, std::size_t new_size_ ) noexcept { return mi_realloc ( ptr_, new_size_ ); }
inline void free ( void * ptr_ ) noexcept { mi_free ( ptr_ ); }

namespace {

#    if _WIN32
// Set eager region commit on Windows.
inline bool const windows_eager_region_commit = [] {
    mi_option_set ( mi_option_eager_region_commit, 1 );
    return mi_option_get ( mi_option_eager_region_commit );
}( );
#    endif

} // namespace

#else
[[nodiscard]] inline void * malloc ( std::size_t size_ ) noexcept { return std::malloc ( size_ ); }
[[nodiscard]] inline void * zalloc ( std::size_t size_ ) noexcept { return std::calloc ( 1u, size_ ); }
[[nodiscard]] inline void * calloc ( std::size_t num_, std::size_t size_ ) noexcept { return std::calloc ( num_, size_ ); }
[[nodiscard]] inline void * realloc ( void * ptr_, std::size_t new_size_ ) noexcept { return std::realloc ( ptr_, new_size_ ); }
inline void free ( void * ptr_ ) noexcept { std::free ( ptr_ ); }
#endif

template<typename Type, typename SizeType = int>
struct params {

    using value_type    = Type;
    using pointer       = value_type *;
    using const_pointer = value_type const *;

    using size_type = SizeType;

    size_type capacity, size;
};

} // namespace detail::cv

template<typename Type, typename SizeType = int, SizeType max_allocation_size = std::numeric_limits<SizeType>::max ( ),
         SizeType default_allocation_size = 1>
class compact_vector {

    public:
    using value_type    = Type;
    using pointer       = value_type *;
    using const_pointer = value_type const *;

    using reference       = value_type &;
    using const_reference = value_type const &;
    using rv_reference    = value_type &&;

    using size_type       = SizeType;
    using difference_type = std::make_signed<size_type>;

    using iterator               = pointer;
    using const_iterator         = const_pointer;
    using reverse_iterator       = pointer;
    using const_reverse_iterator = const_pointer;

    using void_ptr = void *;
    using params   = detail::cv ::params<value_type, size_type>;

    static_assert ( std::is_trivially_copyable<Type>::value, "Type must be trivially copyable!" );

    // Construct.

    explicit compact_vector ( ) noexcept {}
    compact_vector ( size_type const size_ ) noexcept {
        void_ptr p = detail::cv::malloc ( sizeof ( params ) + size_ * sizeof ( value_type ) );
        new ( p ) params{ size_, size_ };
        m_data = ptr_mem ( p );
        std::for_each ( begin ( ), end ( ),
                        [] ( value_type & value_ref ) { new ( &value_ref ) value_type{ }; } ); // default construct values.
    }
    compact_vector ( compact_vector const & cv_ ) {
        if ( cv_.m_data ) {
            auto const size = cv_.size ( );
            void_ptr p      = detail::cv::malloc ( sizeof ( params ) + size * sizeof ( value_type ) );
            new ( p ) params{ size, size };
            m_data = ptr_mem ( p );
            std::copy ( std::begin ( cv_ ), std::end ( cv_ ), begin ( ) );
        }
    }
    compact_vector ( compact_vector && cv_ ) noexcept { swap ( cv_ ); }

    ~compact_vector ( ) noexcept { release ( ); }

    // Assignment.

    [[maybe_unused]] compact_vector & operator= ( compact_vector const & rhs_ ) {
        if ( rhs_.m_data ) {
            auto const size = rhs_.size_ref ( );
            if ( m_data ) {
                for ( auto & v : *this )
                    v.~Type ( );
                if ( capacity_ref ( ) < size ) {
                    params_ref ( ) = { size, size };
                    m_data =
                        ptr_mem ( detail::cv::realloc ( mem_ptr ( m_data ), sizeof ( params ) + size * sizeof ( value_type ) ) );
                }
            }
            else {
                void_ptr p = detail::cv::malloc ( sizeof ( params ) + size * sizeof ( value_type ) );
                m_data     = ptr_mem ( p );
                new ( p ) params{ size, size };
            }
            std::copy ( std::begin ( rhs_ ), std::end ( rhs_ ), std::begin ( *this ) );
        }
        else {
            release ( );
        }
        return *this;
    }

    [[maybe_unused]] compact_vector & operator= ( compact_vector && rhs_ ) noexcept {
        swap ( rhs_ );
        return *this;
    }

    // Manage.

    void clear ( ) {
        if ( m_data ) {
            std::for_each ( begin ( ), end ( ), [] ( value_type & value_ref ) { value_ref.~Type ( ); } );
            size_ref ( ) = 0;
        }
    }

    void release ( ) noexcept {
        if ( m_data ) {
            std::for_each ( begin ( ), end ( ), [] ( value_type & value_ref ) { value_ref.~Type ( ); } );
            detail::cv::free ( mem_ptr ( m_data ) );
            m_data = nullptr;
        }
    }

    void reserve ( size_type cap_ ) {
        cap_ = std::min ( max_allocation_size, cap_ ); // clamp.
        if ( m_data ) {
            if ( cap_ > capacity_ref ( ) ) {
                capacity_ref ( ) = cap_;
                m_data           = ptr_mem ( detail::cv::realloc ( m_data, sizeof ( params ) + cap_ * sizeof ( value_type ) ) );
            }
        }
        else {
            m_data = ptr_mem ( detail::cv::malloc ( sizeof ( params ) + cap_ * sizeof ( value_type ) ) );
            new ( m_data ) params{ cap_, 0 };
        }
    }

    void resize ( size_type new_size_ ) { // TODO
        if ( m_data ) {
            auto size = size_ref ( );
            if ( capacity_ref ( ) < new_size_ ) {
                params_ref ( ) = { new_size_, new_size_ };
                m_data =
                    ptr_mem ( detail::cv::realloc ( mem_ptr ( m_data ), sizeof ( params ) + new_size_ * sizeof ( value_type ) ) );
                std::for_each ( begin ( ) + size, end ( ), [] ( value_type & value_ref ) { new ( &value_ref ) value_type{ }; } );
            }
            else {
            }
        }
        else {
            m_data = ptr_mem ( detail::cv::malloc ( sizeof ( params ) + new_size_ * sizeof ( value_type ) ) );
            new ( m_data ) params{ new_size_, new_size_ };
            std::for_each ( begin ( ), end ( ),
                            [] ( value_type & value_ref ) { new ( &value_ref ) value_type{ }; } ); // default construct values.
        }
    }

    // Access.

    [[nodiscard]] const_reference front ( ) const noexcept {
        assert ( size ( ) );
        return m_data[ 0 ];
    }
    [[nodiscard]] reference front ( ) noexcept { return const_cast<reference> ( std::as_const ( *this ).front ( ) ); }

    [[nodiscard]] const_reference back ( ) const noexcept {
        assert ( size ( ) );
        return m_data[ size_ref ( ) - size_type{ 1 } ];
    }
    [[nodiscard]] reference back ( ) noexcept { return const_cast<reference> ( std::as_const ( *this ).back ( ) ); }

    [[nodiscard]] const_reference at ( size_type const i_ ) const { // TODO add index value to message.
        if ( i_ < size_type{ 0 } )
            throw std::runtime_error ( "compact_vector access error: negative index" );
        if ( i_ >= static_cast<size_type> ( size_ref ( ) ) )
            throw std::runtime_error ( "compact_vector access error: index too large" );
        return m_data[ i_ ];
    }
    [[nodiscard]] reference at ( size_type const i_ ) { return const_cast<reference> ( std::as_const ( *this ).at ( i_ ) ); }

    [[nodiscard]] const_reference operator[] ( size_type const i_ ) const noexcept {
        assert ( not( i_ < size_type{ 0 } ) );
        assert ( not( i_ >= static_cast<size_type> ( size_ref ( ) ) ) );
        return m_data[ i_ ];
    }
    [[nodiscard]] reference operator[] ( size_type const i_ ) noexcept {
        return const_cast<reference> ( std::as_const ( *this ).operator[] ( i_ ) );
    }

    // Compare for equality.

    [[nodiscard]] bool operator== ( compact_vector const & rhs_ ) const noexcept {
        if ( m_data == rhs_.m_data ) // includes comparing 2 nullptrs.
            return true;
        if ( not m_data or not rhs_.m_data or size_ref ( ) != rhs_.size_ref ( ) )
            return false;
        return std::equal ( begin ( ), end ( ), rhs_.begin ( ), rhs_.end ( ) );
    }
    [[nodiscard]] bool operator!= ( compact_vector const & rhs_ ) const noexcept { return not operator== ( rhs_ ); }

    // Data.

    [[nodiscard]] const_pointer data ( ) const noexcept { return m_data; }
    [[nodiscard]] pointer data ( ) noexcept { return const_cast<pointer> ( std::as_const ( *this ).m_data ); }

    // Iterators.

    [[nodiscard]] const_iterator begin ( ) const noexcept {
        assert ( m_data );
        return const_iterator{ m_data };
    }
    [[nodiscard]] const_iterator cbegin ( ) const noexcept { return begin ( ); }
    [[nodiscard]] iterator begin ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).begin ( ) ); }

    [[nodiscard]] const_iterator end ( ) const noexcept {
        assert ( m_data );
        return const_iterator{ m_data + size_ref ( ) };
    }
    [[nodiscard]] const_iterator cend ( ) const noexcept { return end ( ); }
    [[nodiscard]] iterator end ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).end ( ) ); }

    [[nodiscard]] const_iterator rbegin ( ) const noexcept {
        assert ( m_data );
        return const_iterator{ m_data + ( size_ref ( ) - size_type{ 1 } ) };
    }
    [[nodiscard]] const_iterator crbegin ( ) const noexcept { return rbegin ( ); }
    [[nodiscard]] iterator rbegin ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).rbegin ( ) ); }

    [[nodiscard]] const_iterator rend ( ) const noexcept {
        assert ( m_data );
        return const_iterator{ m_data - size_type{ 1 } };
    }
    [[nodiscard]] const_iterator crend ( ) const noexcept { return rend ( ); }
    [[nodiscard]] iterator rend ( ) noexcept { return const_cast<iterator> ( std::as_const ( *this ).rend ( ) ); }

    // Sizes.

    [[nodiscard]] static constexpr size_type max_capacity ( ) noexcept { return max_allocation_size; }

    [[nodiscard]] inline size_type capacity ( ) const noexcept {
        return m_data ? *reinterpret_cast<size_type *> ( reinterpret_cast<char *> ( m_data ) - 2 * sizeof ( size_type ) ) : 0;
    }
    [[nodiscard]] inline size_type size ( ) const noexcept {
        return m_data ? *reinterpret_cast<size_type *> ( reinterpret_cast<char *> ( m_data ) - 1 * sizeof ( size_type ) ) : 0;
    }

    [[nodiscard]] bool empty ( ) const noexcept { return not m_data or not size ( ); }

    // Emplace/Pop.

    private:
    template<typename... Args>
    [[maybe_unused]] reference construct ( Args &&... args_ ) {
        void_ptr const p = detail::cv::malloc ( sizeof ( params ) + default_allocation_size * sizeof ( value_type ) );
        new ( p ) params{ default_allocation_size, 1 };
        m_data = ptr_mem ( p );
        assert ( size ( ) < capacity ( ) );
        return *new ( m_data ) value_type{ std::forward<Args> ( args_ )... };
    }

    public:
    template<typename... Args>
    [[maybe_unused]] reference emplace_back ( Args &&... args_ ) {
        if ( m_data ) {                             // not allocate, maybe relocate.
            if ( size_ref ( ) == capacity_ref ( ) ) // relocate.
                m_data = ptr_mem (
                    detail::cv::realloc ( mem_ptr ( m_data ), sizeof ( params ) + set_new_capacity ( ) * sizeof ( value_type ) ) );
            assert ( size ( ) < capacity ( ) );
            return *new ( m_data + size_ref ( )++ ) value_type{ std::forward<Args> ( args_ )... };
        }
        else { // allocate.
            return construct ( std::forward<Args> ( args_ )... );
        }
    }

    [[maybe_unused]] reference push_back ( const_reference v_ ) { return emplace_back ( value_type{ v_ } ); }

    void pop_back ( ) noexcept {
        assert ( size_ref ( ) );
        m_data[ --size_ref ( ) ].~Type ( );
    }

    // Swap.

    void swap ( compact_vector & rhs_ ) noexcept { std::swap ( m_data, rhs_.m_data ); }

    void swap_elements ( size_type const a_, size_type const b_ ) noexcept { std::swap ( m_data[ a_ ], m_data[ b_ ] ); }

    // Erase.

    void erase ( size_type const i_ ) noexcept {
        auto & s     = --size_ref ( );
        m_data[ i_ ] = m_data[ s ];
        m_data[ s ].~Type ( );
    }

    // Output.

    template<typename Stream>
    [[maybe_unused]] friend Stream & operator<< ( Stream & out_, compact_vector const & m_ ) noexcept {
        for ( auto const & e : m_ )
            out_ << e << sp; // A wide- or narrow-string space, as appropriate.
        return out_;
    }

    private:
    // Set and return the new (grown) capacity. Needs to be called before realloc, so the
    // new capacity will be set correctly in the newly created memory block. This function
    // implements the MSVC-growth strategy for std::vector.
    [[maybe_unused]] size_type set_new_capacity ( ) noexcept {
        size_type c = capacity_ref ( );
        if ( c > 1 ) {
            c                = std::min ( max_allocation_size, c + c / 2 );
            capacity_ref ( ) = c;
            return c;
        }
        else {
            capacity_ref ( ) = 2;
            return 2;
        }
    }

    [[nodiscard]] inline params const & params_ref ( ) const noexcept {
        assert ( m_data );
        return *reinterpret_cast<params *> ( reinterpret_cast<char *> ( m_data ) - 2 * sizeof ( size_type ) );
    }
    [[nodiscard]] inline size_type const & capacity_ref ( ) const noexcept {
        assert ( m_data );
        return *reinterpret_cast<size_type *> ( reinterpret_cast<char *> ( m_data ) - 2 * sizeof ( size_type ) );
    }
    [[nodiscard]] inline size_type const & size_ref ( ) const noexcept {
        assert ( m_data );
        return *reinterpret_cast<size_type *> ( reinterpret_cast<char *> ( m_data ) - 1 * sizeof ( size_type ) );
    }

    [[nodiscard]] inline params & params_ref ( ) noexcept {
        return const_cast<params &> ( std::as_const ( *this ).params_ref ( ) );
    }
    [[nodiscard]] inline size_type & capacity_ref ( ) noexcept {
        return const_cast<size_type &> ( std::as_const ( *this ).capacity_ref ( ) );
    }
    [[nodiscard]] inline size_type & size_ref ( ) noexcept {
        return const_cast<size_type &> ( std::as_const ( *this ).size_ref ( ) );
    }

    [[nodiscard]] inline void_ptr mem_ptr ( pointer mem_ ) const noexcept {
        assert ( mem_ );
        return reinterpret_cast<void_ptr> ( reinterpret_cast<char *> ( mem_ ) - 2 * sizeof ( size_type ) );
    }
    [[nodiscard]] inline pointer ptr_mem ( void_ptr ptr_ ) const noexcept {
        std::cout << "pa " << pointer_alignment ( ptr_ ) << nl;
        assert ( ptr_ );
        return reinterpret_cast<pointer> ( reinterpret_cast<char *> ( ptr_ ) + 2 * sizeof ( size_type ) );
    }

    // Pointer alignment.
    static inline int pointer_alignment ( void_ptr ptr_ ) noexcept {
        return ( int ) ( ( std::uintptr_t ) ptr_ & ( std::uintptr_t ) - ( ( std::intptr_t ) ptr_ ) );
    }

    pointer m_data = nullptr;
};

} // namespace sax
