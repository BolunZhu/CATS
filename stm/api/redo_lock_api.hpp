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

#ifndef REDO_LOCK_API_H
#define REDO_LOCK_API_H

#include "../support/DescriptorPolicy.hpp"

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
        redo_lock::Descriptor& tx = *redo_lock::currentDescriptor;      \
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
#define BEGIN_TRANSACTION                                               \
    {                                                                   \
        redo_lock::Descriptor& tx = *redo_lock::currentDescriptor;      \
        while (true) {                                                  \
            try {                                                       \
                tx.begin_transaction();                                 \
                {

#define BEGIN_READONLY_TRANSACTION BEGIN_TRANSACTION

#define END_TRANSACTION                                                 \
                }                                                       \
                tx.commit();                                            \
                break;                                                  \
            } catch (stm::RollBack) {                                   \
            } catch (...) {                                             \
                tx.onError();                                           \
                throw;                                                  \
            }                                                           \
            tx.unwind();                                                \
        }                                                               \
    }

#else
#error "Invalid STM_ROLLBACK option, please reconfigure and rebuild"
#endif // STM_ROLLBACK

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
 /* reads through read pointers require post-validation */              \
 template <class RSTM_UNIQUE>                                           \
 t RSTM_CONCAT(get_, n)(const stm::rd_ptr<RSTM_UNIQUE>& rdp) const {    \
   t ret = RSTM_CONCAT(m_, n);                                          \
   rdp.m_v.validate(rdp.getDescriptor(), *this);                        \
   return ret;                                                          \
 }                                                                      \
                                                                        \
 /* reads through write pointers do not require post-validation */      \
 template <class RSTM_UNIQUE>                                           \
 t RSTM_CONCAT(get_, n)(const stm::wr_ptr<RSTM_UNIQUE>&) const {        \
   return RSTM_CONCAT(m_, n);                                           \
 }                                                                      \
                                                                        \
 /* reads through un pointers do not require post-validation */         \
 template <class RSTM_UNIQUE>                                           \
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
 /* reads through read pointers require post-validation */              \
 template <class RSTM_UNIQUE>                                           \
 t RSTM_CONCAT(get_, n)(int i, const stm::rd_ptr<RSTM_UNIQUE>& rdp) const { \
   t ret = RSTM_CONCAT(m_, n)[i];                                       \
   rdp.m_v.validate(rdp.getDescriptor(), *this);                        \
   return ret;                                                          \
 }                                                                      \
                                                                        \
 /* reads through write pointers do not require post-validation */      \
 template <class RSTM_UNIQUE>                                           \
 t RSTM_CONCAT(get_, n)(int i, const stm::wr_ptr<RSTM_UNIQUE>&) const { \
   return RSTM_CONCAT(m_, n)[i];                                        \
 }                                                                      \
                                                                        \
 /* reads through un pointers do not require post-validation */         \
 template <class RSTM_UNIQUE>                                           \
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
 /* reads through read pointers require post-validation */              \
 template <class RSTM_UNIQUE>                                           \
 t RSTM_CONCAT(get_, n)(int x, int y,                                   \
                        const stm::rd_ptr<RSTM_UNIQUE>& rdp) const {    \
   t ret = RSTM_CONCAT(m_, n)[x][y];                                    \
   rdp.m_v.validate(rdp.getDescriptor(), *this);                        \
   return ret;                                                          \
 }                                                                      \
                                                                        \
 /* reads through write pointers do not require post-validation */      \
 template <class RSTM_UNIQUE>                                           \
 t RSTM_CONCAT(get_, n)(int x, int y, const stm::wr_ptr<RSTM_UNIQUE>&) const { \
   return RSTM_CONCAT(m_, n)[x][y];                                     \
 }                                                                      \
                                                                        \
 /* reads through un pointers do not require post-validation */         \
 template <class RSTM_UNIQUE>                                           \
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
using redo_lock::Object;

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
  /**
   * These three classes are subtypes of sh_ptr. We need friend access for
   * conversion purposes.
   */
  friend class rd_ptr<T>;
  friend class wr_ptr<T>;
  friend class un_ptr<T>;

  /**
   * Used internally to allow <code>if (<smart_pointer>)</code> tests while
   * preventing <code>delete &lt;smart_pointer&gt;</code> from compiling.
   *
   * NB: This may be overly complicated. It's probably fine just to make
   *     <code>operator delete</code> private for <code>sh_ptr</code>.
   */
  class try_tx_delete_instead {
    void operator delete(void*);
  };

 protected:
  /**
   * Redo_lock is a 0-indirection, redo-log system. Because this we can store a
   * pointer to the actual object directly. This isn't going to change
   * locations on us like it would in an RSTM-style system.
   *
   * Mutable because the current library implementation was written without
   * regards to constness.
   *
   * @todo [ld] fix library implementation so that this can be non-mutable.
   */
  mutable T* m_obj;

 public:

  /*** The default constructor initializes a <code>NULL sh_ptr</code>. */
  sh_ptr() : m_obj(NULL) {
  }

  /**
   * @param obj
   *
   * Explicit constructor for a sh_ptr. This constructor is how you make an
   * <code>stm::Object</code> shared. The passed pointer is no longer safe to
   * use after you have called this constructor.
   *
   * <strong>Using the pointer parameter after the call to this constructor is
   * a semantic error and can result in very-difficult-to-debug runtime
   * problems.</strong>
   *
   * Initializing multiple <code>sh_ptr</code>s from the same <code>T*</code>
   * <strong>is</strong> supported.
   *
   */
  explicit sh_ptr(T* const obj)
    : m_obj(static_cast<T*>(redo_lock::Descriptor::get_inplace_version(obj))) {
    // The library implementation only knows about redo_lock::Object(s), so
    // we're required to do this cast. It will succeed because T has to inherit
    // from Object.
  }

  /**
   * This allows sh_ptrs to act sort of polymorphically based on the pointed to
   * type. Essentially, one can use an sh_ptr<Subclass> anywhere an
   * sh_ptr<Superclass> is expected.
   */
  template <class Super> operator sh_ptr<Super>() {
    // No fancy checks here. The sh_ptr<Super> constructor will only pass type
    // checking if T inherits from Super.
    return sh_ptr<Super>(m_obj);
  }

  /**
   * Allows ordering so that smart pointers can be stored in sorted containers.
   */
  bool operator<(const sh_ptr<T>& rhs) const {
    return m_obj < rhs.m_obj;
  }

  /*** Allows tests of the form <code>if (!&lt;smart pointer&gt;)</code>. */
  bool operator!() const {
    return !m_obj;
  }

  /**
   * Allows you to test to see if two smart pointers are equal. This is a
   * pointer comparison, so it won't call <code>operator==</code> on the
   * underlying objects. To do that you have to use the smart pointer's
   * <code>opeartor*</code> (<code>*lhs == *rhs</code>).
   */
  bool operator==(const sh_ptr<T>& rhs) const {
    return m_obj == rhs.m_obj;
  }

  /**
   * Allows you to test to see if two smart pointers are equal. This is a
   * pointer comparison, so it won't call <code>operator!=</code> on the
   * underlying objects. To do that you have to use the smart pointer's
   * <code>opeartor*</code> (<code>*lhs != *rhs</code>).
   */
  bool operator!=(const sh_ptr<T>& rhs) const {
    return m_obj != rhs.m_obj;
  }


  // some compilers break if you treat NULL as a void* instead of int.
