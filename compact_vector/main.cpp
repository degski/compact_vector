
// MIT License
//
// Copyright (c) 2020 degski
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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <random>
#include <sax/iostream.hpp>

#include <sax/splitmix.hpp>
#include <sax/uniform_int_distribution.hpp>

#define USE_MIMALLC false

#include "compact_vector.hpp"

// -fsanitize=address
/*
C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan_cxx-x86_64.lib;
C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan-preinit-x86_64.lib;
C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan-x86_64.lib
*/

void handleEptr ( std::exception_ptr eptr ) { // Passing by value is ok.
    try {
        if ( eptr )
            std::rethrow_exception ( eptr );
    }
    catch ( std::exception const & e ) {
        std::cout << "Caught exception \"" << e.what ( ) << "\"\n";
    }
}

template<class Container>
void emplace_back_random ( typename Container::size_type range_ ) noexcept {
    using value_type = typename Container::value_type;
    using size_type  = typename Container::size_type;
    sax::splitmix64 gen;
    for ( int i = 0; i < 10; ++i ) {
        size_type const is =
            sax::uniform_int_distribution<size_type> ( size_type{ 0 }, static_cast<size_type> ( range_ - 1 ) ) ( gen );
        Container data ( static_cast<size_type> ( is ) );
        size_type idx = sax::uniform_int_distribution<size_type> ( size_type{ 1 }, static_cast<size_type> ( range_ - is ) ) ( gen );
        while ( idx-- )
            data.emplace_back ( static_cast<value_type> ( idx ) );
    }
}

