///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2008, 2009
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

// In order to be as close to cross-platform as possible, we're using a c++
// program to configure the rstm libraries.  If you don't have a compliant c++
// compiler, then you can't build rstm anyway, but this way you don't need to
// have perl, bash, java, or some other tool just to create a config.h on win32

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cassert>
#include <cstdarg>
#include <cstdlib>

// Simple mechanism for during a compile time -D argument into a string. We
// evaluate the argument once to get the value, and then use the # preprocessor
// operator to turn that value into a string literal. This allows users to
// configure with different versions of gcc at compile time.
#define TO_STRING_LITERAL(arg) #arg
#define MAKE_STR(arg) TO_STRING_LITERAL(arg)

using std::string;
using std::cout;
using std::endl;
using std::cin;
using std::vector;
using std::ofstream;
using std::stringstream;

// enums to make it easy to track our platform / build environment
enum os_t  { LINUX, SOLARIS, MAC, WINDOWS, OPENBSD, FREEBSD, AIX };
enum cpu_t { X86, SPARC, IA64, POWER };
enum cc_t  { GCC, MSC, LLVM };

// globals

// for determining platform
os_t   OS;
cpu_t  CPU;
cc_t   CC;
string LIB = "";

// for the makefile
string AR          = "AR          = ";
string BUILD_CXX   = "CXX         = ";
string FINAL_CXX   = "FINAL_CXX   = ";
string LDFLAGS     = "LDFLAGS     = ";
string CXXFLAGS    = "CXXFLAGS    = -Wall -D_REENTRANT ";
string STM_VERSION = "STM_VERSION = ";
string MFLAGS      = "MFLAGS      = -j6";

// for the config.h
vector<string> CONFIGH;

// this class is going to encapsulate an option set.  This is terrible
// engineering, as we don't have a good strategy for construction, but
// hopefully this will evolve into something useful and good
class Iterator;
class OptionSet
{
  friend Iterator begin(const OptionSet&);
  friend Iterator end(const OptionSet&);

 protected:
  /*** the text to display for interactive configuration */
  string prompt;

  /*** text (token) for each option */
  vector<string> options;

  /*** text describing each option */
  vector<string> descriptions;

  /*** the user choice */
  unsigned int choice;

  /*** default value */
  unsigned int default_value;

  /*** actually output something to the config.h and/or Makefile.inc */
  virtual void configure() = 0;

  // utility to prompt the user for input and return an int between 1 and
  // maxchoice.  Note that hitting return when prompted will return _default,
  // and that non-int characters won't break things
  int ask(int _default, int maxchoice)
  {
    string tmp;
    while (true) {
      cout << "Choice (1 -- " << options.size() << ") ";
      cout << "[default = " << _default << "] :> ";
      getline(cin, tmp);
      // newline means use default value
      if (tmp == "") {
        cout << endl;
        return _default;
      }
      int i = -1;
      i = strtol(tmp.c_str(), NULL, 10); // returns zero on error
      if ((i > 0) && (i <= maxchoice)) {
        cout << endl;
        return i;
      }
    }
  }

 public:
  /*** setters for simple fields */
  void setdefault(int d)   { default_value = d; }
  void setprompt(string s) { prompt        = s; }
  virtual void setchoice(unsigned int i) {
    choice = i - 1;
    assert(choice < options.size() && "Choice out of option range.");
    configure();
  }

  /*** getter for the string representing the choice */
  string getchoicestring()   {
    assert(choice < options.size() && "Choice out of option range");
    return options[choice];
  }

  /*** configuration: add an option */
  void addoption(string option, string description)
  {
    options.push_back(option);
    descriptions.push_back(description);
  }

  /*** print choices, ask user which one to use */
  virtual void interact()
  {
    cout << prompt << endl;
    int ctr = 1;
    vector<string>::iterator i;
    for (i = descriptions.begin(); i != descriptions.end(); ++i) {
      cout << "  " << (ctr < 10 ? " " : "") << "[";
      cout << ctr++ << "] " << *i << endl;
    }
    choice = ask(default_value, options.size()) - 1;
    configure();
  }

  /*** nop for now */
  virtual ~OptionSet() { }
};

class ProfileOptionSet : public OptionSet
{
  vector<string> cxxopts;
  vector<string> ldopts;