#if defined(_MSC_VER)
  /**
   * Allows tests of the form <code>if (&lt;smart pointer&gt; == NULL)</code>.
   */
  bool operator==(int rhs) const {
    API_ASSERT(NULL == rhs);
    return m_obj == (void*)rhs;
  }

  /**
   * Allows tests of the form <code>if (&lt;smart pointer&gt; != NULL)</code>.
   */
  bool operator!=(int rhs) const {
    API_ASSERT(NULL == rhs);
    return m_obj != (void*)rhs;
  }

  /**
   * Allows tests of the form <code>if (NULL == &lt;smart
   * pointer&gt;)</code>. Just forwards to <code>(sh_ptr ==
   * NULL)</code>. Technically this allows tests against any naked
   * <code>T*</code> pointers, though we don't expect that that will happen.
   */
  inline friend bool operator==(int lhs, const sh_ptr<T>& rhs) {
    API_ASSERT(lhs == 0);
    return rhs == lhs;
  }

  /**
   * Allows tests of the form <code>if (NULL != &lt;smart
   * pointer&gt;)</code>. Just forwards to <code>(sh_ptr !=
   * NULL)</code>. Technically this also allows tests against naked
   * <code>T*</code> pointers, though we don't expect any use for that.
   */
  inline friend bool operator!=(int lhs, const sh_ptr<T>& rhs) {
    API_ASSERT(lhs == 0);
    return rhs != lhs;
  }
