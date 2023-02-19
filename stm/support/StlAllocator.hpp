///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2005, 2006, 2007, 2008, 2009
// University of Rochester
// Department of Computer Science
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the University of Rochester nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef __STL_ALLOCATOR_H__
#define __STL_ALLOCATOR_H__

// needed for certain types, such as ptrdiff_t
#include <cstddef>

// ----------------------------------------------------------------------------
// FILE: StlAllocator.h
//
//  StlAllocator.h contains the template defenition for an stl compliant
//  allocator adapter.
//
//  Much of the credit for this class goes to Scott Meyer's Effective STL, and
//  Nicolai Josuttis' http://www.josuttis.com/cppcode/allocator.html allocator
//  example, from which this was adapted.
//
// ----------------------------------------------------------------------------

namespace stm
{
  namespace mm
  {
    extern void* txalloc(size_t);
    extern void txfree(void*);
  }

    // ------------------------------------------------------------------------
    // CLASS: StlAllocator
    // TEMPLATE PARAMATERS:
    //
    //  T: The type that we are allocating (the type that we are storing in the
    //     container.
    //
    //
    // DESCRIPTION:
    //
    //  The StlAllocator provides a template that we can use to force the Stl
    //  library containers to use our custom allocator for allocation and
    //  deletion of its internal nodes.
    //
    // ------------------------------------------------------------------------
    template <typename T>
    class StlAllocator
    {
      public:
        // type definitions
        typedef T        value_type;
        typedef T*       pointer;
        typedef const T* const_pointer;
        typedef T&       reference;
        typedef const T& const_reference;
        typedef std::size_t    size_type;
        typedef std::ptrdiff_t difference_type;

        // rebind template is required by the stl containers
        // see Meyer's book for more information (available from LD)
        template <class U>
        struct rebind
        {
            typedef  StlAllocator<U> other;
        };

        // return address of values
        pointer address(reference value) const
        {
            return &value;
        }

        const_pointer address(const_reference value) const
        {
            return &value;
        }

        // Construction and destruction
        StlAllocator()
        {
        }

        StlAllocator(const StlAllocator&)
        {
        }

        template <class U>
        StlAllocator(const StlAllocator<U>&)
        {
        }

        // Not virtual, NOT DESIGNED FOR INHERITENCE
        ~StlAllocator()
        {
        }

        // allocate but don't initialize num elements of type T
        pointer allocate(size_type num, const void* = 0)
        {
            // note that num is the # of Ts, not the size of the memory.
            return static_cast<pointer>(mm::txalloc(num*sizeof(T)));
        }

        // initialize elements of allocated storage p with value value
        void construct (pointer p, const T& value)
        {
            // initialize memory with placement new
            new((void*)p)T(value);
        }

        // destroy elements of initialized storage p
        void destroy (pointer p)
        {
            // destroy objects by calling their destructor
            p->~T();
        }

        // deallocate storage p of deleted elements
        void deallocate(pointer p, size_type num)
        {
            mm::txfree(static_cast<void*>(p));
        }

        // how many of this type can be allocated
        // without this we cannot compile on MSVC++
        size_type max_size() const
        {
            return (~((size_type)0)) / sizeof(T);
        }
    };

    // return that all specializations of this allocator are interchangeable
    template <class T1, class T2>
    bool operator==(const StlAllocator<T1>&, const StlAllocator<T2>&)
    {
        return true;
    }

    template <class T1, class T2>
    bool operator!=(const StlAllocator<T1>&, const StlAllocator<T2>&)
    {
        return false;
    }

} // namespace stm

#endif // __CUSTOM_ALLOC_H__