 public:
  void addoption(string cxx, string ld, string desc)
  {
    cxxopts.push_back(cxx);
    ldopts.push_back(ld);
    options.push_back("");
    descriptions.push_back(desc);
  }

  virtual void configure()
  {
    CXXFLAGS += cxxopts[choice];
    LDFLAGS += ldopts[choice];
  }
};


// takes a string (passed by reference) and uppercases every character in
// it by calling toupper()
void str_ucase_inplace(string& t)
{
  for (unsigned i = 0; i < t.length(); ++i)
    t[i] = toupper(t[i]);
}

// subclass of OptionSet that handles the selection of an STM library
//
// NB: always pick the library first, because other options depend on it
class LibOptionSet : public OptionSet
{
 protected:

  virtual void configure()
  {
    string choice = getchoicestring();
    STM_VERSION += choice;
    LIB = choice;
    str_ucase_inplace(LIB);
    CONFIGH.push_back("#define STM_LIB_" + LIB);
  }
};

// most of the time, we ask the user to pick between a few options, and the
// choice maps directly to a single #define that gets added to the config.h
class SimpleOptionSet : public OptionSet
{
 protected:
  virtual void configure()
  {
    string choice = getchoicestring();
    CONFIGH.push_back("#define " + choice);
  }
};

class CMOptionSet : public OptionSet
{
 protected:
  virtual void configure()
  {
    string choice = getchoicestring();
    CONFIGH.push_back("#define STM_DEFAULT_CM " + choice);
    CONFIGH.push_back("#include \"stm/cm/" + choice + ".hpp\"");
  }
};

class OverrideableOptionSet : public SimpleOptionSet
{
  friend Iterator begin(const OverrideableOptionSet&);

  int override;
 public:
  OverrideableOptionSet() : SimpleOptionSet(), override(-1) { }

  virtual void interact()
  {
    if (override == -1) {
      OptionSet::interact();
    }
    else if (override != 0) {
      choice = override - 1;
      configure();
    }
  }

  void setoverride(int i) { override = i; }

  virtual void setchoice(unsigned int i) {
    choice = (override == -1) ? i - 1 : override - 1;
    assert(choice < options.size() && "Choice out of option range");
    configure();
  }

  void OS_trigger(int os, unsigned int trigger_choice, bool& flag)
  {
    if (OS == os) {
      if ((choice + 1) == trigger_choice)
        flag = true;
    }
  }

  void LDFLAG_trigger(unsigned int trigger_choice, string txt)
  {
    if ((choice + 1) == trigger_choice) {
      LDFLAGS += txt;
    }
  }
};

// the library uses the LibOptionSet type
LibOptionSet libs;

// most of the choices are SimpleOptionSet type
SimpleOptionSet valheur, priv, lwpriv, inev, lwinev, locks, retrys,
  cache_descriptor, extendedrollback, lwpub, ssspriv, owlpriv, faircm,
  writeset, api_asserts;

CMOptionSet cm;

OverrideableOptionSet tlocals, allocators, rollback;

ProfileOptionSet profiles_debug, profiles_nodebug;

// the gcc we use at UR doesn't emit correct code on sparc/solaris when -ggdb
// is given and __thread is used.  Set this global if __thread is selected
bool warn_solaris_tlocal = false;