#else
  /**
   * Allows tests of the form <code>if (&lt;smart pointer&gt; ==
   * NULL)</code>. Also permits a test against any pointer type (or type that
   * can be implicitely converted to a <code>void*</code>.
   */
  bool operator==(const T* rhs) const {
    return m_obj == rhs;
  }

  /**
   * Allows tests of the form <code>if (&lt;smart pointer&gt; !=
   * NULL)</code>. Also permits a test against any pointer type (or type that
   * can be implicitely converted to a <code>void*</code>.
   */
  bool operator!=(const T* rhs) const {
    return m_obj != rhs;
  }

  /**
   * Allows tests of the form <code>if (NULL == &lt;smart
   * pointer&gt;)</code>. Just forwards to <code>(sh_ptr ==
   * NULL)</code>. Technically this allows tests against any naked
   * <code>T*</code> pointers, though we don't expect that that will happen.
   */
  inline friend bool operator==(const T* lhs, const sh_ptr<T>& rhs) {
    return rhs == lhs;
  }

  /**
   * Allows tests of the form <code>if (NULL != &lt;smart
   * pointer&gt;)</code>. Just forwards to <code>(sh_ptr !=
   * NULL)</code>. Technically this also allows tests against naked
   * <code>T*</code> pointers, though we don't expect any use for that.
   */
  inline friend bool operator!=(const T* lhs, const sh_ptr<T>& rhs) {
    return rhs != lhs;
  }
#endif

  /**
   * Allows tests of the form <code>if (&lt;smart pointer&gt;)</code>. The
   * <code>try_tx_delete_instead</code> is a helper class that we provide an
   * implicit conversion to. The <code>try_tx_delete</code> class hass a
   * private destructor, so a call of <code>delete &lt;smart pointer&gt;</code>
   * (which matches this conversion too) will fail at compile time.
   *
   * This comes straight out of Andrei Alexandrescu's book "Modern C++ Design:
   * Generic Programming and Design Patterns Applied." The chapter on smart
   * pointers was released publically online.
   */
  operator try_tx_delete_instead*() const {
    static try_tx_delete_instead test;
    return  (m_obj == NULL) ? NULL : &test;
  }

 private:
  /**
   * Delete a shared object. We can't safely delete shared objects inside a
   * transaction directly, so we open it for reading first.
   *
   * This can only be safely called within a transactional context. If you want
   * to delete a shared object outside a transaction, use an un_ptr manually.
   *
   * Implemented later because it depends on the <code>rd_ptr<code> code.
   */
  friend void tx_delete<>(sh_ptr<T>& shared);

}; // template class stm::sh_ptr


/**
 * A <code>rd_ptr</code> represents a pointer to a const shared object. You can
 * initialize an <code>rd_ptr</code> from an <code>sh_ptr</code>, or a
 * <code>wr_ptr</code>. You can also assign an <code>sh_ptr</code> or
 * <code>wr_ptr</code> to an <code>rd_ptr</code> any time you want.
 *
 * Initializing or assigning from an sh_ptr is semantically an "open for
 * reading".
 */
