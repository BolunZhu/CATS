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

#ifndef RSTM_API_H
#define RSTM_API_H

#include <alt-license/static_check.h>
#include <alt-license/TypeManip.h>
#include "../support/DescriptorPolicy.hpp"

//-----------------------------------------------------------------------------
// API_ASSERT, API_ASSERT_UNPTR
//-----------------------------------------------------------------------------
// Set up a customizable assertion mechanism for the API
//   -- this lets us turn off costly asserts in the API without having to
//      disable all asserts in the user's code.
//-----------------------------------------------------------------------------
#if defined(STM_API_ASSERTS_ON)
#  define API_ASSERT(x) assert(x)
#  define API_ASSERT_UNPTR(x) assert(x)
#elif defined(STM_API_ASSERTS_OFF)
#  define API_ASSERT(x)
#  define API_ASSERT_UNPTR(x)
#elif defined(STM_API_ASSERTS_ON_NO_UN_PTR)
#  define API_ASSERT(x) assert(x)
#  define API_ASSERT_UNPTR(x)
#endif



//=============================================================================
// Macro Definitions
//=============================================================================


//-----------------------------------------------------------------------------
// BEGIN_TRANSACTION, BEGIN_READONLY_TRANSACTION, END_TRANSACTION
//-----------------------------------------------------------------------------
//   Set up macros for how to begin and end transactions. These macros depend
//   on the configured rollback option.
//-----------------------------------------------------------------------------
#if defined(STM_ROLLBACK_SETJMP)
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
#define BEGIN_TRANSACTION                                               \
    {                                                                   \
        rstm::Descriptor& tx = *rstm::currentDescriptor;                \
        while (true) {                                                  \
            jmp_buf _jmpbuf;                                            \
            setjmp(_jmpbuf);                                            \
            tx.begin_transaction(&_jmpbuf);                             \
            {

#define BEGIN_READONLY_TRANSACTION BEGIN_TRANSACTION

#define END_TRANSACTION                                                 \
            }                                                           \
            tx.commit();                                                \
            break;                                                      \
        }                                                               \
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
#define BEGIN_TRANSACTION                                       \
    {                                                           \
        rstm::Descriptor& tx = *rstm::currentDescriptor;        \
        while (true) {                                          \
            try {                                               \
                tx.begin_transaction();                         \
                {

#define BEGIN_READONLY_TRANSACTION BEGIN_TRANSACTION

#define END_TRANSACTION                                         \
                }                                               \
                tx.commit();                                    \
                break;                                          \
            } catch (stm::RollBack) {                           \
            } catch (...) {                                     \
                tx.onError();                                   \
                throw;                                          \
            }                                                   \
            tx.unwind();                                        \
        }                                                       \
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
                                                                        \
 /* reads through read pointers require instrumentation under */        \
 /* STM_PRIV_NONBLOCKING */                                             \
 template<class RSTM_UNIQUE>                                            \
 t RSTM_CONCAT(get_, n)(const stm::rd_ptr<RSTM_UNIQUE>& rdp) const {    \
   t ret = RSTM_CONCAT(m_, n);                                          \
   rdp.getDescriptor().check_privatizer_clock();                        \
   return ret;                                                          \
 }                                                                      \
                                                                        \
 /* reads through write pointers do not require instrumentation */      \
 template<class RSTM_UNIQUE>                                            \
 t RSTM_CONCAT(get_, n)(const stm::wr_ptr<RSTM_UNIQUE>&) const {        \
   return RSTM_CONCAT(m_, n);                                           \
 }                                                                      \
                                                                        \
 /* reads through un pointers do not require instrumentation */         \
 template<class RSTM_UNIQUE>                                            \
 t RSTM_CONCAT(get_, n)(const stm::un_ptr<RSTM_UNIQUE>&) const {        \
   return RSTM_CONCAT(m_, n);                                           \
 }                                                                      \
                                                                        \
 /* writes through write pointers do not require instrumentation */     \
 template <class RSTM_UNIQUE>                                           \
 void RSTM_CONCAT(set_, n)(t RSTM_CONCAT(tmp_, n),                      \
                           const stm::wr_ptr<RSTM_UNIQUE>&) {           \
   RSTM_CONCAT(m_, n) = RSTM_CONCAT(tmp_, n);                           \
 }                                                                      \
                                                                        \
 /* writes through un pointers do not require instrumentation */        \
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
 /* reads through read pointers require instrumentation under */        \
 /* STM_PRIV_NONBLOCKING */                                             \
 template<class RSTM_UNIQUE>                                            \
 t RSTM_CONCAT(get_, n)(int i, const stm::rd_ptr<RSTM_UNIQUE>& rdp) const { \
   t ret = RSTM_CONCAT(m_, n)[i];                                       \
   rdp.getDescriptor().check_privatizer_clock();                        \
   return ret;                                                          \
 }                                                                      \
                                                                        \
 /* reads through write pointers do not require instrumentation */      \
 template<class RSTM_UNIQUE>                                            \
 t RSTM_CONCAT(get_, n)(int i, const stm::wr_ptr<RSTM_UNIQUE>&) const { \
   return RSTM_CONCAT(m_, n)[i];                                        \
 }                                                                      \
                                                                        \
 /* reads through un pointers do not require instrumentation */         \
 template<class RSTM_UNIQUE>                                            \
 t RSTM_CONCAT(get_, n)(int i, const stm::un_ptr<RSTM_UNIQUE>&) const { \
   return RSTM_CONCAT(m_, n)[i];                                        \
 }                                                                      \
                                                                        \
 /* writes through write pointers do not require instrumentation */     \
 template <class RSTM_UNIQUE>                                           \
 void RSTM_CONCAT(set_, n)(int i, t RSTM_CONCAT(tmp_, n),               \
                           const stm::wr_ptr<RSTM_UNIQUE>&) {           \
   RSTM_CONCAT(m_, n)[i] = RSTM_CONCAT(tmp_, n);                        \
 }                                                                      \
                                                                        \
 /* writes through un pointers do not require instrumentation */        \
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
#define GENERATE_2DARRAY(t, n, xx, yy)                                  \
protected: t RSTM_CONCAT(m_, n)[xx][yy];                                \
public:                                                                 \
                                                                        \
 /* reads through read pointers require instrumentation under */        \
 /* STM_PRIV_NONBLOCKING */                                             \
 template<class RSTM_UNIQUE>                                            \
 t RSTM_CONCAT(get_, n)(int x, int y,                                   \
                        const stm::rd_ptr<RSTM_UNIQUE>& rdp) const {    \
   t ret = RSTM_CONCAT(m_, n)[x][y];                                    \
   rdp.getDescriptor().check_privatizer_clock();                        \
   return ret;                                                          \
 }                                                                      \
                                                                        \
 /* reads through write pointers do not require instrumentation */      \
 template<class RSTM_UNIQUE>                                            \
 t RSTM_CONCAT(get_, n)(int x, int y, const stm::wr_ptr<RSTM_UNIQUE>&) const { \
   return RSTM_CONCAT(m_, n)[x][y];                                     \
 }                                                                      \
                                                                        \
 /* reads through un pointers do not require instrumentation */         \
 template<class RSTM_UNIQUE>                                            \
 t RSTM_CONCAT(get_, n)(int x, int y, const stm::un_ptr<RSTM_UNIQUE>&) const { \
   return RSTM_CONCAT(m_, n)[x][y];                                     \
 }                                                                      \
                                                                        \
 /* writes through write pointers do not require instrumentation */     \
 template <class RSTM_UNIQUE>                                           \
 void RSTM_CONCAT(set_, n)(int x, int y, t RSTM_CONCAT(tmp_, n),        \
                           const stm::wr_ptr<RSTM_UNIQUE>&) {           \
   RSTM_CONCAT(m_, n)[x][y] = RSTM_CONCAT(tmp_, n);                     \
 }                                                                      \
                                                                        \
 /* writes through un pointers do not require instrumentation */        \
 template <class RSTM_UNIQUE>                                           \
 void RSTM_CONCAT(set_, n)(int x, int y, t RSTM_CONCAT(tmp_, n),        \
                           const stm::un_ptr<RSTM_UNIQUE>&) {           \
   RSTM_CONCAT(m_, n)[x][y] = RSTM_CONCAT(tmp_, n);                     \
 }

namespace stm {

//-----------------------------------------------------------------------------
// Import the Object class into the stm:: namespace
//-----------------------------------------------------------------------------
using rstm::Object;

//-----------------------------------------------------------------------------
// Some necessary forward declarations.
//-----------------------------------------------------------------------------
template <class> class sh_ptr;
template <class> class rd_ptr;
template <class> class wr_ptr;
template <class> class un_ptr;

template <class T> void tx_delete(sh_ptr<T>&);

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
  friend class rd_ptr<T>;
  friend class wr_ptr<T>;
  friend class un_ptr<T>;

  /**
   * Shared pointers to different types are all friends of each other. This
   * lets you use sh_ptr<Subclass> and sh_ptr<Superclass> in polymorphic
   * fashion, I think.
   */
  template <class Subclass> friend class sh_ptr;

  class try_tx_delete_instead {
    void operator delete(void*);
  };

 protected:
  rstm::SharedHandle* m_sh;

 public:
  /*** Default constructor initializes a <code>NULL sh_ptr</code>. */
  sh_ptr() : m_sh(NULL) {
  }

  /**
   * Explicit constructor. The passed pointer is no longer safe to use after
   * this routine. Usually used like <code>sh_ptr<T> s(new T())</code>.
   *
   * <strong>DO NOT INITIALIZE MORE THAN ONE <code>sh_ptr</code> WITH THE SAME
   * T*, AS THIS WILL LEAD TO AN ALIAS PROBLEM AND VERY DIFFICULT TO DEBUG
   * INCORRECT BEHAVIOR.</strong>
   *
   * Essentially, you are giving up control of the <code>T*</code> when you
   * call this.
   */
  explicit sh_ptr(T* obj) : m_sh(rstm::create_shared_handle(obj)) {
  }

  /**
   * This allows sh_ptrs to act sort of polymorphically based on the pointed to
   * type. Essentially, one can use an sh_ptr<Subclass> anywhere an
   * sh_ptr<Superclass> is expected.
   *
   * Keep in mind that if you open the resulting pointer for writing, it is no
   * longer safe to use previously opened rd_ptrs unless you have alias
   * detection and patching enabled (it is enabled by default).
   */
  template <class Super> operator sh_ptr<Super>() {
    // static assert that Super is a superclass of T
    LOKI_STATIC_CHECK( LOKI_SUPERSUBCLASS(Super, T), INVALID_CAST );
    sh_ptr<Super> ret();
    ret.m_sh = m_sh;
    return ret;
  }

  /*** Allows tests of the form <code>if (!sh_ptr)</code>. */
  bool operator!() const {
    return m_sh == NULL;
  }

  /**
   * Allows tests of the form <code>if (sh_ptr < sh_ptr)</code> so that
   * <code>sh_ptr</code>s can be used in sorted containers.
   */
  bool operator<(const sh_ptr<T>& r) const {
    return (m_sh < r.m_sh);
  }

  /**
   * Allows tests of the form <code>if (sh_ptr == sh_ptr)</code>.  Equivalent
   * to <code>if (T* == T*)</code>. This is a pointer comparison, so it won't
   * call the <code>T<code>'s <code>operator==</code> if it exists. To do that,
   * use <code>(*sh_ptr == *sh_ptr)</code>.
   */
  bool operator==(const sh_ptr<T>& r) const {
    return m_sh == r.m_sh;
  }

  /**
   * Allows tests of the form <code>if (sh_ptr != sh_ptr)</code>.  Equivalent
   * to <code>if (T* != T*)</code>. This is a pointer comparison, so it won't
   * call the <code>T<code>'s <code>operator!=</code> if it exists. To do that,
   * use <code>(*sh_ptr != *sh_ptr)</code>.
   */
  bool operator!=(const sh_ptr<T>& r) const {
    return m_sh != r.m_sh;
  }

  /*** Allows tests of the form <code>if (sh_ptr == NULL)</code>. */
  bool operator==(const void* r) const {
    return m_sh == r;
  }

  /*** Allows tests of the form <code>if (sh_ptr != NULL)</code>. */
  bool operator!=(const void* r) const {
    return m_sh != r;
  }

#if defined(_MSC_VER)
  // Visual C++ complains if we treat NULL as a void* instead of int
  bool operator==(int r) const { return m_sh == (void*)r; }
  bool operator!=(int r) const { return m_sh != (void*)r; }
#endif

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
    return  (m_sh == NULL) ? NULL : &test;
  }

 private:
  /**
   * Delete a shared object. We can't safely delete shared objects inside a
   * transaction directly, so we open it for reading first.
   *
   * This can only be safely called within a transactional context. If you want
   * to delete a shared object outside a transaction, use an un_ptr manually.
   *
   * NB: Implemented later because it depends on the <code>rd_ptr<code> code.
   */
  friend void tx_delete<T>(sh_ptr<T>& shared);
}; // template class stm::sh_ptr

template <class T>
class rd_ptr : public sh_ptr<T>,
               public stm::DescriptorPolicy<rstm::Descriptor> {
  friend class wr_ptr<T>;
  using sh_ptr<T>::m_sh;
  typedef stm::DescriptorPolicy<rstm::Descriptor> DP;

 protected:
  mutable T* m_obj;

 private:
  void open() const {
    m_obj = static_cast<T*>(DP::getDescriptor().openReadOnly(m_sh));
  }

 public:
  /**
   * The default constructor initializes a <code>NULL rd_ptr</code>. This
   * pointer can then be used as the target of an assignment from an
   * <code>sh_ptr</code>.
   *
   * This requires a thread local lookup, so a best practice is to avoid using
   * this constructor as much as possible. In particular, it is more efficient
   * to declare a <code>rd_ptr</code> once, outside of a loop, than to
   * continuously call this constructor inside a loop.
   */
  rd_ptr() : sh_ptr<T>(), DP(), m_obj(NULL) {
    API_ASSERT(DP::getDescriptor().inTransaction());

  }

  /**
   * A <code>rd_ptr</code> can be initialized directly from an
   * <code>sh_ptr</code>, which implies "open for reading" semantics. This is
   * the most efficient way to open a shared object for reading, if there is no
   * already available <code>rd_ptr</code> available for the assignment version
   * (<code>operator=(sh_ptr<T>&)</code>).
   */
  explicit rd_ptr(const sh_ptr<T>& open) : sh_ptr<T>(open), DP(), m_obj(NULL) {
    API_ASSERT(DP::getDescriptor().inTransaction());
    this->open();
  }

  /**
   * This assignment operation is semantically an "open for read" operation,
   * and is the normal way that one interacts with shared data.
   */
  rd_ptr<T>& operator=(const sh_ptr<T>& open) {
    m_sh = open.m_sh;
    this->open();
    return *this;
  }

  /**
   * The is what makes an <code>rd_ptr</code> a smart pointer. We can do
   * dynamic inter-object alias detection in here too. <code>const</code>
   * access is what makes this class "read only".
   */
  const T* operator->() const {
    if (DP::getDescriptor().isAliased(m_sh, m_obj))
      open();

    return m_obj;
  }

  /**
   * This isn't really all that safe, but it's necessary for when someone needs
   * to call an operator on the <code>T</code> itself. <strong>Do not cache the
   * returned reference and try to use it directly.</strong>
   */
  const T& operator*() const {
    if (DP::getDescriptor().isAliased(m_sh, m_obj))
      open();

    return *m_obj;
  }

  /**
   * Used for early release, which removes the pointed to object from the
   * transaction's read set. This requires careful use, and seems incompatible
   * with certain types of privatization.
   */
  inline friend void tx_release(rd_ptr<T>& to_release) {
    to_release.getDescriptor().release(to_release.m_sh);
  }

  /**
   * "Deletes" a pointed to object. This is actually a transactional delete, so
   * the delete only really happens if the transaction commits.
   */
  inline friend void tx_delete(rd_ptr<T>& on_commit) {
    on_commit.getDescriptor().deleteTransactionally(on_commit.m_sh);
  }
}; // template class stm::rd_ptr


template <class T>
void tx_delete(sh_ptr<T>& shared) {
  // forward to tx_delete(rd_ptr<T>&) --- the rd_ptr constructor will fail
  // if you call this outside of a transaction.
  rd_ptr<T> rd(shared);
  tx_delete(rd);
}


template <class T>
class wr_ptr : public rd_ptr<T> {
  using sh_ptr<T>::m_sh;
  using rd_ptr<T>::m_obj;

  void open() const {
    m_obj = static_cast<T*>(rd_ptr<T>::getDescriptor().openReadWrite(m_sh, sizeof(T)));
  }

  void upgrade() const {
    m_obj = static_cast<T*>(rd_ptr<T>::getDescriptor().upgradeToReadWrite(m_sh,sizeof(T)));
  }

 public:
  typedef rd_ptr<T> rd;

  /**
   * A wr_ptr doesn't have any different data than a rd_ptr, so the default
   * constructor is fine. It already asserts inTransaction, so this doesn't
   * have to do anything.
   */
  wr_ptr() : rd_ptr<T>() {
  }

  /**
   * This is the "open shared pointer for writing" constructor. The names are
   * slightly annoying because they all require prepending, but it's
   * straightforward.
   */
  explicit wr_ptr(const sh_ptr<T>& open) : rd_ptr<T>() {
    m_sh = open.m_sh;
    this->open();
  }

  /**
   * This is the "upgrade readable pointer for writing" constructor. It just
   * copies the data from the readable pointer using the rd_ptr copy
   * constructor, and then replaces the cached Object* with an upgraded
   * version.
   *
   * As a safety feature, it "fixes up" the readable pointer that it was
   * initialized with to point to the upgraded version of the Obeject*.
   */
  explicit wr_ptr(const rd_ptr<T>& upgrade)
    : rd_ptr<T>(upgrade)  // copy constructor (inits m_sh, m_tx, m_obj)
  {
    this->upgrade();
    upgrade.m_obj = m_obj;        // Fix-up the aliasing problem
  }


  /**
   * Open the handle for writing. Replace my current shared handle, and replace
   * my cached object.
   */
  wr_ptr<T>& operator=(const sh_ptr<T>& open) {
    m_sh  = open.m_sh;
    this->open();
    return *this;
  }

  /** Explicit assignment from a readable pointer is considered an upgrade. */
  wr_ptr<T>& operator=(const rd_ptr<T>& upgrade) {
    m_sh  = upgrade.m_sh;
    this->upgrade();

    // Fix-up aliasing problem
    upgrade.m_obj = m_obj;

    return *this;
  }

  /**
   * The is what makes an <code>wr_ptr</code> a smart pointer. We don't have to
   * do anything special here because we have a private writable copy we're
   * updating.
   */
  T* operator->() const {
    return m_obj;
  }

  /**
   * This isn't really all that safe, but it's necessary for when someone needs
   * to call an operator on the <code>T</code> itself. <strong>Do not cache the
   * returned reference and try to use it directly.</strong>
   */
  T& operator*() const {
    return *m_obj;
  }
}; // template class stm::wr_ptr


template <class T>
class un_ptr : public sh_ptr<T> {
  using sh_ptr<T>::m_sh;

 protected:
  T* m_obj;

 public:
  typedef un_ptr<T> rd;

  /**
   * Initializes a null un_ptr. Can only be initialized outside of a
   * transaction, from a "privatized" shared pointer.
   */
  un_ptr() : sh_ptr<T>(), m_obj(NULL) {
    API_ASSERT_UNPTR(rstm::not_in_transaction());
  }

  /**
   * Initializes a an un_ptr. Can only be initialized outside of a transaction,
   * from a "privatized" shared pointer. In actuality, there isn't any reason
   * that this couldn't happen inside a transaction, but we want to limit
   * potential programmer errors.
   */
  explicit un_ptr(const sh_ptr<T>& privatized)
    : sh_ptr<T>(privatized),
      m_obj(static_cast<T*>(rstm::open_privatized(m_sh))) {
    API_ASSERT_UNPTR(rstm::not_in_transaction());
  }


  /**
   * Nominally shared pointers that have been privatized can be opened by
   * assignment into an existing un_ptr. This should only happen outside of a
   * transaction. In actuality, there isn't any reason that this couldn't
   * happen inside a transaction, but we want to limit potential programmer
   * errors.
   */
  un_ptr<T>& operator=(const sh_ptr<T>& privatized) {
    API_ASSERT_UNPTR(rstm::not_in_transaction());

    m_sh  = privatized.m_sh;
    m_obj = static_cast<T*>(rstm::open_privatized(m_sh));

    return *this;
  }

  /**
   * Writable access to the <code>un_ptr</code>. The library might want to do
   * something special per access, so we call the event handler.
   */
  T* operator->() {
    rstm::on_private_use(m_sh, m_obj);
    return m_obj;
  }

  /**
   * Readable access to the <code>un_ptr</code>. The library might want to do
   * something special per access, so we call the even handler.
   */
  const T* operator->() const {
    rstm::on_private_use(m_sh, m_obj);
    return m_obj;
  }

  /**
   * Needed if you want to call a custom operator of the underlying pointed-to
   * object. The library might want to do something special per access, so we
   * call the event handler.
   */
  T& operator*() {
    rstm::on_private_use(m_sh, m_obj);
    return *m_obj;
  }

  /**
   * Needed if you want to call a custom operator of the underlying pointed-to
   * object. The library might want to do something special per access, so we
   * call the event handler.
   */
  const T& operator*() const {
    rstm::on_private_use(m_sh, m_obj);
    return *m_obj;
  }

  /*** Deletes an un_ptr */
  inline friend void tx_delete(un_ptr<T>& delete_now) {
    API_ASSERT_UNPTR(rstm::not_in_transaction());
    rstm::delete_privatized(delete_now.m_sh);
  }
}; // template class stm::un_ptr


//=============================================================================
// Define the rest of the client's public stm:: API
//=============================================================================


//-----------------------------------------------------------------------------
// TM System Interface
//-----------------------------------------------------------------------------
inline void init(std::string a, std::string b, bool c) {
  rstm::thr_init(a, b, c);
}
inline void shutdown(unsigned long i) { rstm::thr_shutdown(i); }
using rstm::not_in_transaction;


//-----------------------------------------------------------------------------
// Memory Management Interface
//-----------------------------------------------------------------------------
inline void* tx_alloc(size_t s) { return rstm::tx_alloc(s); }
inline void  tx_free(void* p)   { rstm::tx_free(p); }


//-----------------------------------------------------------------------------
// Privatization Interface
//-----------------------------------------------------------------------------
inline void fence()         { rstm::fence(); }
inline void acquire_fence() { rstm::acquire_fence(); }
inline void release_fence() { rstm::release_fence(); }


//-----------------------------------------------------------------------------
// Retry Interface
//-----------------------------------------------------------------------------
inline void  retry()                 { rstm::retry(); }


//-----------------------------------------------------------------------------
// Inevitability Interface
//-----------------------------------------------------------------------------
inline bool try_inevitable() { return rstm::try_inevitable(); }
inline void inev_read_prefetch(const void* addr, unsigned bytes) { }
inline void inev_write_prefetch(const void* addr, unsigned bytes) { }

} // namespace stm

#endif // RSTM_API_H
