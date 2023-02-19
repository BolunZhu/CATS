/******************************************************************************
 *
 * Copyright (c) 2007, 2008, 2009
 * University of Rochester
 * Department of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *    * Neither the name of the University of Rochester nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * STMList.hpp
 *
 * List for STM Game Objects.
 */

#ifndef __STMLIST_H__
#define __STMLIST_H__

#include "Game.hpp"

class GameObject;

class STMListNode : public stm::Object
{
    GENERATE_FIELD(sh_ptr<GameObject>, val);
    GENERATE_FIELD(sh_ptr<STMListNode>, next);
  public:

    // ctors
    STMListNode(sh_ptr<GameObject> val = sh_ptr<GameObject>(NULL))
        : m_val(val), m_next()
    { }

    STMListNode(sh_ptr<GameObject> val, const sh_ptr<STMListNode>& next)
        : m_val(val), m_next(next)
    { }
};

class STMList : public stm::Object
{
    GENERATE_FIELD(sh_ptr<STMListNode>, sentinel);

  public:
    /**
     *  construct an empty list
     */
    STMList() : m_sentinel(new STMListNode()) { }

    /**
     *  clear the list
     */
    void Clear(wr_ptr<STMList> wrThis, int iThreadID = -1)
    {
        // [TODO]
    }

    void Insert(wr_ptr<STMList> wrThis, sh_ptr<GameObject> val, int iThreadID = -1);
    void Remove(wr_ptr<STMList> wrThis, sh_ptr<GameObject> val, int iThreadID = -1);
    std::vector< sh_ptr<GameObject> > GrabAll(rd_ptr<STMList> rdThis,
                                              int iThreadID = -1) const;

};

#endif // __STMLIST_H__