template <class T>
class rd_ptr : public sh_ptr<T>,
               public stm::DescriptorPolicy<redo_lock::Descriptor> {
  // a wr_ptr needs friend access for initialization purposes.
  friend class wr_ptr<T>;
  using sh_ptr<T>::m_obj;
  typedef stm::DescriptorPolicy<redo_lock::Descriptor> DP;

 public:
  /**
   * The validator for post-validation. Has to be mutable because alias
   * detection might change the value of the validator and the active pointer,
   * even in nominally <code>const</code> contexts.
   */
  mutable redo_lock::Validator m_v;

 protected:
  /**
   * The active pointer to the shared object. Logically const for the
   * <code>rd_ptr</code>, but used in the <code>wr_ptr</code> too. Mutable
   * because automatic alias detection can actually change the pointer.
   */
  mutable T* m_active;

 private:
  /**
   * This utility method just wraps the call to the descriptor to open the
   * current object. This operation happens in a couple of places, including
   * initialization during construction, so we return the T* rather than
   * setting m_active inside here.
   *
   * open() is entirely non-const, however all of the stuff that it modifies is
   * mutable.
   */
  void open() const {
    m_active = static_cast<T*>(DP::getDescriptor().openReadOnly(m_obj, m_v,sizeof(T)));
  }

 public:
  /**
   * The default constructor initializes a <code>NULL rd_ptr</code>. This
   * pointer can then be used as the target of an assignment from an
   * <code>sh_ptr</code> to interact with a shared object in read-only fashion.
   *
   * This requires a thread local lookup, so a best practice is to avoid using
   * this constructor if your thread local costs are high. I particular, it is
   * more efficient to declare an <code>rd_ptr</code> once, outside a loop,
   * than to continuously call this constructor inside a loop.
   */
  rd_ptr() : sh_ptr<T>(), DP(), m_v(), m_active(NULL) {
    API_ASSERT(DP::getDescriptor().inTransaction());
  }


  /**
   * An <code>rd_ptr</code> can be explicitely initialized directly from an
   * <code>sh_ptr</code> which implies "open for reading" semantics. This is
   * the most efficient way to open a shared object for reading, if there is no
   * available <code>rd_ptr</code> available for the assignment version
   * (<code>operator=(sh_ptr&lt;T&gt;)</code>).
   */
  explicit rd_ptr(const sh_ptr<T>& to_open)
    : sh_ptr<T>(to_open), DP(), m_v(), m_active(NULL) {
    API_ASSERT(DP::getDescriptor().inTransaction());
    open();
  }

  /**
   * This assignment is semantically an "open for reading" operation, and is
   * the normal way that one opens a <code>sh_ptr</code>. Note that this is
   * more efficient than constantly re-initializing an <code>rd_ptr</code>
   * inside of a loop because it doesn't need a thread-local lookup.
   */
  rd_ptr<T>& operator=(const sh_ptr<T>& to_open) {
    m_obj = to_open.m_obj;
    open();
    return *this;
  }

  /**
   * This is what makes an <code>rd_ptr</code> a smart pointer. We can do
   * dynamic inter-object alias detection in here too.  <code>const T*</code>
   * return type is what makes this class a read-only pointer.
   *
   * @todo Make the alias check compile and/or runtime configurable.
   */
  const T* operator->() const {
    if (DP::getDescriptor().isAliased(m_obj, m_active))
      open();
    return m_active;
  }

  /**
   * This isn't really all that safe, but it's necessary for when someone needs
   * to call an operator on the <code>T</code> itself. <strong>Do not cache the
   * returned reference and try to use it directly.</strong>
   */
  const T& operator*() const {
    if (DP::getDescriptor().isAliased(m_obj, m_active))
      open();
    return *m_active;
  }

  /**
   * Used for early release, which removes the pointed-to object from the
   * transaction's read-set. This requires careful use, and seems incompatible
   * with certain types of privatization and word-based systems.
   */
  inline friend void tx_release(rd_ptr<T>& to_release) {
    to_release.getDescriptor().release(to_release.m_obj);
  }

  /**
   * Called to delete a shared object inside of a transaction. Only actually
   * happens if the transaction commits. There is probably another level of
   * delay as well, as even if this transaction commits we can't delete the
   * object until no threads are looking at it anymore. This is all transparent
   * to the client.
   */
  inline friend void tx_delete(rd_ptr<T>& on_commit) {
    on_commit.getDescriptor().deleteTransactionally(on_commit.m_obj);
  }
}; // template class stm::rd_ptr


