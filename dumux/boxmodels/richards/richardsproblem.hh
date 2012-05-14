// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   Copyright (C) 2009-2012 by Andreas Lauser                               *
 *   Institute for Modelling Hydraulic and Environmental Systems             *
 *   University of Stuttgart, Germany                                        *
 *   email: <givenname>.<name>@iws.uni-stuttgart.de                          *
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
 * \brief Base class for all problems which use the box scheme
 */
#ifndef DUMUX_RICHARDS_PROBLEM_HH
#define DUMUX_RICHARDS_PROBLEM_HH

#include <dumux/boxmodels/common/boxmultiphaseproblem.hh>

#include "richardsproperties.hh"
#include <dune/common/fvector.hh>

namespace Dumux
{
/*!
 * \ingroup RichardsModel
 * \ingroup BoxBaseProblems
 * \brief Base class for all problems which use the two-phase box model
 *
 * For a description of the Richards model, see Dumux::RichardsModel
 */
template<class TypeTag>
class RichardsBoxProblem : public BoxMultiPhaseProblem<TypeTag>
{
    typedef BoxMultiPhaseProblem<TypeTag> ParentType;

    typedef typename GET_PROP_TYPE(TypeTag, Problem) Implementation;
    typedef typename GET_PROP_TYPE(TypeTag, TimeManager) TimeManager;
    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;

    enum {
        dim = GridView::dimension,
        dimWorld = GridView::dimensionworld
    };


    typedef Dune::FieldVector<Scalar, dimWorld> DimVector;

public:
    /*!
     * \brief The constructor.
     *
     * The overloaded class must allocate all data structures
     * required, but _must not_ do any calls to the model, the
     * jacobian assembler, etc inside the constructor.
     *
     * If the problem requires information from these, the
     * BoxProblem::init() method be overloaded.
     *
     * \param timeManager The TimeManager which keeps track of time
     * \param gridView The GridView used by the problem.
     */
    RichardsBoxProblem(TimeManager &timeManager, const GridView &gridView)
        : ParentType(timeManager, gridView),
          gravity_(0)
    {
        gravity_ = 0;
        if (GET_PARAM(TypeTag, bool, EnableGravity))
            gravity_[dim-1]  = -9.81;
    }

    /*!
     * \name Problem parameters
     */
    // \{

    /*!
     * \brief Returns the acceleration due to gravity \f$\mathrm{[m/s^2]}\f$.
     *
     * \param context Container for the volume variables, element,
     *                fvElementGeometry, etc
     * \param spaceIdx The local index of the sub control volume inside
     *                 the element
     */
    template <class Context>
    const DimVector &gravity(const Context &context,
                          int spaceIdx, int timeIdx) const
    {
        return asImp_().gravity();
    }

    /*!
     * \brief Returns the acceleration due to gravity \f$\mathrm{[m/s^2]}\f$.
     *
     * This method is used for problems where the gravitational
     * acceleration does not depend on the spatial position. The
     * default behaviour is that if the <tt>EnableGravity</tt>
     * property is true, \f$\boldsymbol{g} = ( 0,\dots,\ -9.81)^T \f$ holds,
     * else \f$\boldsymbol{g} = ( 0,\dots, 0)^T \f$.
     */
    const DimVector &gravity() const
    { return gravity_; }


    // \}

private:
    Implementation &asImp_()
    { return *static_cast<Implementation*>(this); }
    const Implementation &asImp_() const
    { return *static_cast<const Implementation*>(this); }

    // the gravity vector
    DimVector gravity_;
};

}

#endif