// Use compiler defines to figure out the cpu, compiler, and os; update globals
// accordingly
//
// The following compiler built-in defines should be sufficient to determine
// our entire platform:
//   Compiler: __GNUC__ or _MSC_VER
//   OS:       __linux__, __APPLE__, _MSC_VER, __OpenBSD, __FreeBSD__, __sun
//   CPU:      __i386__, __x86_64__, __sparc__, _MSC_VER
void platform()
{
  // figure out the OS
#if defined(_MSC_VER)
  OS = WINDOWS;
#elif defined(__linux__)
  OS = LINUX;
#elif defined(__APPLE__)
  OS = MAC;
#elif defined(__OpenBSD__)
  OS = OPENBSD;
#elif defined(__FreeBSD__)
  OS = FREEBSD;
#elif defined(_AIX)
  OS = AIX;
#elif defined(__sun)
  OS = SOLARIS;
  LDFLAGS += "-lrt ";
#else
#error "Unknown Operating System"
#endif

  // figure out the compiler
#if defined(_MSC_VER)
  CC = MSC;
  BUILD_CXX += "msc";
  AR += "ar";
  FINAL_CXX += "msc++";
#elif defined(__llvm__)
  CC = LLVM;
  BUILD_CXX += "llvm-g++";
  AR += "llvm-ar";
  FINAL_CXX += "llvm-ld";
  LDFLAGS   += "-v -native -lstdc++ ";
#elif defined(__GNUC__)
  CC = GCC;
  // We assume that CXX is going to be some sort of gcc, and that we can't do
  // strict aliasing because of the way our read and write logs work.
  CXXFLAGS += "-fno-strict-aliasing ";
#if !defined(CXX)
  // If we're being compiled with gcc, and the user hasn't specified a
  // CXX to use, then use "g++" by default.
  BUILD_CXX += "g++";
  FINAL_CXX += "g++";
#else
  // Otherwise the user would like to use the CXX that they passed as
  // -DCXX=. The "user" may be the included makefile.
  BUILD_CXX += MAKE_STR(CXX);
  FINAL_CXX += MAKE_STR(CXX);
#endif
  AR += "ar";
#else
#error "Unknown Compiler"
#endif

  // figure out the CPU
#if defined(_MSC_VER)
  CPU = X86;
#elif defined(__sparc__)
  CPU = SPARC;
  CXXFLAGS += "-pthreads -mcpu=v9 ";
#elif defined(__i386__) || defined(__x86_64__)
  CPU = X86;
  LDFLAGS  += (LLVM == CC) ? "-lpthread " : "-lpthread -m32 ";
  CXXFLAGS += (LLVM == CC) ? "-emit-llvm " : "-msse2 -mfpmath=sse -m32 ";
#elif defined(__ia64__)
  CPU = IA64;
  LDFLAGS += "-lpthread";
#elif defined(_POWER)
  CPU = POWER;
  LDFLAGS += "-lpthread -Wl,-bmaxdata:0xD0000000/dsa ";
  CXXFLAGS += "-Wa,-many ";
#else
#error "Unknown CPU"
#endif
}

