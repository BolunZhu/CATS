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
//

#ifndef WORDBASED_API_HPP
#define WORDBASED_API_HPP

#include <alt-license/static_check.h>
#include <alt-license/TypeManip.h>
#include "../support/DescriptorPolicy.hpp"
#include <string>

//-----------------------------------------------------------------------------
// API_ASSERT, API_ASSERT_UNPTR
//-----------------------------------------------------------------------------
// Set up a customizable assertion mechanism for the API
//   -- this lets us turn off costly asserts in the API without having to
//      disable all asserts in the user's code.
//-----------------------------------------------------------------------------
#if defined(STM_API_ASSERTS_ON)
#   define API_ASSERT(x) assert(x)
#   define API_ASSERT_UNPTR(x) assert(x)
#elif defined(STM_API_ASSERTS_OFF)
#   define API_ASSERT(x)
#   define API_ASSERT_UNPTR(x)
#elif defined(STM_API_ASSERTS_ON_NO_UN_PTR)
#   define API_ASSERT(x) assert(x)
#   define API_ASSERT_UNPTR(x)
#endif


namespace stm {

//=============================================================================
// Define most of the client's public stm:: API -- the definition of
// stm::tx_delete must wait until the end of the file due to some
// dependencies.
//
// These implementations mostly just forward to the descriptor's methods.
//=============================================================================


//-----------------------------------------------------------------------------
// Object-Based Compatibility
//-----------------------------------------------------------------------------
typedef mm::CustomAllocedBase Object;


//-----------------------------------------------------------------------------
// TM System Interface
//-----------------------------------------------------------------------------
inline void init(std::string x, std::string y, bool z) {
  Descriptor::init(x, y, z);
}
inline void shutdown(unsigned long i) { Descriptor::Self->dumpstats(i); }
inline bool not_in_transaction() {
  return !Descriptor::Self->isTransactional();
}


//-----------------------------------------------------------------------------
// Memory Management Interface
//-----------------------------------------------------------------------------
inline void* tx_alloc(size_t size) { return Descriptor::alloc(size); }
inline void tx_free(void* p)       { Descriptor::txfree(p); }


//-----------------------------------------------------------------------------
// Privatization Interface
//-----------------------------------------------------------------------------
inline void fence()         { Descriptor::fence(); }
inline void acquire_fence() { Descriptor::acquire_fence(); }
inline void release_fence() { Descriptor::release_fence(); }


//-----------------------------------------------------------------------------
// Inevitability Interface
//-----------------------------------------------------------------------------
inline bool try_inevitable() { return Descriptor::Self->try_inevitable(); }
inline void inev_read_prefetch(const void* addr, unsigned bytes) {
  Descriptor::inev_read_prefetch(addr, bytes);
}
inline void inev_write_prefetch(const void* addr, unsigned bytes) {
  Descriptor::inev_write_prefetch(addr, bytes);
}


//-----------------------------------------------------------------------------
// Transactional Control Interface
//-----------------------------------------------------------------------------
inline void retry() { Descriptor::Self->retry(); }
inline void halt_retry() { Descriptor::halt_retry(); }
inline void setPrio(int i) { Descriptor::setPrio(i); }
inline void restart() { Descriptor::Self->restart(); }

} // namespace stm


//=============================================================================
// Macro Definitions
//=============================================================================


//-----------------------------------------------------------------------------
// BEGIN_TRANSACTION, BEGIN_READONLY_TRANSACTION, END_TRANSACTION
//-----------------------------------------------------------------------------
//   Set up macros for how to begin and end transactions. These macros depend
//   on the configured rollback option.
//-----------------------------------------------------------------------------


#if defined(STM_ROLLBACK_NONE)
//-----------------------------------------------------------------------------
// STM_ROLLBACK_NONE
//-----------------------------------------------------------------------------
//   This version is UNSAFE in real transactional code.
//
//   This version of the BEGIN, BEGIN_READONLY, and END_TRANSACTION macros
//   assumes that the transaction never rolls back. This means that we cannot
//   have an explicit retry(), and that we can't ever actually abort.
//
//   This configuration is primarily useful for performance overhead testing
//   purposes.
//-----------------------------------------------------------------------------
#define BEGIN_TRANSACTION                         \
  {                                               \
    stm::Descriptor& tx = *stm::Descriptor::Self; \
    tx.beginTransaction();                        \
    {

#define BEGIN_READONLY_TRANSACTION BEGIN_TRANSACTION

#define END_TRANSACTION                           \
    }                                             \
    tx.commit();                                  \
  }

