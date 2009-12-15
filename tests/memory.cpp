/*  This file is part of the Vc library.

    Copyright (C) 2009 Matthias Kretz <kretz@kde.org>

    Vc is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    Vc is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Vc.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <Vc/Vc>
#include "unittest.h"

using namespace Vc;

template<typename V, unsigned int Size> struct TestEntries { static void run(); };
template<typename V> struct TestEntries<V, 0> { static void run() {} };

template<typename V, unsigned int Size> struct TestVectors { static void run(); };
template<typename V> struct TestVectors<V, 0> { static void run() {} };

template<typename V, unsigned int Size> void TestEntries<V, Size>::run()
{
    TestEntries<V, Size - 1>::run();
    typedef typename V::EntryType T;
    const T x = Size;
    FixedSizeMemory<V, Size> m;
    const FixedSizeMemory<V, Size> &m2 = m;
    for (unsigned int i = 0; i < Size; ++i) {
        m[i] = x;
    }
    for (unsigned int i = 0; i < Size; ++i) {
        COMPARE(m[i], x);
        COMPARE(m2[i], x);
    }
    for (unsigned int i = 0; i < Size; ++i) {
        COMPARE(m.entries()[i], x);
        COMPARE(m2.entries()[i], x);
    }
    const T *ptr = m2;
    for (unsigned int i = 0; i < Size; ++i) {
        COMPARE(ptr[i], x);
    }
}

template<typename V, unsigned int Size> void TestVectors<V, Size>::run()
{
    TestVectors<V, Size - 1>::run();
    typedef typename V::EntryType T;
    const V x = Size;
    FixedSizeMemory<V, Size> m;
    const FixedSizeMemory<V, Size> &m2 = m;
    for (unsigned int i = 0; i < m.vectorsCount(); ++i) {
        x.store(m.vector(i));
    }
    for (unsigned int i = 0; i < m.vectorsCount(); ++i) {
        COMPARE(V(m.vector(i)), x);
        COMPARE(V(m2.vector(i)), x);
    }
}

template<typename V> void testEntries()
{
    TestEntries<V, 128>::run();
}

template<typename V> void testVectors()
{
    TestVectors<V, 128>::run();
}

int main()
{
    testAllTypes(testEntries);
    testAllTypes(testVectors);

    return 0;
}