// configure all vectors of strings.  Note that some are needed even if we are
// in the "use defaults" mode
void init_strings()
{
  // config the library choice strings
  libs.setprompt("Which back-end STM library would you like to build?");
#if !defined(_POWER)
  libs.addoption("rstm",
                 "RSTM     - object-based, nonblocking, indirection");
  libs.addoption("redo_lock",
                 "RedoLock - object-based, blocking, no indirection, redo-log");
#endif
  libs.addoption("llt",
                 "LLT      - word-based, blocking, lazy lazy, uses timestamps like TL2");
  libs.addoption("flow",
                 "Flow     - framework for flow-serializable (ALA/SFS) semantics");
#if !defined(_POWER)
  libs.addoption("et",
                 "ET       - word-based, extendable timestamps (eager/lazy acquire/update)");
  libs.addoption("fair",
                 "Fair     - lazy runtime with starvation avoidance");
  libs.addoption("strict",
                 "Strict   - framework for strict serializability (SSS) semantics");
  libs.addoption("sgla",
                 "SGLA     - single global lock atomicity for weakly atomic orec stm");
#endif
  libs.addoption("tml",
                 "TML      - Transactional Mutex Lock runtime");
  libs.addoption("tml_lazy",
                 "TML+Lazy - TML with lazy acquire");
  libs.addoption("precise",
                 "Precise  - Livelock-free, lazy STM with value-based validation");
#if !defined(_POWER)
  libs.addoption("ringsw",
                 "RingSW   - Single-Writer variant of RingSTM");
#endif
  libs.addoption("cgl",
                 "CGL      - global lock, no concurrency, no overhead");
  libs.setdefault(1);

  // configure thread-local storage choices
  tlocals.setprompt("How would you like to interact with thread local storage?");
  tlocals.addoption("STM_TLS_GCC",     "gcc __thread -- nonstandard, but fast");
  tlocals.addoption("STM_TLS_PTHREAD", "Pthread API  -- standard, but often slow");
  tlocals.setdefault(1);
  if (OS == WINDOWS)
    tlocals.setoverride(0);
  else if ((OS == MAC) || (OS == FREEBSD) || (OS == OPENBSD) || (OS == AIX))
    tlocals.setoverride(2);

  // build profiles
  profiles_debug.setprompt("Which optimization/debugging profile would you like?");
  // debugging symbols are currently incompatible with our LLVM build
  if (CC != LLVM) {
    profiles_debug.addoption("-O3 -ggdb", "", "Normal   (-O3, debug symbols, asserts)");
    profiles_debug.addoption("-O3 -DNDEBUG", "", "Fastest  (-O3, NO debug symbols, NO asserts)");
    profiles_debug.addoption("-O3 -DNDEBUG -fno-schedule-insns -fno-schedule-insns2 -fno-tree-ch", "", "Fastest  (-O3, NO debug symbols, NO asserts), safe for AIX");
    profiles_debug.addoption("-O0 -ggdb", "", "Debug    (-O0, debug symbols, asserts)");
    profiles_debug.addoption("-O3 -pg -ggdb", "-pg", "Normal + gnuprof");
    profiles_debug.addoption("-O0 -pg -ggdb", "-pg", "Debug + gnuprof");
  }
  else {
    profiles_debug.addoption("-O3", "",          "Normal (-O3, NO debugging symbols, asserts)");
    profiles_debug.addoption("-O3 -DNDEBUG", "", "Fast   (-O3, NO debugging symbols, NO asserts)");
    profiles_debug.addoption("-O0", "",          "Slow   (-O0, NO debugging symbols, asserts)");
    profiles_debug.addoption("-O3 -pg", "-pg",   "Normal + gnuprof");
    profiles_debug.addoption("-O0 -pg", "-pg",   "Slow + gnuprof");
  }
  profiles_debug.setdefault(1);

  // note:  nodebug is only for SOLARIS with __thread turned on
  profiles_nodebug.setprompt("Which optimization/debugging profile would you like? (note: debugging symbols are off since you are using __thread in Solaris)");
  profiles_nodebug.addoption("-O3", "", "Normal   (-O3, asserts)");
  profiles_nodebug.addoption("-O3 -DNDEBUG", "", "Fastest  (-O3, NO asserts)");
  profiles_nodebug.addoption("-O0", "", "Debug    (-O0, asserts)");
  profiles_nodebug.addoption("-O3 -pg", "-pg", "Normal + gnuprof");
  profiles_nodebug.addoption("-O0 -pg", "-pg", "Debug + gnuprof");
  profiles_nodebug.setdefault(1);

  // allocators
  allocators.setprompt("Which allocator would you like? (note: any malloc-compliant allocator can be used by choosing Malloc and then setting an LD_PRELOAD)");
  allocators.addoption("STM_ALLOCATOR_GCHEAP_NOEPOCH", "GCHeap       - Nonblocking via lazy reclamation of deleted objects");
  allocators.addoption("STM_ALLOCATOR_GCHEAP_EPOCH",   "GCHeap+Epoch - GCHeap, but with added (unnecessary) epoch-based reclamation");
  allocators.addoption("STM_ALLOCATOR_MALLOC",         "Malloc       - As defined by your system");
  if (OS == SOLARIS)
    allocators.addoption("STM_ALLOCATOR_MTMALLOC", "MTMalloc");
  allocators.setdefault(1);

  // contention management (two vectors)
  cm.setprompt("Which contention manager would you like to use by default?");
  cm.addoption("Aggressive",   "Aggressive");
  cm.addoption("Eruption",     "Eruption");
  cm.addoption("Greedy",       "Greedy");
  cm.addoption("Highlander",   "Highlander");
  cm.addoption("Justice",      "Justice");
  cm.addoption("Karma",        "Karma");
  cm.addoption("Killblocked",  "Killblocked");
  cm.addoption("Polite",       "Polite");
  cm.addoption("Polka",        "Polka");
  cm.addoption("Polkaruption", "Polkaruption");
  cm.addoption("Polkavis",     "Polkavis");
  cm.addoption("Reincarnate",  "Reincarnate");
  cm.addoption("Serializer",   "Serializer");
  cm.addoption("Timestamp",    "Timestamp");
  cm.addoption("Timid",        "Timid");
  cm.addoption("Whpolka",      "Whpolka");
  cm.setdefault(9);

  // validation heuristics
  valheur.setprompt("Would you like to use a global commit counter to avoid some validation?");
  valheur.addoption("STM_NO_COMMIT_COUNTER", "No");
  valheur.addoption("STM_COMMIT_COUNTER", "Yes");
  valheur.setdefault(1);

  // privatization
  priv.setprompt("When privatization is needed, how would you like to ensure correctness?");
  priv.addoption("STM_PRIV_TFENCE", "Transactional Fence - long delay, but no overhead on private code");
  priv.addoption("STM_PRIV_VFENCE", "Validation Fence - less delay, but some overhead on private code");
  priv.addoption("STM_PRIV_NONBLOCKING", "Nonblocking - more overhead on transactions and on private code, but no delay");
  priv.addoption("STM_PRIV_LOGIC", "Program Logic, such as barriers and fork/join, are sufficient in my program");
  priv.setdefault(1);

  // privatization in word-based stms
  lwpriv.setprompt("When privatization is needed, how would you like to ensure correctness?");
  lwpriv.addoption("STM_PRIV_TFENCE", "Transactional Fence - long delay, but no overhead on private code");
  lwpriv.addoption("STM_PRIV_LOGIC", "Program Logic, such as barriers and fork/join, are sufficient in my program");
  lwpriv.setdefault(1);

  // inevitability
  lwinev.setprompt("Which inevitability option would you like to use?");
  lwinev.addoption("STM_INEV_NONE", "no-op (unsafe)");
  lwinev.addoption("STM_INEV_GRL", "Global Read-Write Lock");
  lwinev.setdefault(1);

  // inevitability in llt
  inev.setprompt("Which inevitability option would you like to use?");
  inev.addoption("STM_INEV_NONE", "no-op (unsafe)");
  inev.addoption("STM_INEV_GRL", "Global Read-Write Lock");
  inev.addoption("STM_INEV_GWL", "Global Write Lock");
  inev.addoption("STM_INEV_GWLFENCE", "Global Write Lock + Transactional Fence");
  inev.addoption("STM_INEV_DRAIN", "Drain");
  inev.addoption("STM_INEV_BLOOM_SMALL", "Bloom (small)");
  inev.addoption("STM_INEV_BLOOM_MEDIUM", "Bloom (medium)");
  inev.addoption("STM_INEV_BLOOM_LARGE", "Bloom (large)");
  inev.addoption("STM_INEV_IRL", "Individual Read Locks");
  inev.setdefault(1);

  // lock types
  locks.setprompt("Which lock would you like CGL to use?");
#if !defined(_POWER)
  locks.addoption("STM_LOCK_TATAS", "test-and-test-and-set with exponential backoff");
#endif
  if (OS != WINDOWS)
    locks.addoption("STM_LOCK_PTHREAD", "pthread_mutex_lock");
#if !defined(_POWER)
  locks.addoption("STM_LOCK_TICKET", "ticket lock");
  locks.addoption("STM_LOCK_MCS", "MCS lock");
#endif
  locks.setdefault(1);

  // retry
  retrys.setprompt("How would you like to implement retry?");
  retrys.addoption("STM_RETRY_SLEEP", "sleep briefly (50 usec) and then restart");
  retrys.addoption("STM_RETRY_VISREAD", "wait using visible read bits");
  retrys.addoption("STM_RETRY_BLOOM", "wait using bloom filters");
  retrys.setdefault(1);

  // descriptor caching
  cache_descriptor.setprompt("Would you like to cache transaction descriptors on the stack (reduces thread-local overhead)?");
  cache_descriptor.addoption("STM_API_CACHE_DESCRIPTOR", "Yes");
  cache_descriptor.addoption("STM_API_NO_CACHE_DESCRIPTOR", "No");
  cache_descriptor.setdefault(1);

  // setjmp or throw for rollback
  extendedrollback.setprompt("What mechanism would you like to use for rollback?");
  extendedrollback.addoption("STM_ROLLBACK_SETJMP", "Use setjmp/longjmp (will not call destructors for stack objects)");
#if !defined(_POWER)
  extendedrollback.addoption("STM_ROLLBACK_THROW", "Use throw/catch (may introduce serialization on locks)");
#endif
  extendedrollback.setdefault(1);

  rollback.setprompt("What mechanism would you like to use for rollback?");
  rollback.addoption("STM_ROLLBACK_SETJMP", "Use setjmp/longjmp (will not call destructors for stack objects)");
  rollback.setdefault(1);
  rollback.setoverride(1);

  // publication safety (STRICT only)
  lwpub.setprompt("How would you like to ensure SSS publication safety?");
  lwpub.addoption("STM_PUBLICATION_TFENCE", "Use a transaction fence");
  lwpub.addoption("STM_PUBLICATION_SHOOTDOWN", "Force concurrent transactions to abort");
  lwpub.setdefault(1);

  // privatization in strict
  ssspriv.setprompt("When privatization is needed, how would you like to ensure correctness?");
  ssspriv.addoption("STM_PRIV_TFENCE", "Transactional Fence - long delay, but no overhead on private code");
  ssspriv.addoption("STM_PRIV_LOGIC", "Program Logic, such as barriers and fork/join, are sufficient in my program");
  ssspriv.addoption("STM_PRIV_COMMIT_SERIALIZE", "Serialize transaction commit");
  ssspriv.addoption("STM_PRIV_COMMIT_FENCE", "Non-timestamp commit-time epoch");
  ssspriv.setdefault(1);

  // flow can be ALA or SFS
  owlpriv.setprompt("Are all transactions potential privatizers?");
  owlpriv.addoption("STM_PRIV_ALA", "Yes - use ALA privatization.");
  owlpriv.addoption("STM_PRIV_SFS", "No  - use SFS privatization.");
  owlpriv.setdefault(1);

  // fairstm can use basic, VisReadPrio, or BloomPrio CM
  faircm.setprompt("What CM strategy would you like?");
  faircm.addoption("STM_CM_LW", "LightWeight CM (no priority).");
  faircm.addoption("STM_CM_BLOOMPRIO", "CM with priority via Bloom Filters");
  faircm.addoption("STM_CM_VISREADPRIO", "CM with priority via Visible Reads");
  faircm.setdefault(1);

  // writeset
  writeset.setprompt("What kind of datastructure should be used to store your writeset?");
  writeset.addoption("STM_USE_HASHTABLE_WRITESET", "Hashtable - O(1) lookups");
  writeset.addoption("STM_USE_MINIVECTOR_WRITESET", "MiniVector - O(W) lookups");
  writeset.setdefault(1);

  // do you want API asserts on or off?
  api_asserts.setprompt("Do you want the API to use asserts to ensure the correct use of smart pointers?");
  api_asserts.addoption("STM_API_ASSERTS_OFF", "No thanks.");
  api_asserts.addoption("STM_API_ASSERTS_ON",  "Yes.");
  api_asserts.addoption("STM_API_ASSERTS_ON_NO_UN_PTR", "Yes, but don't have asserts for un_ptr use.");
  api_asserts.setdefault(1);
}

