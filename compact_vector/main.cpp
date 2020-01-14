
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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <random>
#include <sax/iostream.hpp>

#include <sax/splitmix.hpp>
#include <sax/uniform_int_distribution.hpp>

#include "compact_vector.hpp"

// -fsanitize=address
/*
C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan_cxx-x86_64.lib
C:\Program Files\LLVM\lib\clang\10.0.0\lib\windows\clang_rt.asan-preinit-x86_64.lib
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
    for ( int i = 0; i < 1'000; ++i ) {
        size_type const is =
            sax::uniform_int_distribution<size_type> ( size_type{ 0 }, static_cast<size_type> ( range_ - 1 ) ) ( gen );
        Container data ( static_cast<size_type> ( is ) );
        size_type idx = sax::uniform_int_distribution<size_type> ( size_type{ 1 }, static_cast<size_type> ( range_ - is ) ) ( gen );
        while ( idx-- )
            data.emplace_back ( static_cast<value_type> ( idx ) );
    }
}

void test_eb ( ) {
    using Con = sax::compact_vector<int, int>;
    for ( int i = 4; i <= 65'536; i <<= 1 )
        emplace_back_random<Con> ( i );
}

int main ( ) {
    std::exception_ptr eptr;
    try {
        test_eb ( );
    }
    catch ( ... ) {
        eptr = std::current_exception ( ); // Capture.
    }
    handleEptr ( eptr );
    return EXIT_SUCCESS;
}
