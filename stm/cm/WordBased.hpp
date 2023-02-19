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

#ifndef WBCM_H__
#define WBCM_H__

#include <string>
#include <iostream>
#include "../support/hrtime.h"

#if defined(_MSC_VER)
#include "alt-license/rand_r.h"
#endif

namespace stm
{
  /*** Lightweight Contention Manager for et framework */
  class WBCM
  {
      // allow selection at construction time
      enum policy_t { TIMID = 0, POLITE = 1, AGGRESSIVE = 2, PATIENT = 3 };
      policy_t policy;

      // for exponential backoff in Polite
      unsigned tries;

      // for randomized exponential backoff on abort
      bool abort_use_reb;
      unsigned abort_bits; // how many bits of a rand to use
      unsigned abort_seed;
      unsigned MIN_ABORT_BITS; // 4
      unsigned MAX_ABORT_BITS; // 16

      // exponential backoff thresholds
      enum thresh_t {
          MIN_BACKOFF = 4,
          MAX_BACKOFF = 16,
          MAX_BACKOFF_RETRIES = MAX_BACKOFF - MIN_BACKOFF + 1
      };

      // for randomization of backoff
      unsigned int seed;

      /**
       *  randomized exponential backoff, stolen from Polite.hpp
       */
      void backoff()
      {
          if (tries > 0 && tries <= MAX_BACKOFF_RETRIES) {
              // what time is it now
              unsigned long long starttime = getElapsedTime();

              // how long should we wait (random)
              unsigned long delay = rand_r(&seed);
              delay = delay % (1 << (tries + MIN_BACKOFF));

              // spin until /delay/ nanoseconds have passed.  We can do
              // whatever we want in the spin, as long as it doesn't have
              // an impact on other threads
              unsigned long long endtime;
              do {
                  endtime = getElapsedTime();
              } while (endtime < starttime + delay);
          }
          tries++;
      }

      void abort_backoff()
      {
          // get a random amount of time to wait, bounded by an exponentially
          // increasing limit
          unsigned long delay = rand_r(&abort_seed);
          delay &= ((1 << abort_bits)-1);

          // wait until at least that many ns have passed
          unsigned long long start = getElapsedTime();
          unsigned long long stop_at = start + delay;

          while (getElapsedTime() < stop_at) { spin64(); }

          // advance limit for next time
          if (abort_bits < MAX_ABORT_BITS)
              abort_bits++;
      }


    public:
      // conflict resolution options
      enum ConflictResolutions { AbortSelf, AbortOther, Wait };

      WBCM(std::string POLICY)
          : tries(1),
            abort_use_reb(false), abort_bits(MIN_ABORT_BITS),
            abort_seed((unsigned long)&abort_bits),
            MIN_ABORT_BITS(4), MAX_ABORT_BITS(16),
            seed((unsigned long)&tries)
      {
          if (POLICY == "Polite")
              policy = POLITE;
          else if (POLICY == "Aggressive")
              policy = AGGRESSIVE;
          else if (POLICY == "Timid")
              policy = TIMID;
          else if (POLICY == "Patient")
              policy = PATIENT;
          else if (POLICY == "PoliteR") {
              policy = POLITE;
              abort_use_reb = true;
          }
          else if (POLICY == "PatientR") {
              policy = PATIENT;
              abort_use_reb = true;
          }
          else if (POLICY == "AggressiveR") {
              policy = AGGRESSIVE;
              abort_use_reb = true;
          }
          else if (POLICY == "TimidR") {
              policy = TIMID;
              abort_use_reb = true;
          }
          // this is the "choose your own exponential backoff parameters"
          else if (POLICY.find_first_of('-') < POLICY.size()) {
              std::string str = POLICY;
              int pos1 = str.find_first_of('-', 0);
              int pos2 = str.find_first_of('-', pos1+1);
              int pos3 = str.size();
              int p1 = atoi(str.substr(pos1+1, pos2-pos1-1).c_str());
              int p2 = atoi(str.substr(pos2+1, pos3-pos2-1).c_str());
              std::string p = str.substr(0, pos1);
              if (p == "Polite") {
                  policy = POLITE;
              }
              else if (p == "Aggressive") {
                  policy = AGGRESSIVE;
              }
              else if (p == "Timid") {
                  policy = TIMID;
              }
              else if (p == "Patient") {
                  policy = PATIENT;
              }
              else {
                  std::cerr << "Unrecognized CM " << p << " using Timid" << std::endl;
                  policy = TIMID;
              }
              abort_use_reb = true;
              MIN_ABORT_BITS = p1;
              MAX_ABORT_BITS = p2;
              abort_bits = MIN_ABORT_BITS;
          }
          else {
              std::cerr << "Unsupported CM option " << POLICY
                        << "... defaulting to Timid" << std::endl;
              policy = TIMID;
          }
      }

      /**
       *  Simple Event Methods
       */
      void onBegin()  { tries = 1; }
      void onOpen()   { tries = 1; }
      void onCommit() { abort_bits = MIN_ABORT_BITS; }

      void onAbort()
      {
          if (abort_use_reb)
              abort_backoff();
      }

      // request permission to abort enemy tx.  For now, policies are totally
      // local, so we don't need a parameter to this method
      ConflictResolutions onConflict()
      {
          if (policy == POLITE) {
              if (tries > MAX_BACKOFF_RETRIES)
                  return AbortOther;
              backoff();
              return Wait;
          }
          else if (policy == PATIENT) {
              return Wait;
          }
          else if (policy == AGGRESSIVE) {
              return AbortOther;
          }
          else if (policy == TIMID) {
              return AbortSelf;
          }
          return Wait;
      }
  };
} // namespace stm

#endif // WBCM_H__
