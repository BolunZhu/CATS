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

#include <iostream>

#include "ContentionManager.hpp"
#include "Polka.hpp"
#include "Karma.hpp"
#include "Eruption.hpp"
#include "Polite.hpp"
#include "Timestamp.hpp"
#include "Polkaruption.hpp"
#include "Killblocked.hpp"
#include "Aggressive.hpp"
#include "Highlander.hpp"
#include "Whpolka.hpp"
#include "Greedy.hpp"
#include "Polkavis.hpp"
#include "Serializer.hpp"
#include "Justice.hpp"
#include "Timid.hpp"
#include "Reincarnate.hpp"
#include "Gladiator.hpp"
#include "AggressiveR.hpp"
#include "TimidR.hpp"
#include "PoliteR.hpp"

// create the appropriate cm based on the input string
stm::ContentionManager* stm::Factory(std::string cm_type)
{
    if (cm_type == "Eruption")
        return new Eruption();
    else if (cm_type == "Serializer")
        return new Serializer();
    else if (cm_type == "Justice")
        return new Justice();
    else if (cm_type == "Karma")
        return new Karma();
    else if (cm_type == "Killblocked")
        return new Killblocked();
    else if (cm_type == "Polite")
        return new Polite();
    else if (cm_type == "Polka")
        return new Polka();
    else if (cm_type == "Polkaruption")
        return new Polkaruption();
    else if (cm_type == "Timestamp")
        return new Timestamp();
    else if (cm_type == "Aggressive")
        return new Aggressive();
    else if (cm_type == "Highlander")
        return new Highlander();
    else if (cm_type == "Whpolka")
        return new Whpolka();
    else if (cm_type == "Greedy")
        return new Greedy();
    else if (cm_type == "Polkavis")
        return new Polkavis();
    else if (cm_type == "Timid")
        return new Timid();
    else if (cm_type == "Reincarnate")
        return new Reincarnate();
    else if (cm_type == "Gladiator")
        return new Gladiator();
    else if (cm_type == "PoliteR")
        return new PoliteR();
    else if (cm_type == "TimidR")
        return new TimidR();
    else if (cm_type == "AggressiveR")
        return new AggressiveR();
    else {
        std::cerr << "*** Warning: unknown contention manager "
                  << cm_type << std::endl;
        std::cerr << "*** Defaulting to Polka." << std::endl;
        return new Polka();
    }
}

// provide backing for static variables in different CMs
volatile unsigned long stm::Greedy::timeCounter = 0;
volatile unsigned long stm::Serializer::timeCounter = 0;
volatile unsigned long stm::Reincarnate::timeCounter = 0;