void csv_to_vector(string& input, vector<int>& ints)
{
  // we have a string and we think it is a comma-separated list of ints.
  // Let's create a vector from it
  string tmp = "";
  for (string::iterator i = input.begin(); i != input.end(); ++i) {
    if (*i >= '0' && *i <= '9') {
      // it's a digit, so add it to the tmp string
      tmp += *i;
    }
    else if (*i == ',') {
      // it's a delimiter, so work with the tmp string
      int t;
      stringstream(tmp) >> t;
      ints.push_back(t);
      tmp = "";
    }
    else
      cout << "error reading" << *i << " ... ignoring character" << endl;
  }
  if (tmp != "") {
    int t;
    stringstream(tmp) >> t;
    ints.push_back(t);
    tmp = "";
  }
}

// Rather than write out everything we need to do, we're going to fill this
// vector with all of the optionsets we want to run.
vector<OptionSet*> whichsets;

// var arg function that takes a bunch of optionsets and pushes them into
// whichsets
void addsets(OptionSet* a, ...)
{

  whichsets.push_back(a);
  va_list list;
  va_start(list, a);

  typedef OptionSet Set;
  for (Set* o = va_arg(list, Set*); o != NULL; o = va_arg(list, Set*))
    whichsets.push_back(o);

  va_end(list);
}

