// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   Copyright (C) 2012 by Andreas Lauser                                    *
 *   Copyright (C) 2012 by Klaus Mosthaf                                     *
 *                                                                           *
 *   This program is free software: you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation, either version 2 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 *****************************************************************************/
/*!
 * \file
 *
 * \brief Defines default values for the properties required by the
 *        Stokes box model.
 */
#ifndef DUMUX_STOKES_PROPERTY_DEFAULTS_HH
#define DUMUX_STOKES_PROPERTY_DEFAULTS_HH

#include "stokesproperties.hh"
#include "stokesindices.hh"
#include "stokeslocalresidual.hh"
#include "stokesmodel.hh"
#include "stokesvolumevariables.hh"
#include "stokesfluxvariables.hh"
#include "stokesboundaryratevector.hh"

#include <dumux/material/fluidsystems/gasphase.hh>
#include <dumux/material/fluidsystems/liquidphase.hh>
#include <dumux/material/components/nullcomponent.hh>
#include <dumux/material/heatconduction/fluidconduction.hh>

#include <dumux/material/fluidsystems/1pfluidsystem.hh>
#include <dumux/material/fluidstates/compositionalfluidstate.hh>

namespace Dumux {

namespace Properties {
//////////////////////////////////////////////////////////////////
// Properties
//////////////////////////////////////////////////////////////////

SET_PROP(BoxStokes, NumEq) //!< set the number of equations
{
    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;

public:
    static constexpr int value = GridView::dimensionworld + FluidSystem::numComponents;
};

//! the number of phases
SET_INT_PROP(BoxStokes, NumPhases, 1);

//! the number of components
SET_INT_PROP(BoxStokes, NumComponents, 1);

//! Use the Stokes local residual function for the Stokes model
SET_TYPE_PROP(BoxStokes,
              LocalResidual,
              StokesLocalResidual<TypeTag>);

//! Use the Stokes local residual function for the Stokes model
SET_TYPE_PROP(BoxStokes,
              BaseProblem,
              StokesProblem<TypeTag>);

//! Increase the relative tolerance of the newton method to 10^-7
SET_SCALAR_PROP(BoxStokes, NewtonRelTolerance, 1e-7);

#if HAVE_SUPERLU
SET_TYPE_PROP(BoxStokes, LinearSolver, SuperLUBackend<TypeTag>);
#else
#warning "No SuperLU installed. SuperLU is the recommended linear solver for the Stokes models."
#endif

//! the Model property
SET_TYPE_PROP(BoxStokes, Model, StokesModel<TypeTag>);

//! the VolumeVariables property
SET_TYPE_PROP(BoxStokes, VolumeVariables, StokesVolumeVariables<TypeTag>);

//! the FluxVariables property
SET_TYPE_PROP(BoxStokes, FluxVariables, StokesFluxVariables<TypeTag>);

//! the BoundaryRateVector property
SET_TYPE_PROP(BoxStokes, BoundaryRateVector, StokesBoundaryRateVector<TypeTag>);

//! The fluid system to use by default
SET_PROP(BoxStokes, FluidSystem)
{ private:
    typedef typename GET_PROP_TYPE(TypeTag, Fluid) Fluid;
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
public:
    typedef FluidSystems::OneP<Scalar, Fluid> type;
};

//! The fluid that is used in the single-phase fluidsystem.
SET_PROP(BoxStokes, Fluid)
{ private:
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
public:
    typedef Dumux::LiquidPhase<Scalar, Dumux::NullComponent<Scalar> > type;
};

SET_TYPE_PROP(BoxStokes, Indices, StokesIndices<TypeTag, /*PVOffset=*/0>);

//! Choose the type of the employed fluid state.
SET_PROP(BoxStokes, FluidState)
{
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, FluidSystem) FluidSystem;
public:
    typedef Dumux::CompositionalFluidState<Scalar, FluidSystem> type;
};

//! set the heat conduction law to a dummy one by default
SET_TYPE_PROP(BoxStokes,
              HeatConductionLaw,
              Dumux::FluidHeatConduction<typename GET_PROP_TYPE(TypeTag, FluidSystem),
                                         typename GET_PROP_TYPE(TypeTag, Scalar),
                                         GET_PROP_VALUE(TypeTag, StokesPhaseIndex)>);

//! extract the type parameter objects for the heat conduction law
//! from the law itself
SET_TYPE_PROP(BoxStokes,
              HeatConductionLawParams,
              typename GET_PROP_TYPE(TypeTag, HeatConductionLaw)::Params);

//! Set the phaseIndex per default to zero (important for two-phase fluidsystems).
SET_INT_PROP(BoxStokes, StokesPhaseIndex, 0);

//!< Disable the energy equation by default
SET_BOOL_PROP(BoxStokes, EnableEnergy, false);

//!< Disable the inertial term by default
SET_BOOL_PROP(BoxStokes, EnableNavierTerm, false);
}

}

#endif