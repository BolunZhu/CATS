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

#ifndef STM_COMMON_DESCRIPTOR_POLICY_HPP__
#define STM_COMMON_DESCRIPTOR_POLICY_HPP__

/**
 *  All of the API files interact with the stm libraries through a Descriptor
 *  class. This Descriptor is a per-thread object, which is accessed through
 *  thread local storage. The APIs have the option of either hitting thread
 *  local storage every time that they need a descriptor, or wasting some
 *  extra space in readable and writable smart pointers in order to cache it.
 *
 *  This file contains the interface to this mechanism. Using multiple
 *  inheritence, a smart pointer can inherit from a DescriptorPolicy which
 *  will control which policy (caching or lookup) it uses.
 */

namespace stm
{
  // The STM_API_CACHE_DESCRIPTOR flag is set in config.h by the
  // configuration script.
#ifdef STM_API_CACHE_DESCRIPTOR

  /**
   *  The basic descriptor policy is to cache a reference to the descriptor,
   *  returning it whenever it's needed.
   *
   *  \type Descriptor    The library's Descriptor type. The policy doesn't
   *                      really want to know about this type
   *                      directly. Descriptor should have a static method
   *                      called MyDescriptor() that will return the
   *                      calling thread's Descriptor.
   */
  template <class Descriptor>
  class DescriptorPolicy
  {
    public:
      Descriptor& getDescriptor() const { return m_tx; }

    protected:
      DescriptorPolicy() : m_tx(Descriptor::MyDescriptor()) { }

      DescriptorPolicy& operator=(const DescriptorPolicy&) { return *this; }

    private:
      Descriptor& m_tx;
  };

  /**
   * Word-based caching policy is a little cleaner, since the word-based STMs
   * expose a Self field
   */
  template <class D> // D-> Descriptor
  class WBDescriptorPolicy
  {
    public:
      D& getDescriptor() const { return m_tx; }

    protected:
      WBDescriptorPolicy() : m_tx(*D::Self.get()) { }

      WBDescriptorPolicy& operator=(const WBDescriptorPolicy&) { return *this; }

    private:
      D& m_tx;
  };

#else
  /**
   *  This implementation looks up the thread local descriptor whenever it's
   *  needed.
   *
   *  \type Descriptor    The library-specific Descriptor class. Should have
   *                      a static method MyDescriptor() that will return
   *                      the calling thread's Descriptor.
   */
  template <class D>
  class DescriptorPolicy
  {
    public:
      static D& getDescriptor() { return D::MyDescriptor(); }
  };

  /*** Again, word-based is a bit easier */
  template <class D>
  class WBDescriptorPolicy
  {
    public:
      static D& getDescriptor() { return *D::Self.get(); }
  };

#endif
} // namespace stm

#endif // STM_COMMON_DESCRIPTOR_POLICY_HPP__