void test_eb ( ) {
    using Con = sax::compact_vector<int, std::int32_t>;
    for ( int r = 0; r < 1'000; r++ )
        for ( int i = 4; i <= 65'536; i <<= 2 )
            emplace_back_random<Con> ( i );
}

int main ( ) {

    std::exception_ptr eptr;
    try {

        /*
        sax::compact_vector<int, std::int64_t, 4> v;

        v.push_back ( 1 );
        v.push_back ( 11 );
        v.push_back ( 111 );
        v.push_back ( 1111 );

        for ( auto i : v )
            std::cout << i << ' ';
        std::cout << nl;

        std::cout << v.size ( ) << nl;
        std::cout << v.capacity ( ) << nl;

        std::cout << v.unordered_erase ( 0 ) << nl;

        for ( auto i : v )
            std::cout << i << ' ';
        std::cout << nl;

        auto w = v;

        std::cout << w.unordered_erase ( 0 ) << nl;

        for ( auto i : w )
            std::cout << i << ' ';
        std::cout << nl;
        */

        test_eb ( );
    }
    catch ( ... ) {
        eptr = std::current_exception ( ); // Capture.
    }
    handleEptr ( eptr );

    return EXIT_SUCCESS;
}

#include <immintrin.h>

#include <chrono>

void test ( ) {

    const int sz = 1'024;
    float * mas  = ( float * ) _mm_malloc ( sz * sizeof ( float ), 32 );
    float * tar  = ( float * ) _mm_malloc ( sz * sizeof ( float ), 32 );
    float a      = 0;
    std::generate ( mas, mas + sz, [ & ] ( ) { return ++a; } );

    const int nn = 1'000'000; // Number of iteration in tester loops
    std::chrono::time_point<std::chrono::system_clock> start1, end1, start2, end2, start3, end3;

    // std::copy testing
    start1 = std::chrono::system_clock::now ( );
    for ( int i = 0; i < nn; ++i )
        std::copy ( mas, mas + sz, tar );
    // std::memcpy ( tar, mas, sz * sizeof ( float ) );
    // std::strncpy ( ( char * ) tar, ( char * ) mas, sz * sizeof ( float ) );
    end1           = std::chrono::system_clock::now ( );
    float elapsed1 = std::chrono::duration_cast<std::chrono::microseconds> ( end1 - start1 ).count ( );

    // SSE-copy testing
    start2 = std::chrono::system_clock::now ( );
    for ( int i = 0; i < nn; ++i ) {
        auto _mas = mas;
        auto _tar = tar;
        for ( ; _mas != mas + sz; _mas += 4, _tar += 4 ) {
            __m128 buffer = _mm_load_ps ( _mas );
            _mm_store_ps ( _tar, buffer );
        }
    }
    end2           = std::chrono::system_clock::now ( );
    float elapsed2 = std::chrono::duration_cast<std::chrono::microseconds> ( end2 - start2 ).count ( );

    // AVX-copy testing
    start3 = std::chrono::system_clock::now ( );
    for ( int i = 0; i < nn; ++i ) {
        auto _mas = mas;
        auto _tar = tar;
        for ( ; _mas != mas + sz; _mas += 8, _tar += 8 ) {
            __m256 buffer = _mm256_load_ps ( _mas );
            _mm256_store_ps ( _tar, buffer );
        }
    }
    end3           = std::chrono::system_clock::now ( );
    float elapsed3 = std::chrono::duration_cast<std::chrono::microseconds> ( end3 - start3 ).count ( );

    std::cout << "serial - " << elapsed1 << ", SSE - " << elapsed2 << ", AVX - " << elapsed3
              << "\nSSE gain: " << elapsed1 / elapsed2 << "\nAVX gain: " << elapsed1 / elapsed3 << nl;

    _mm_free ( mas );
    _mm_free ( tar );
}

int main6578 ( ) {

    for ( int i = 0; i < 3; ++i )
        test ( );

    return EXIT_SUCCESS;
}

#include <memory>

template<std::size_t Alignment>
class Aligned {
    public:
    void * operator new ( std::size_t size ) {
        std::size_t space   = size + ( Alignment - 1 );
        void * ptr          = malloc ( space + sizeof ( void * ) );
        void * original_ptr = ptr;

        char * ptr_bytes = static_cast<char *> ( ptr );
        ptr_bytes += sizeof ( void * );
        ptr = static_cast<void *> ( ptr_bytes );

        ptr = std::align ( Alignment, size, ptr, space );

        ptr_bytes = static_cast<char *> ( ptr );
        ptr_bytes -= sizeof ( void * );
        std::memcpy ( ptr_bytes, &original_ptr, sizeof ( void * ) );

        return ptr;
    }

    void operator delete ( void * ptr ) {
        char * ptr_bytes = static_cast<char *> ( ptr );
        ptr_bytes -= sizeof ( void * );

        void * original_ptr;
        std::memcpy ( &original_ptr, ptr_bytes, sizeof ( void * ) );

        std::free ( original_ptr );
    }
};

// Use it like this :

class Camera : public Aligned<16> {};

template<typename Type>
inline Type * memcpyAVX ( Type * destination, Type * source, size_t count ) {
    unsigned int firstLoopCount = ( ( count * sizeof ( Type ) ) / sizeof ( __m256 ) );

    __m256 avxRegister, *pre;

    float * a = reinterpret_cast<float *> ( reinterpret_cast<void *> ( destination ) );
    float * b = reinterpret_cast<float *> ( reinterpret_cast<void *> ( source ) );

    for ( unsigned int i = 0; i < firstLoopCount; i += ( sizeof ( __m256 ) / sizeof ( Type ) ) ) {
        avxRegister = _mm256_load_ps ( b );
        _mm256_store_ps ( a, avxRegister );

        a += 8;
        b += 8;
    }

    return destination;
}

#include <iostream>
using std::cout;
using std::endl;

#include <emmintrin.h>
#include <malloc.h>
#include <time.h>
#include <string.h>

#define ENABLE_PREFETCH

#define f_vector __m128d
#define i_ptr size_t
inline void swap_block ( f_vector * A, f_vector * B, i_ptr L ) {
    //  To be super-optimized later.

    f_vector * stop = A + L;

    do {
        f_vector tmpA = *A;
        f_vector tmpB = *B;
        *A++          = tmpB;
        *B++          = tmpA;
    } while ( A < stop );
}
void transpose_even ( f_vector * T, i_ptr block, i_ptr x ) {
    //  Transposes T.
    //  T contains x columns and x rows.
    //  Each unit is of size (block * sizeof(f_vector)) bytes.

    // Conditions:
    //  - 0 < block
    //  - 1 < x

    i_ptr row_size  = block * x;
    i_ptr iter_size = row_size + block;

    //  End of entire matrix.
    f_vector * stop_T = T + row_size * x;
    f_vector * end    = stop_T - row_size;

    //  Iterate each row.
    f_vector * y_iter = T;
    do {
        //  Iterate each column.
        f_vector * ptr_x = y_iter + block;
        f_vector * ptr_y = y_iter + row_size;

        do {

#ifdef ENABLE_PREFETCH
            _mm_prefetch ( ( char * ) ( ptr_y + row_size ), _MM_HINT_T0 );
#endif

            swap_block ( ptr_x, ptr_y, block );

            ptr_x += block;
            ptr_y += row_size;
        } while ( ptr_y < stop_T );

        y_iter += iter_size;
    } while ( y_iter < end );
}
int main785 ( ) {

    i_ptr dimension = 4096;
    i_ptr block     = 16;

    i_ptr words = block * dimension * dimension;
    i_ptr bytes = words * sizeof ( f_vector );

    cout << "bytes = " << bytes << endl;
    //    system("pause");

    f_vector * T = ( f_vector * ) _mm_malloc ( bytes, 16 );
    if ( T == NULL ) {
        cout << "Memory Allocation Failure" << endl;
        system ( "pause" );
        exit ( 1 );
    }
    memset ( T, 0, bytes );

    //  Perform in-place data transpose
    cout << "Starting Data Transpose...   ";
    clock_t start = clock ( );
    transpose_even ( T, block, dimension );
    clock_t end = clock ( );

    cout << "Done" << endl;
    cout << "Time: " << ( double ) ( end - start ) / CLOCKS_PER_SEC << " seconds" << endl;

    _mm_free ( T );
    system ( "pause" );

    return EXIT_SUCCESS;
}

int main678678 ( ) {

    // std::wcout << sax::fg::wmagenta << sax::winvert_colors << L"This is magenta" << sax::wdefault_colors << nl;

    std::cout << sax::fg::bright_magenta << "This is coloured" << sax::reset << nl;

    return EXIT_SUCCESS;
}