// given a LIB, put the right optionsets into the vector, so that we can use
// them in either interactive or default config
void pick_optionsets()
{
  if (LIB == "RSTM")      addsets(&cm, &valheur, &priv, &lwinev, &retrys, &cache_descriptor, &extendedrollback, &writeset, &api_asserts, NULL);
  if (LIB == "REDO_LOCK") addsets(&cm, &valheur, &priv, &lwinev, &cache_descriptor, &extendedrollback, &writeset, &api_asserts, NULL);
  if (LIB == "LLT")       addsets(&lwpriv, &inev, &cache_descriptor, &extendedrollback, &writeset, &api_asserts, NULL);
  if (LIB == "CGL")       addsets(&locks, &cache_descriptor, &api_asserts, NULL);
  if (LIB == "ET")        addsets(&lwpriv, &lwinev, &cache_descriptor, &rollback, &api_asserts, NULL);
  if (LIB == "TML")       addsets(&cache_descriptor, &rollback, &api_asserts, NULL);
  if (LIB == "PRECISE")   addsets(&cache_descriptor, &rollback, &api_asserts, NULL);
  if (LIB == "FLOW")      addsets(&lwinev, &owlpriv, &cache_descriptor, &rollback, &api_asserts, NULL);
  if (LIB == "TML_LAZY")  addsets(&cache_descriptor, &rollback, &api_asserts, NULL);
  if (LIB == "STRICT")    addsets(&ssspriv, &lwinev, &lwpub, &cache_descriptor, &rollback, &api_asserts, NULL);
  if (LIB == "SGLA")      addsets(&cache_descriptor, &rollback, &api_asserts, NULL);
  if (LIB == "FAIR")      addsets(&lwpriv, &cache_descriptor, &rollback, &faircm, &api_asserts, NULL);
  if (LIB == "RINGSW")    addsets(&cache_descriptor, &rollback, &api_asserts, NULL);
}

