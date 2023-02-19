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

#ifndef WBMETADATA_H__
#define WBMETADATA_H__

namespace stm
{
  /**
   *  owner_version_t indicates the version number of an unlocked location,
   *  or the owner of a locked location.  owner_version_t should be suitable
   *  for use in either a global table (for stripe-based STM) or in an
   *  object's header (for object-based STM)
   */
  template <class T>
  union owner_version_t
  {
      // note that when lock == 1, then (owner & ~1) is a valid Descriptor*
      T* owner;
      struct
      {
          // ensure lsb is lock bit regardless of platform
#ifdef STM_INEV_IRL
          // IRL needs a read bit in the 2lsb position
#    if defined(__i386__) || defined(_MSC_VER) /* little endian */
          unsigned lock:1;
          unsigned reads:1;
          unsigned num:(8*sizeof(void*))-2;
#    else /* big endian */
          unsigned num:(8*sizeof(void*))-2;
          unsigned reads:1;
          unsigned lock:1;
#    endif
#else
#    if defined(__i386__) || defined(_MSC_VER) /* little endian */
          unsigned lock:1;
          unsigned num:(8*sizeof(void*))-1;
#    else /* big endian */
          unsigned num:(8*sizeof(void*))-1;
          unsigned lock:1;
#    endif
#endif
      } version;
      unsigned long all; // read entire struct in 1 op
  };

  struct wlog_t
  {
      void** addr;
      void* val;
      wlog_t(void** a, void* v) : addr(a), val(v) { }
  };

  /**
   * C++ Template Voodoo: the addr_dispatch class takes an address and a type,
   * and determines which words (represented as void*s) ought to be read and
   * written to effect a read or write of the given type, from the given
   * address.
   */
  template <class D, typename T, int S>
  struct addr_dispatch
  {
      // use this to ensure compile-time errors
      struct InvalidTypeAsSecondTemplateParameter { };

      // the read method will transform a read to a sizeof(T) byte range
      // starting at addr into a set of stm_read_word calls.  For now, the
      // range must be aligned on a sizeof(T) boundary, and T must be 1, 4, or
      // 8 bytes.
      __attribute__((always_inline))
      static T read(T* addr, D* thread)
      {
          InvalidTypeAsSecondTemplateParameter itastp;
          T invalid = (T)itastp;
          return invalid;
      }

      // same as read, but for writes
      __attribute__((always_inline))
      static void write(T* addr, T val, D* thread)
      {
          InvalidTypeAsSecondTemplateParameter itaftp;
          T invalid = (T)itaftp;
      }
  };

  /*** standard dispatch for 4-byte types (to include sh_ptr<T>) */
  template <class D, typename T>
  struct addr_dispatch<D, T, 4>
  {
      __attribute__((always_inline))
      static T read(T* addr, D* thread)
      {
          union { T* t; void** v; } a, v;
          a.t = addr;
          void* val = thread->stm_read_word(a.v);
          v.v = &val;
          return *v.t;
      }

      __attribute__((always_inline))
      static void write(T* addr, T val, D* thread)
      {
          union { T* t; void** v; } a, v;
          a.t = addr;
          v.t = &val;
          thread->stm_write_word(a.v, *v.v);
      }
  };

  template <class D>
  struct addr_dispatch<D, float, 4>
  {
      __attribute__((always_inline))
      static float read(float* addr, D* thread)
      {
          union { float* f; void** v; } a;
          union { float f;  void* v;  } v;
          a.f = addr;
          v.v = thread->stm_read_word(a.v);
          return v.f;
      }

      __attribute__((always_inline))
      static void write(float* addr, float val, D* thread)
      {
          union { float* f; void** v; } a;
          union { float f;  void* v;  } v;
          a.f = addr;
          v.f = val;
          thread->stm_write_word(a.v, v.v);
      }
  };

  template <class D>
  struct addr_dispatch<D, const float, 4>
  {
      __attribute__((always_inline))
      static float read(const float* addr, D* thread)
      {
          union { const float* f; void** v; } a;
          union { float f;  void* v;  } v;
          a.f = addr;
          v.v = thread->stm_read_word(a.v);
          return v.f;
      }

      __attribute__((always_inline))
      static void write(const float* addr, float val, D* thread)
      {
          assert(false && "You should not be writing a const float!");
      }
  };

  template <class D, typename T>
  struct addr_dispatch<D, T, 8>
  {
      __attribute__((always_inline))
      static T read(T* addr, D* thread)
      {
          // get second word's address
          void** addr2 = (void**)((long)addr + 4);
          union {
              long long l;
              struct { void* v1; void* v2; } v;
          } v;
          // read the two words
          v.v.v1 = thread->stm_read_word((void**)addr);
          v.v.v2 = thread->stm_read_word(addr2);
          // return the two words as a T
          union { long long* l; T* t; } ret;
          ret.l = &v.l;
          return *ret.l;
      }

