/////////////////////////////////////////////////////////////////////////////
// Name:        tie.cpp
// Author:      Rodolfo Zitellini
// Created:     26/06/2012
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "tie.h"

//----------------------------------------------------------------------------

#include <assert.h>

//----------------------------------------------------------------------------

namespace vrv {

//----------------------------------------------------------------------------
// Tie
//----------------------------------------------------------------------------

Tie::Tie() : ControlElement("tie-"), TimeSpanningInterface(), AttCurvature()
{
    RegisterInterface(TimeSpanningInterface::GetAttClasses(), TimeSpanningInterface::IsInterface());
    RegisterAttClass(ATT_CURVATURE);

    Reset();
}

Tie::~Tie()
{
}

void Tie::Reset()
{
    ControlElement::Reset();
    TimeSpanningInterface::Reset();
    ResetCurvature();
}

} // namespace vrv