void clear_optionsets() {
  whichsets.clear();
}

class Iterator {
  const vector<string>::const_iterator begin, end;
  vector<string>::const_iterator current;
  bool overridden;

 public:
  Iterator() : begin(), end(), current(), overridden(false) { }
  Iterator(const vector<string>::const_iterator begin,
           const vector<string>::const_iterator end,
           vector<string>::const_iterator start,
           bool overridden = false)
    : begin(begin), end(end), current(start), overridden(overridden)
  { }

  Iterator& operator++() {
    current = (overridden) ? end : current + 1;
    return *this;
  }

  bool operator!=(const Iterator& rhs) const {
    return (current != rhs.current);
  }

  unsigned operator*() const {
    return current - begin + 1;
  }
};

Iterator begin(const OptionSet& s) {
  return Iterator(s.options.begin(), s.options.end(), s.options.begin());
}

Iterator end(const OptionSet& s) {
  return Iterator(s.options.begin(), s.options.end(), s.options.end());
}

Iterator begin(const OverrideableOptionSet& s) {
  return (s.override != -1) ?
    Iterator(s.options.begin(), s.options.end(), s.options.begin() + s.override - 1, true) :
    begin((OptionSet&)s);
}

// tracks the enumerated selections so that we can print them out when needed
vector<unsigned> selections;
// partial recursive enumerations of library dependent sets
void enumerate(unsigned setidx) {
  if (setidx == whichsets.size()) {
    cout << selections[0];
    for (unsigned i = 1; i < selections.size(); ++i)
      cout << ',' << selections[i];
    cout << "\n";
    return;
  }

  OptionSet* set = whichsets[setidx];
  for (Iterator i = begin(*set); i != end(*set); ++i) {
    selections.push_back(*i);
    enumerate(setidx + 1);
    selections.pop_back();
  }
}

void enumerate() {
  for (Iterator lib = begin(libs); lib != end(libs); ++lib) {
    // Set the selection, the rest of the selections depend on this.
    libs.setchoice(*lib);
    selections.push_back(*lib);
    for (Iterator tls = begin(tlocals); tls != end(tlocals); ++tls) {
      selections.push_back(*tls);
      // trigger the os
      bool nodebug;
      tlocals.OS_trigger(SOLARIS, 1, nodebug);
      for (Iterator prof = (nodebug) ? begin(profiles_nodebug) : begin(profiles_debug);
           prof != ((nodebug) ? end(profiles_nodebug) : end(profiles_debug)); ++prof)
      {
        selections.push_back(*prof);
        for (Iterator alloc = begin(allocators); alloc != end(allocators); ++alloc) {
          selections.push_back(*alloc);

          pick_optionsets();
          enumerate(0);
          clear_optionsets();
          selections.pop_back();
        }
        selections.pop_back();
      }
      selections.pop_back();
    }
    selections.pop_back();
  }
}