/**
 * A <code>wr_ptr</code> represents a pointer to a shared object. You can
 * initialize a <code>wr_ptr</code> from an <code>sh_ptr</code> to open it for
 * write access, or from an <code>rd_ptr</code> to upgrade to writable access.
 *
 * It doesn't need any more fields than the <code>rd_ptr</code> that it
 * inherits from, it just provides non-<code>const</code> access to the
 * <code>m_active</code> object.
 */
template <class T>
class wr_ptr : public rd_ptr<T> {
  using sh_ptr<T>::m_obj;
  using rd_ptr<T>::m_v;
  using rd_ptr<T>::m_active;

  /**
   * Just a utility to open the <code>m_obj</code> for writing. Called in a
   * couple of places so this just centralizes the code.
   */
  void open() {
    m_active = static_cast<T*>(rd_ptr<T>::getDescriptor().openReadWrite(m_obj,sizeof(T)));
    // We don't need to validate writable objects since they are thread-local.
    m_v.config(0);
  }

 public:
  /*** Used by some template code for generic and meta-programming purposes. */
  typedef rd_ptr<T> rd;

  /**
   * A <code>wr_ptr</code> doesn't have any different data than a
   * <code>rd_ptr</code>, si the default constructor is fine. It already
   * asserts inTransaction if the correct debugging level is enabled, so we're
   * ok.
   */
  wr_ptr() : rd_ptr<T>() {
  }

  /**
   * This constructor opens the passed <code>sh_ptr</code> for writing. We use
   * the default <code>rd_ptr</code> constructor because we don't actually want
   * to open the object for reading first.
   */
  explicit wr_ptr(const sh_ptr<T>& to_open) : rd_ptr<T>() {
    m_obj = to_open.m_obj;
    open();
  }

  /**
   * This is the "upgrade to writable from readable" constructor. As a safety
   * feature, it "fixes up" the readable pointer that it was initialized with
   * so that future accesses through the <code>rd_ptr</code> won't have alias
   * issues.
   */
  explicit wr_ptr(rd_ptr<T>& upgrade) :
    rd_ptr<T>(upgrade) // copy constructor (inits m_obj, m_v, m_active)
  {
    open();

    // reset the readable pointer
    upgrade.m_active = m_active;
    upgrade.m_v = m_v;
  }

  /**
   * "Open for writing". This assignment is more efficient than calling the
   * equivalent constructor because it doesn't require a thread local lookup.
   */
  wr_ptr<T>& operator=(const sh_ptr<T>& to_open) {
    m_obj = to_open.m_obj;
    open();
    return *this;
  }

  /**
   * An explicit assignment from a readable pointer is considered an
   * upgrade. As with the equivalent explicit constructor, we'll "fix-up" the
   * readable pointer so that the next time we use it we won't have alias
   * errors.
   */
  wr_ptr<T>& operator=(rd_ptr<T>& upgrade) {
    m_obj = upgrade.m_obj;
    open();
    upgrade.m_active = m_active;  // fix-up
    upgrade.m_v = m_v;
    return *this;
  }

  /**
   * The heart of what makes this a smart pointer. Overrides the
   * <code>rd_ptr</code> implementation to allow non-<code>const</code> access
   * to the <code>m_active</code> pointer.
   *
   * In the redo_lock framework a write-pointer can never be aliased, as all
   * write pointers to a particular object are guaranteed by the library
   * implementation to point to the same underlying object.
   */
  T* operator->() const {
    return m_active;
  }

