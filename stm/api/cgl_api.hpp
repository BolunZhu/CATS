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

#ifndef CGL_API_H
#define CGL_API_H

#include <string>
#include <cassert>

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


//=============================================================================
// Macro Definitions
//=============================================================================


//-----------------------------------------------------------------------------
// BEGIN_TRANSACTION, BEGIN_READONLY_TRANSACTION, END_TRANSACTION
//-----------------------------------------------------------------------------
//   Set up macros for how to begin and end transactions. This is really simply
//   in since there can't be any rollback. We just open a scope so that we can
//   reuse the 'tx' name, and then begin the transaction. We open another scope
//   to hide the 'tx' name from the client.
//-----------------------------------------------------------------------------
#define BEGIN_TRANSACTION                           \
  {                                                 \
    stm::Descriptor& tx = *stm::Descriptor::Self;   \
    tx.beginTransaction(true);                      \
    {

#define BEGIN_READONLY_TRANSACTION                  \
  {                                                 \
    stm::Descriptor& tx = *stm::Descriptor::Self;   \
    tx.beginTransaction(false);                     \
    {

#define END_TRANSACTION                             \
    }                                               \
    tx.endTransaction();                            \
  }


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
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 t RSTM_CONCAT(get_, n)(const stm::rd_ptr<RSTM_UNIQUE>&) const {        \
   return RSTM_CONCAT(m_, n);                                           \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 t RSTM_CONCAT(get_, n)(const stm::un_ptr<RSTM_UNIQUE>&) const {        \
   return RSTM_CONCAT(m_, n);                                           \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 void RSTM_CONCAT(set_, n)(t RSTM_CONCAT(tmp_, n),                      \
                           const stm::wr_ptr<RSTM_UNIQUE>&) {           \
   RSTM_CONCAT(m_, n) = RSTM_CONCAT(tmp_, n);                           \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 void RSTM_CONCAT(set_, n)(t RSTM_CONCAT(tmp_, n),                      \
                           const stm::un_ptr<RSTM_UNIQUE>&) {           \
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
 t RSTM_CONCAT(get_, n)(int i, const stm::rd_ptr<RSTM_UNIQUE>&) const { \
   return RSTM_CONCAT(m_, n)[i];                                        \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 t RSTM_CONCAT(get_, n)(int i, const stm::un_ptr<RSTM_UNIQUE>&) const { \
   return RSTM_CONCAT(m_, n)[i];                                        \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 void RSTM_CONCAT(set_, n)(int i, t RSTM_CONCAT(tmp_, n),               \
                           const stm::wr_ptr<RSTM_UNIQUE>&) {           \
   RSTM_CONCAT(m_, n)[i] = RSTM_CONCAT(tmp_, n);                        \
 }                                                                      \
                                                                        \
 template <class RSTM_UNIQUE>                                           \
 void RSTM_CONCAT(set_, n)(int i, t RSTM_CONCAT(tmp_, n),               \
                           const stm::un_ptr<RSTM_UNIQUE>&) {           \
   RSTM_CONCAT(m_, n)[i] = RSTM_CONCAT(tmp_, n);                        \
 }


//-----------------------------------------------------------------------------
// GENERATE_2DARRAY
//-----------------------------------------------------------------------------
//   Using the type t and the name n, both parameters to the macro, generate a
//   protected declaration for a field that is a fixed length 2-D array of size
//   xx by yy (t m_name[xx][yy]), as well as public getters and setters.
//-----------------------------------------------------------------------------
#define GENERATE_2DARRAY(t, n, xx, yy)                              \
protected: t RSTM_CONCAT(m_, n)[xx][yy];                            \
public:                                                             \
                                                                    \
 template <class RSTM_UNIQUE>                                       \
 t RSTM_CONCAT(get_, n)(int x, int y,                               \
                        const stm::rd_ptr<RSTM_UNIQUE>&) const {    \
   return RSTM_CONCAT(m_, n)[x][y];                                 \
 }                                                                  \
                                                                    \
 /* un pointers don't inherit from rd pointers */                   \
 template <class RSTM_UNIQUE>                                       \
 t RSTM_CONCAT(get_, n)(int x, int y,                               \
                        const stm::un_ptr<RSTM_UNIQUE>&) const {    \
   return RSTM_CONCAT(m_, n)[x][y];                                 \
 }                                                                  \
                                                                    \
 template <class RSTM_UNIQUE>                                       \
 void RSTM_CONCAT(set_, n)(int x, int y, t RSTM_CONCAT(tmp_, n),    \
                           const stm::wr_ptr<RSTM_UNIQUE>&) {       \
   RSTM_CONCAT(m_, n)[x][y] = RSTM_CONCAT(tmp_, n);                 \
 }                                                                  \
                                                                    \
 template <class RSTM_UNIQUE>                                       \
 void RSTM_CONCAT(set_, n)(int x, int y, t RSTM_CONCAT(tmp_, n),    \
                           const stm::un_ptr<RSTM_UNIQUE>&) {       \
   RSTM_CONCAT(m_, n)[x][y] = RSTM_CONCAT(tmp_, n);                 \
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
  class try_tx_delete_instead { void operator delete(void*); };

 protected:
  T* m_obj;

 public:
  /*** This default constructor initializes a "NULL" shared pointer. */
  sh_ptr() : m_obj(NULL) { }

  /**
   * This constructor is the primary way that something becomes "shared"
   * in the transactional framework.
   *
   * NOTE: the pointer that you pass into this call becomes semantically
   *       NULL after the call. The transactional world takes control and
   *       responsiblity for the pointer for the rest of the object's
   *       life. Using the naked pointer can lead to undefined results.
   *
   * We anticipate that a shared object will generally be created with a
   * call that looks like:
   *
   *    sh_ptr<T> shared(new T());
   *
   */
  explicit sh_ptr(T* obj) : m_obj(obj) { }

  /**
   * This allows sh_ptrs to act sort of polymorphically based on the pointed to
   * type. Essentially, one can use an sh_ptr<Subclass> anywhere an
   * sh_ptr<Superclass> is expected.
   */
  template <class Super>
  operator sh_ptr<Super>() {
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
   * pointers was released publicly online.
   */
  operator try_tx_delete_instead*() const {
    static try_tx_delete_instead test;
    return  (m_obj == NULL) ? NULL : &test;
  }

  inline friend void tx_delete(sh_ptr<T>& shared) {
    delete shared.m_obj;
  }
};



template <class T>
class rd_ptr : public sh_ptr<T> {
 public:
  /*** No magic here, just initializes a NULL rd_ptr. */
  rd_ptr() : sh_ptr<T>() { }

  /**
   * Semantically opens the shared pointer for reading. CGL doesn't actually
   * need to do anything here.
   */
  explicit rd_ptr(const sh_ptr<T>& open) : sh_ptr<T>(open) {
  }

  /**
   * Semantically opens the shared pointer for reading. CGL doesn't actually
   * need to do anything here.
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
};



template <class T>
class wr_ptr : public rd_ptr<T> {
 public:
  typedef rd_ptr<T> rd;

  /*** Again, nothing magic. Just initializes a NULL write pointer. */
  wr_ptr() : rd_ptr<T>() {
  }

  /**
   * Semantically an "open for writing" however CGl doesn't need to do anything
   * here because there is no cconcurrency.
   */
  explicit wr_ptr(const sh_ptr<T>& open) : rd_ptr<T>(open) {
  }

  /**
   * Semantically an "upgrade to writable" however CGL doesn't need to do
   * anything here because there is no concurrency.
   */
  explicit wr_ptr(const rd_ptr<T>& upgrade) : rd_ptr<T>(upgrade) {
  }

  /**
   * Semantically an "open for writing" however CGl doesn't need to do anything
   * here because there is no cconcurrency.
   */
  wr_ptr<T>& operator=(const sh_ptr<T>& open) {
    // Just uses the automatically generated operator=() for an sh_ptr. This is
    // so that we don't need to give wr_ptr friend access to sh_ptr. Maybe
    // complicates this call, but makes the big picture cleaner.
    *static_cast<sh_ptr<T>*>(this) = open;
    return *this;
  }

  /**
   * Semantically an "upgrade to writable" however CGL doesn't need to do
   * anything here because there is no concurrency.
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
};



template <class T>
class un_ptr : public sh_ptr<T> {
 public:
  typedef un_ptr<T> rd;

  /*** Nothing fancy going on here. Initializes a NULL un_ptr. */
  un_ptr() : sh_ptr<T>() {
  }

  /**
   * Semantically an "open for private access" but CGL doesn't need to do
   * anything fancy here.
   */
  explicit un_ptr(const sh_ptr<T>& open) : sh_ptr<T>(open) {
  }

  /**
   * Semantically an "open for private access" but CGL doesn't need to do
   * anything fancy here.
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

  /*** Returns a writable version. */
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
  T& operator*() {
    return *sh_ptr<T>::m_obj;
  }
};


//=============================================================================
// Define the rest of the client's public stm:: API
//=============================================================================


//-----------------------------------------------------------------------------
// Object-Based Compatibility
//-----------------------------------------------------------------------------
typedef mm::CustomAllocedBase Object;


//-----------------------------------------------------------------------------
// Memory Management Interface
//-----------------------------------------------------------------------------
inline void* tx_alloc(size_t size)  { return mm::txalloc(size); }
inline void  tx_free(void* ptr, size_t size) { mm::txfree(ptr); }
template <class T>
inline void tx_delete(sh_ptr<T>& shared) {
  // Constructor will fail if called outside of a transaction, clients need to
  // use an un_ptr in that context.
  rd_ptr<T> rd(shared);
  tx_delete(rd);
}


//-----------------------------------------------------------------------------
// TM System Interface
//-----------------------------------------------------------------------------
inline void init(std::string, std::string, bool)  { Descriptor::init(); }
inline void shutdown(unsigned long i) { Descriptor::Self->dumpStats(i); }
inline bool not_in_transaction() {
  return !Descriptor::Self->isTransactional();
}


//-----------------------------------------------------------------------------
// Privatization Interface
//-----------------------------------------------------------------------------
inline void fence() { }
inline void acquire_fence() { }
inline void release_fence() { }


//-----------------------------------------------------------------------------
// Inevitability Interface
//-----------------------------------------------------------------------------
inline bool try_inevitable() { return Descriptor::Self->try_inevitable(); }
inline void inev_read_prefetch(const void*, unsigned)  { }
inline void inev_write_prefetch(const void*, unsigned) { }


//-----------------------------------------------------------------------------
// Retry Interface
//-----------------------------------------------------------------------------
inline void retry() {
  assert(false && "retry() not supported by this runtime.");
}

} // namespace stm

#endif // CGL_API_H