// interactive mode:  ask the user how to configure everything
void interactive_config()
{
  // now figure out what library we're building
  libs.interact();

  // now figure out the tlocal, because the build profile depends on it
  tlocals.interact();
  tlocals.OS_trigger(SOLARIS, 1, warn_solaris_tlocal);

  // get the build profile
  if (OS != WINDOWS) {
    if (warn_solaris_tlocal)
      profiles_nodebug.interact();
    else
      profiles_debug.interact();
  }

  // and set the allocator
  allocators.interact();
  allocators.LDFLAG_trigger(4, "-lmtmalloc ");

  // now use the value of the lib to pick which other optionsets to use
  pick_optionsets();

  // and run through them
  vector<OptionSet*>::iterator i;
  for (i = whichsets.begin(); i != whichsets.end(); ++i) {
    (*i)->interact();
  }
}

// default mode: just use defaults for everything
void default_config(vector<int>::iterator i)
{
  // default lib is RSTM
  libs.setchoice(*i);
  ++i;

  // now figure out the tlocal
  tlocals.setchoice(*i);
  ++i;
  tlocals.OS_trigger(SOLARIS, 1, warn_solaris_tlocal);

  // set the build profile to NORMAL
  if (OS != WINDOWS) {
    if (warn_solaris_tlocal) {
      profiles_nodebug.setchoice(*i);
      ++i;
    }
    else {
      profiles_debug.setchoice(*i);
      ++i;
    }
  }

  // and set the allocator to GCHEAP
  allocators.setchoice(*i);
  ++i;
  allocators.LDFLAG_trigger(4, "-lmtmalloc ");

  // now use the value of the lib to pick which other optionsets to use
  pick_optionsets();

  // and run through them
  vector<OptionSet*>::iterator s;
  for (s = whichsets.begin(); s != whichsets.end(); ++s) {
    (*s)->setchoice(*i);
    ++i;
  }
}

// main program: just check if the first arg is -D, and if so, use default
// mode, else use interactive mode to get configuration options.  Then generate
// the files and exit.
int main(int argc, char** argv)
{
  // set the globals relating to the cpu/os/cc of this combination
  platform();

  // configure all output strings
  init_strings();

  // see if we are being requested to do an enumeration
  if ((argc > 1) && (string(argv[1]) == "-E")) {
    enumerate();
    return 0;
  }

  // if a -D flag is given, we will use the defaults mechanism
  bool use_defaults = ((argc > 1) && (string(argv[1]) == "-D"));
  string csv = "";

  // if there was a parameter to -D, assume it is a comma-delimited list of
  // integers and turn it into a vector of ints.  then use that vector to drive
  // the default_config function
  if (use_defaults) {
    // is a csv provided?
    if (argc > 2) {
      csv = string(argv[2]);
    }
    // if not, use hard-coded string for the defaults
    else {
      // win32 doesn't do a debug flag
      if (OS == WINDOWS) {
        csv = "1,1,1,9,1,1,1,1,1,1,1,1,1,1,1";
      }
      else if (CPU == POWER) {
        csv = "1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1";
      }
      else {
        csv = "1,1,1,1,9,1,1,1,1,1,1,1,1,1,1,1";
      }
    }
    vector<int> defaults;
    csv_to_vector(csv, defaults);
    default_config(defaults.begin());
  }
  else {
    interactive_config();
  }

  // generate Makefile.inc
  if (OS != WINDOWS) {
    ofstream mkfile;
    mkfile.open("Makefile.inc");
    mkfile << "# This file was automatically generated" << endl;
    mkfile << AR << endl;
    mkfile << BUILD_CXX << endl;
    mkfile << FINAL_CXX << endl;
    mkfile << LDFLAGS << endl;
    mkfile << CXXFLAGS << endl;
    mkfile << STM_VERSION << endl;
    mkfile << MFLAGS << endl;
    mkfile.close();
  }

  // generate config.h
  ofstream cfgfile;
  cfgfile.open("config.h");
  cfgfile << "/* This file was automatically generated */" << endl;
  for (vector<string>::iterator i = CONFIGH.begin(); i != CONFIGH.end(); ++i)
    cfgfile << *i << endl;
  cfgfile.close();

  cout << "Configuration complete." << endl;
}