      __attribute__((always_inline))
       static void write(T* addr, T val, D* thread)
      {
          // compute the two addresses
          void** addr1 = (void**)addr;
          void** addr2 = (void**)((long)addr + 4);
          // turn the value into two words
          union {
              T t;
              struct { void* v1; void* v2; } v;
          } v;
          v.t = val;
          // write the two words
          thread->stm_write_word(addr1, v.v.v1);
          thread->stm_write_word(addr2, v.v.v2);
      }

  };

  template <class D>
  struct addr_dispatch<D, double, 8>
  {
      __attribute__((always_inline))
      static double read(double* addr, D* thread)
      {
          // get second word's address
          void** addr2 = (void**)((long)addr + 4);
          union {
              double t;
              struct { void* v1; void* v2; } v;
          } v;
          // read the two words
          v.v.v1 = thread->stm_read_word((void**)addr);
          v.v.v2 = thread->stm_read_word(addr2);
          return v.t;
      }

      __attribute__((always_inline))
       static void write(double* addr, double val, D* thread)
      {
          // compute the two addresses
          void** addr1 = (void**) addr;
          void** addr2 = (void**) ((long)addr + 4);
          // turn the value into two words
          union {
              double t;
              struct { void* v1; void* v2; } v;
          } v;
          v.t = val;
          // write the two words
          thread->stm_write_word(addr1, v.v.v1);
          thread->stm_write_word(addr2, v.v.v2);
      }
  };

  template <class D>
  struct addr_dispatch<D, const double, 8>
  {
      __attribute__((always_inline))
      static double read(const double* addr, D* thread)
      {
          // get the second word's address
          void** addr2 = (void**)((long)addr + 4);
          union {
              double t;
              struct { void* v1; void* v2; } v;
          } v;
          // read the two words
          v.v.v1 = thread->stm_read_word((void**)addr);
          v.v.v2 = thread->stm_read_word(addr2);
          return v.t;
      }

      __attribute__((always_inline))
      static void write(const double* addr, double val, D* thread)
      {
          assert(false && "You should not be writing a const double!");
      }
  };

  template <class D, typename T>
  struct addr_dispatch<D, T, 1>
  {
      __attribute__((always_inline))
      static T read(T* addr, D* thread)
      {
          // we must read the word (as a void*) that contains the byte at
          // address addr, then treat that as an array of T's from which we
          // pull out a specific selement (based on masking the last two bits)
          union { char v[4]; void* v2; } v;
          void** a = (void**)(((long)addr) & ~3);
          long offset = ((long)addr) & 3;
          v.v2 = thread->stm_read_word(a);
          union { T* a; char* addr2; } q;
          q.addr2 = &v.v[offset];
          return *q.a;
      }

       __attribute__((always_inline))
       static void write(T* addr, T val, D* thread)
       {
           // to protect granularity, we need to read the whole word and then
           // write a byte of it
           union { T v[4]; void* v2; } v;
           void** a = (void**)(((long)addr) & ~3);
           long offset = ((long)addr) & 3;
           // read the enclosing word
           v.v2 = thread->stm_read_word(a);
           v.v[offset] = val;
           thread->stm_write_word(a,v.v2);
       }
  };

  /**
   * In RSTM, we can always find the last version of an acquired but not
   * committed transaction by looking at shared->payload->next.  We need
   * something similar here, so we will say that the big table of orecs holds
   * two orecs, one that is used for locking and one that holds the previous
   * version number of a locked orec.  This also makes lock release easier.
   */
  template <class T>
  struct orec_t
  {
      owner_version_t<T> v; // the current version number or a locked owner*
      owner_version_t<T> p; // the previous version number
  };

  /**
   *  Store an entry in the read set.  We don't care about actual addresses
   *  read, we only care about the orecs we've acquired.
   */
  template <class T>
  struct rset_t
  {
      orec_t<T>*  orec;    // the address of the orec we read
      unsigned version;    // the version of the orec that we observed

      /***  Basic constructor */
      rset_t(orec_t<T>* o, unsigned v) : orec(o), version(v) { }

      // for IterableMap support
      bool operator==(const rset_t& rhs) const
      {
          return ((orec == rhs.orec) && (version == rhs.version));
      }

      void* const getKey() const { return orec; }
  };

  /**
   *  Padded unsigned for keeping each entry of an unsigned array in its own
   *  cache line
   */
  struct padded_unsigned_t
  {
      volatile unsigned long val;
      char pad[64-sizeof(unsigned)];
  };

  /*** double-padded volatile unsigned long */
  struct double_padded_unsigned_t
  {
      char pad1[128];
      volatile unsigned long val;
      char pad2[128];
  };

} // stm

#endif // WBMETADATA_H__