#elif defined(STM_ROLLBACK_SETJMP)
//-----------------------------------------------------------------------------
// STM_ROLLBACK_SETJMP
//-----------------------------------------------------------------------------
//   This is the standard rollback mechanism. This puts some rollback overhead
//   up-front by checkpointing registers via setjmp before beginning the
//   transaction.
//
//   This is not compliant with C++, because on rollback, the destructors of
//   stack objects will not be called, however it is likely to be OK. Any
//   constructors with side-effects that would be globally visible (*including
//   I/O*) need to derive from the stm::Object class, however classes deriving
//   from stm::Object are /required/ to have trivial constructors, so skipping
//   their destructors will be ok.
//
//   Objects that have local-only effects will not be rolled back correctly.
//
//   Note: the while(true) loop should not be necessary because the
//         setjmp/longjmp abort mechanism should encode the proper control
//         flow, however some of the platforms that we support fail to work
//         properly without the loop protection, possibly due to aggressive
//         compiler optimization.
//-----------------------------------------------------------------------------
#define BEGIN_TRANSACTION                           \
  {                                                 \
    stm::Descriptor& tx = *stm::Descriptor::Self;   \
    while (true) {                                  \
      jmp_buf _jmpbuf;                              \
      setjmp(_jmpbuf);                              \
      tx.beginTransaction(&_jmpbuf);                \
      {

#define BEGIN_READONLY_TRANSACTION BEGIN_TRANSACTION

#define END_TRANSACTION                             \
      }                                             \
      tx.commit();                                  \
      break;                                        \
    }                                               \
  }

#elif defined(STM_ROLLBACK_THROW)
//-----------------------------------------------------------------------------
// STM_ROLLBACK_THROW
//-----------------------------------------------------------------------------
//   Puts the overhead at the abort point.  We use try/catch to do the
//   rollback, and then on the catch, we have to do work to restore the state
//   that we had at the begin point. This is the correct way to do things in
//   C++. Unfortunately, in GNU C++, this mechanism doesn't scale as well as
//   setjmp, because the g++ runtime has a reader-writer lock protecting a
//   single global list that stores compressed DWARF data for handling stack
//   unwinding. So aborts cause serialization, convoying, and cache
//   thrashing.
//
//   The "correct for C++" claim is a little bit iffy... we use catch (...) to
//   abort/unwind the transaction and then propagate the exception, which may
//   not always be what the programmer expects. This can lead to memory access
//   problems if thrown exceptions point to stack locations or transactionally
//   allocated data.
//-----------------------------------------------------------------------------
#define BEGIN_TRANSACTION                             \
  {                                                   \
    stm::Descriptor& tx = *stm::Descriptor::Self;     \
    while (true) {                                    \
      try {                                           \
        tx.beginTransaction();                        \
        {

#define BEGIN_READONLY_TRANSACTION BEGIN_TRANSACTION

#define END_TRANSACTION                               \
        }                                             \
        tx.commit();                                  \
        break;                                        \
      } catch (stm::RollBack) {                       \
      } catch (...) {                                 \
        tx.onError();                                 \
        throw;                                        \
      }                                               \
      tx.unwind();                                    \
    }                                                 \
  }

#else
#error "Invalid STM_ROLLBACK option, please reconfigure and rebuild"
#endif // STM_ROLLBACK_*

//-----------------------------------------------------------------------------
// Field Accessor Interface
//-----------------------------------------------------------------------------
//   Set up getters and setters for accessing transactional objects.
//
//   http://www.cs.rochester.edu/research/synchronization/rstm/api.shtml#object_fields
//-----------------------------------------------------------------------------

#define RSTM_CONCAT(a,b)  a ## b