  /**
   * This is necessary if you want to call custom or built-in operators on the
   * actual, underlying <code>T</code> type. <strong>Do not cache the returned
   * reference and try to use it directly</strong>, it may work occasionally,
   * but you're in for difficult-to-debug errors if you do it.
   */
  T& operator*() const {
    return *m_active;
  }
}; // template class stm::wr_ptr


/**
 * The <code>un_ptr</code> represents a pointer to a non-<code>const</code>,
 * shared object that has been privatized and safely used outside of a
 * transaction. This is an advanced feature and should not be used lightly.
 *
 * More information on privatization in the RSTM/redo_lock framework is
 * available in the web documentation and various publications of the RSTM
 * group.
 */
template <class T>
class un_ptr : public sh_ptr<T> {
  using sh_ptr<T>::m_obj;

  /**
   * Utility used to open the <code>m_obj</code> for private access. Done in a
   * couple of places so this centralizes the operation.
   */
  void open() {
    redo_lock::Descriptor::open_privatized(m_obj, sizeof(T));
  }

 public:
  /**
   * Used by some template code for generic and meta-programming purposes.
   */
  typedef un_ptr<T> rd;

  /**
   * Initializes a <code>NULL un_ptr</code>. Can only be initialized outside of
   * a transaction.
   */
  un_ptr() : sh_ptr<T>() {
    API_ASSERT_UNPTR(!redo_lock::currentDescriptor->inTransaction());
  }

  /**
   * Initializes an <code>un_ptr</code> from a privatized
   * <code>sh_ptr</code>. Typical usage is that the <code>sh_ptr</code> is
   * privatized inside of a transaction (unlinking a list, tree, etc.), and
   * then the <code>un_ptr</code> is initialized after the transaction.
   */
  un_ptr(const sh_ptr<T>& privatized) : sh_ptr<T>(privatized) {
    API_ASSERT_UNPTR(!redo_lock::currentDescriptor->inTransaction());
    open();
  }

  /*** "Open the shared object for private access". */
  un_ptr<T>& operator=(const sh_ptr<T>& privatized) {
    API_ASSERT_UNPTR(!redo_lock::currentDescriptor->inTransaction());
    m_obj = privatized.m_obj;
    open();
    return *this;
  }

  /**
   * The heart of what makes this a smart pointer, and the point of
   * privatization... we can return the in-place <code>m_obj</code> directly
   * for both read and write purposes, and we don't need to validate when using
   * it.
   */
  const T* operator->() const {
    API_ASSERT_UNPTR(!redo_lock::currentDescriptor->inTransaction());
    return m_obj;
  }

  /*** Returns a writable version of the pointed-to object. */
  T* operator->() {
    API_ASSERT_UNPTR(!redo_lock::currentDescriptor->inTransaction());
    return m_obj;
  }

  /**
   * Use this when you need to call a custom or built in operator that is
   * defined as part of the <code>T</code> class. <strong>Do not store the
   * returned reference and try to use it directly later on</strong>.
   */
  const T& operator*() const {
    API_ASSERT_UNPTR(!redo_lock::currentDescriptor->inTransaction());
    return *m_obj;
  }

  /**
   * Use this when you need to call a custom or built in operator that is
   * defined as part of the <code>T</code> class. <strong>Do not store the
   * returned reference and try to use it directly later on</strong>.
   */
  T& operator*() {
    API_ASSERT_UNPTR(!redo_lock::currentDescriptor->inTransaction());
    return *m_obj;
  }

  inline friend void tx_delete(un_ptr<T>& delete_now) {
    API_ASSERT_UNPTR(!redo_lock::currentDescriptor->inTransaction());
    stm::delete_privatized(delete_now.m_obj);
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


//-----------------------------------------------------------------------------
// TM System Interface
//-----------------------------------------------------------------------------
inline bool not_in_transaction() {
  return !redo_lock::currentDescriptor->inTransaction();
}
} // namespace stm

#endif // REDO_LOCK_API_H