//-----------------------------------------------------------------------------
// GENERATE_FIELD
//-----------------------------------------------------------------------------
//   Using the type t and the name n, both parameters to the macro, generate a
//   protected declaration for the field (t m_name), as well as public getters
//   and setters.
//-----------------------------------------------------------------------------
#define GENERATE_FIELD(t, n)                                            \
protected: t RSTM_CONCAT(m_, n);                                        \
public:                                                                 \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 t RSTM_CONCAT(get_, n)(const stm::rd_ptr<RSTM_UNIQUE>& rdp) const {    \
   API_ASSERT(stm::Descriptor::Self->isTransactional());                \
   return rdp.getDescriptor().stm_read(&RSTM_CONCAT(m_, n));            \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 t RSTM_CONCAT(get_, n)(const stm::wr_ptr<RSTM_UNIQUE>& wrp) const {    \
   API_ASSERT(stm::Descriptor::Self->isTransactional());                \
   return wrp.getDescriptor().stm_read(&RSTM_CONCAT(m_, n));            \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 t RSTM_CONCAT(get_, n)(const stm::un_ptr<RSTM_UNIQUE>&) const {        \
   API_ASSERT_UNPTR(!stm::Descriptor::Self->isTransactional());         \
   return RSTM_CONCAT(m_, n);                                           \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 void RSTM_CONCAT(set_, n)(t RSTM_CONCAT(tmp_, n),                      \
                           const stm::wr_ptr<RSTM_UNIQUE>& wrp) {       \
   API_ASSERT(stm::Descriptor::Self->isTransactional());                \
   wrp.getDescriptor().stm_write(&RSTM_CONCAT(m_, n),                   \
                                 RSTM_CONCAT(tmp_, n));                 \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 void RSTM_CONCAT(set_, n)(t RSTM_CONCAT(tmp_, n),                      \
                           const stm::un_ptr<RSTM_UNIQUE>& unp) {       \
   API_ASSERT_UNPTR(!stm::Descriptor::Self->isTransactional());         \
   RSTM_CONCAT(m_, n) = RSTM_CONCAT(tmp_, n);                           \
 }


//-----------------------------------------------------------------------------
// GENERATE_ARRAY
//-----------------------------------------------------------------------------
//   Using the type t and the name n, both parameters to the macro, generate a
//   protected declaration for a field that is a fixed length array of size s
//   (t m_name[s]), as well as public getters and setters.
//-----------------------------------------------------------------------------
#define GENERATE_ARRAY(t, n, s)                                         \
protected: t RSTM_CONCAT(m_, n)[s];                                     \
public:                                                                 \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 t RSTM_CONCAT(get_, n)(int i,  const stm::rd_ptr<RSTM_UNIQUE>& rdp) const { \
   API_ASSERT(stm::Descriptor::Self->isTransactional());                \
   return rdp.getDescriptor().stm_read(&RSTM_CONCAT(m_, n)[i]);         \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 t RSTM_CONCAT(get_, n)(int i, const stm::wr_ptr<RSTM_UNIQUE>& wrp) const { \
   API_ASSERT(stm::Descriptor::Self->isTransactional());                \
   return wrp.getDescriptor().stm_read(&RSTM_CONCAT(m_, n)[i]);         \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 t RSTM_CONCAT(get_, n)(int i, const stm::un_ptr<RSTM_UNIQUE>&) const { \
   API_ASSERT_UNPTR(!stm::Descriptor::Self->isTransactional());         \
   return RSTM_CONCAT(m_, n)[i];                                        \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 void RSTM_CONCAT(set_, n)(int i, t RSTM_CONCAT(tmp_, n),               \
                           const stm::wr_ptr<RSTM_UNIQUE>& wrp) {       \
   API_ASSERT(stm::Descriptor::Self->isTransactional());                \
   wrp.getDescriptor().stm_write(&RSTM_CONCAT(m_, n)[i],                \
                                 RSTM_CONCAT(tmp_, n));                 \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 void RSTM_CONCAT(set_, n)(int i, t RSTM_CONCAT(tmp_, n),               \
                           const stm::un_ptr<RSTM_UNIQUE>&) {           \
   API_ASSERT_UNPTR(!stm::Descriptor::Self->isTransactional());         \
   RSTM_CONCAT(m_, n)[i] = RSTM_CONCAT(tmp_, n);                        \
 }


//-----------------------------------------------------------------------------
// GENERATE_2DARRAY
//-----------------------------------------------------------------------------
//   Using the type t and the name n, both parameters to the macro, generate a
//   protected declaration for a field that is a fixed length 2-D array of size
//   xx by yy (t m_name[xx][yy]), as well as public getters and setters.
//-----------------------------------------------------------------------------
#define GENERATE_2DARRAY(t, n, xx, yy)                                  \
protected: t RSTM_CONCAT(m_, n)[xx][yy];                                \
public:                                                                 \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 t RSTM_CONCAT(get_, n)(int x, int y,                                   \
                        const stm::rd_ptr<RSTM_UNIQUE>& rdp) const {    \
   API_ASSERT(stm::Descriptor::Self->isTransactional());                \
   return rdp.getDescriptor().stm_read(&RSTM_CONCAT(m_, n)[x][y]);      \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 t RSTM_CONCAT(get_, n)(int x, int y,                                   \
                        const stm::wr_ptr<RSTM_UNIQUE>& wrp) const {    \
   API_ASSERT(stm::Descriptor::Self->isTransactional());                \
   return wrp.getDescriptor().stm_read(&RSTM_CONCAT(m_, n)[x][y]);      \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 t RSTM_CONCAT(get_, n)(int x, int y, const stm::un_ptr<RSTM_UNIQUE>&) const { \
   API_ASSERT_UNPTR(!stm::Descriptor::Self->isTransactional());         \
   return RSTM_CONCAT(m_, n)[x][y];                                     \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 void RSTM_CONCAT(set_, n)(int x, int y, t RSTM_CONCAT(tmp_, n),        \
                           const stm::wr_ptr<RSTM_UNIQUE>& wrp) {       \
   API_ASSERT(stm::Descriptor::Self->isTransactional());                \
   wrp.getDescriptor().stm_write(&RSTM_CONCAT(m_, n)[x][y],             \
                                 RSTM_CONCAT(tmp_, n));                 \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 __attribute__((always_inline))                                         \
 void RSTM_CONCAT(set_, n)(int x, int y, t RSTM_CONCAT(tmp_, n),        \
                           const stm::un_ptr<RSTM_UNIQUE>&) {           \
   API_ASSERT_UNPTR(!stm::Descriptor::Self->isTransactional());         \
   RSTM_CONCAT(m_, n)[x][y] = RSTM_CONCAT(tmp_, n);                     \
 }

namespace stm {

//=============================================================================
// Smart Pointers
//=============================================================================

//-----------------------------------------------------------------------------
// sh_ptr<T>, rd_ptr<T>, wr_ptr<T>, un_ptr<T>
//-----------------------------------------------------------------------------
//   These provide hooks for first read and write access, as well as
//   nontransactional interaction.
//
//   http://www.cs.rochester.edu/research/synchronization/rstm/api.shtml#smart_pointers
//-----------------------------------------------------------------------------
template <class T>
class sh_ptr {
  // This class is used internally to provide the ability to use an <code>if
  // (sh_ptr)</code> style test. See operator try_tx_delete_instead.
  class try_tx_delete_instead {
    void operator delete(void*);
  };

 protected:
  T* m_obj;

 public:
  /*** This default constructor initializes a "NULL" shared pointer. */
  sh_ptr() : m_obj(NULL) {
  }

  /**
   * This constructor is the primary way that something becomes "shared" in the
   * transactional framework.
   *
   * NOTE: the pointer that you pass into this call becomes semantically NULL
   *       after the call. The transactional world takes control and
   *       responsiblity for the pointer for the rest of the object's
   *       life. Using the naked pointer can lead to undefined results.
   *
   * We anticipate that a shared object will generally be created with a call
   * that looks like:
   *
   *    sh_ptr<T> shared(new T());
   *
   */
  explicit sh_ptr(T* obj) : m_obj(obj) {
    LOKI_STATIC_CHECK(LOKI_SUPERSUBCLASS(stm::Object, T), INVALID_SHARED_TYPE);
  }

  /**
   * This allows sh_ptrs to act sort of polymorphically based on the pointed to
   * type. Essentially, one can use an sh_ptr<Subclass> anywhere an
   * sh_ptr<Superclass> is expected.
   */
  template <class Super>
  operator sh_ptr<Super>() {
    // No fancy checks here. The sh_ptr<Super> constructor will only pass type
    // checking if T inherits from Super.
    return sh_ptr<Super>(m_obj);
  }

  /*** Allows tests of the form <code>if (!sh_ptr)</code>. */
  bool operator!() const {
    return m_obj == NULL;
  }

  /**
   * Allows tests of the form <code>if (sh_ptr < sh_ptr)</code> so that
   * <code>sh_ptr</code>s can be used in sorted containers.
   */
  bool operator<(const sh_ptr<T>& r) const {
    return (m_obj < r.m_obj);
  }

  /**
   * Allows tests of the form <code>if (sh_ptr == sh_ptr)</code>.  Equivalent
   * to <code>if (T* == T*)</code>. This is a pointer comparison, so it won't
   * call the <code>T<code>'s <code>operator==</code> if it exists. To do that,
   * use <code>(*sh_ptr == *sh_ptr)</code>.
   */
  bool operator==(const sh_ptr<T>& r) const {
    return m_obj == r.m_obj;
  }

  /**
   * Allows tests of the form <code>if (sh_ptr != sh_ptr)</code>.  Equivalent
   * to <code>if (T* != T*)</code>. This is a pointer comparison, so it won't
   * call the <code>T<code>'s <code>operator!=</code> if it exists. To do that,
   * use <code>(*sh_ptr != *sh_ptr)</code>.
   */
  bool operator!=(const sh_ptr<T>& r) const {
    return m_obj != r.m_obj;
  }

  /*** Allows tests of the form <code>if (sh_ptr == NULL)</code>. */
  bool operator==(const T* r) const {
    return m_obj == r;
  }

  /*** Allows tests of the form <code>if (sh_ptr != NULL)</code>. */
  bool operator!=(const T* r) const {
    return m_obj != r;
  }

  /**
   * Allows tests of the form <code>if (NULL == sh_ptr)</code>. Just forwards
   * to <code>(sh_ptr == NULL)</code>. Technically this allows tests against
   * any naked <code>T*</code> pointers, though we don't expect that that will
   * happen.
   */
  inline friend bool operator==(const T* l, const sh_ptr<T>& r) {
    return r == l;
  }

  /**
   * Allows tests of the form <code>if (NULL != sh_ptr)</code>. Just forwards
   * to <code>(sh_ptr != NULL). Technically this also allows tests against
   * naked <code>T*</code> pointers, though we don't expect any use for that.
   */
  inline friend bool operator!=(const T* l, const sh_ptr<T>& r) {
    return r != l;
  }

  /**
   * Allows tests of the form <code>if (sh_ptr)</code>. The
   * <code>try_tx_delete_instead</code> is a helper class that we provide an
   * implicit conversion to. The compiler matches this implicit conversion. The
   * <code>try_tx_delete</code> class hass a private destructor, so a call of
   * <code>delete sh_ptr</code> (which matches this conversion too) will fail
   * at compile time.
   *
   * This comes straight out of Andrei Alexandrescu's book "Modern C++ Design:
   * Generic Programming and Design Patterns Applied." The chapter on smart
   * pointers was released publically online.
   */
  operator try_tx_delete_instead*() const {
    static try_tx_delete_instead test;
    return  (m_obj == NULL) ? NULL : &test;
  }

  inline friend void tx_delete(sh_ptr<T>& shared) {
    tx_free(shared.m_obj);
  }
}; // template class stm::sh_ptr


template <class T>
class rd_ptr : public sh_ptr<T>,
               public stm::WBDescriptorPolicy<stm::Descriptor> {

  typedef stm::WBDescriptorPolicy<stm::Descriptor> DP;

 public:
  /*** No magic here, just initializes a NULL rd_ptr. */
  rd_ptr() : sh_ptr<T>(), DP() {
  }

  /**
   * Semantically opens the shared pointer for reading. Worbased libraries
   * don't actually need to do anything here.
   */
  explicit rd_ptr(const sh_ptr<T>& open) : sh_ptr<T>(open), DP() {
  }

  /**
   * Semantically opens the shared pointer for reading. Worbased libraries
   * don't actually need to do anything here.
   */
  rd_ptr<T>& operator=(const sh_ptr<T>& open) {
    // Just uses the automatically generated operator=() for an sh_ptr. This is
    // so that we don't need to give rd_ptr friend access to sh_ptr. Maybe
    // complicates this call, but makes the big picture cleaner.
    *static_cast<sh_ptr<T>*>(this) = open;
    return *this;
  }

  /*** The main call that makes this a "read only" smart pointer. */
  const T* operator->() const {
    return sh_ptr<T>::m_obj;
  }

  /**
   * Needs to be used occasionally if you need to compare the "pointed to
   * things" with a custom operator.
   *
   *    if (*rd1 == *rd2) { ... }
   *
   * It's dangerous in general to cache this reference, so avoid code that
   * looks like:
   *
   *    T& ref = *rdp;
   */
  const T& operator*() const {
    return *sh_ptr<T>::m_obj;
  }

  inline friend void tx_release(rd_ptr<T>& to_release) {
  }
}; // template class stm::rd_ptr


template <class T>
class wr_ptr : public rd_ptr<T> {
 public:
  typedef rd_ptr<T> rd;

  /*** Again, nothing magic. Just initializes a NULL write pointer. */
  wr_ptr() : rd_ptr<T>() {
  }

  /**
   * Semantically an "open for writing" however our wordbased libraries don't
   * need to do anything here.
   */
  explicit wr_ptr(const sh_ptr<T>& open) : rd_ptr<T>(open) {
  }

  /**
   * Semantically an "open for writing" however our wordbased libraries don't
   * need to do anything here.
   */
  explicit wr_ptr(const rd_ptr<T>& upgrade) : rd_ptr<T>(upgrade) {
  }

  /**
   * Semantically an "open for writing" however our wordbased libraries don't
   * need to do anything here.
   */
  wr_ptr<T>& operator=(const sh_ptr<T>& open) {
    // Just uses the automatically generated operator=() for an sh_ptr. This is
    // so that we don't need to give wr_ptr friend access to sh_ptr. Maybe
    // complicates this call, but makes the big picture cleaner.
    *static_cast<sh_ptr<T>*>(this) = open;
    return *this;
  }

  /**
   * Semantically an "open for writing" however our wordbased libraries don't
   * need to do anything here.
   */
  wr_ptr<T>& operator=(const rd_ptr<T>& upgrade) {
    // Just uses the automatically generated operator=() for an rd_ptr. This is
    // so that we don't need to give wr_ptr friend access to rd_ptr. Maybe
    // complicates this call, but makes the big picture cleaner.
    *static_cast<rd_ptr<T>*>(this) = upgrade;
    return *this;
  }

  /**
   * This is what makes this class a "writable" smart pointer. Return a
   * non-const pointer to my object.
   */
  T* operator->() const {
    return sh_ptr<T>::m_obj;
  }

  /**
   * Needs to be used occasionally if you need to compare the "pointed to
   * things" with a custom operator.
   *
   *    if (*wr1 == *wr2) { ... }
   *
   * It's dangerous in general to cache this reference, so avoid code that
   * looks like:
   *
   *    T& ref = *wrp;
   */
  T& operator*() const {
    return *sh_ptr<T>::m_obj;
  }
}; // template class stm::wr_ptr

template <class T>
class un_ptr : public sh_ptr<T> {
 public:
  typedef un_ptr<T> rd;

  /*** Nothing fancy going on here. Initializes a NULL un_ptr. */
  un_ptr() : sh_ptr<T>() {
  }

  /**
   * Semantically an "open for private access" but our wordbased libraries
   * don't need to do anything fancy here.
   */
  explicit un_ptr(const sh_ptr<T>& open) : sh_ptr<T>(open) {
  }

  /**
   * Semantically an "open for private access" but our wordbased libraries
   * don't need to do anything fancy here.
   */
  un_ptr<T>& operator=(const sh_ptr<T>& open) {
    // Just uses the automatically generated operator=() for an sh_ptr. This is
    // so that we don't need to give un_ptr friend access to sh_ptr. Maybe
    // complicates this call, but makes the big picture cleaner.
    *static_cast<sh_ptr<T>*>(this) = open;
    return *this;
  }

  /*** This call is what makes this class a smart pointer. */
  const T* operator->() const {
    return sh_ptr<T>::m_obj;
  }

  T* operator->() {
    return sh_ptr<T>::m_obj;
  }

  /**
   * Needs to be used occasionally if you need to compare the "pointed to
   * things" with a custom operator.
   *
   *    if (*un1 == *un2) { ... }
   *
   * It's dangerous in general to cache this reference, so avoid code that
   * looks like:
   *
   *    T& ref = *unp;
   */
  const T& operator*() const {
    return *sh_ptr<T>::m_obj;
  }

  T& operator*() {
    return *sh_ptr<T>::m_obj;
  }
}; // template class stm::un_ptr


//=============================================================================
// Define the rest of the client's public stm:: API
//=============================================================================


//-----------------------------------------------------------------------------
// Memory Management Interface
//-----------------------------------------------------------------------------
template <class T>
inline void tx_delete(sh_ptr<T>& shared) {
  // Constructor will fail if called outside of a transaction, clients need to
  // use an un_ptr in that context.
  rd_ptr<T> rd(shared);
  tx_delete(rd);
}

} // namespace stm

#endif // WORDBASED_API_HPP
