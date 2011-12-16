// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   Copyright (C) 2010 by Markus Wolff                                      *
 *   Copyright (C) 2007-2010 by Yufei Cao                                    *
 *   Institute of Applied Analysis and Numerical Simulation                  *
 *   University of Stuttgart, Germany                                        *
 *   email: <givenname>.<name>@mathematik.uni-stuttgart.de                   *
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

#ifndef DUMUX_MPFAOVELOCITY2P_HH
#define DUMUX_MPFAOVELOCITY2P_HH

#include "dumux/decoupled/2p/diffusion/fvmpfa/fvmpfaopressure2p.hh"

/**
 * @file
 * @brief  Velocity Field from a finite volume MPFA O-method solution of a pressure equation.
 * @author Yufei Cao
 */

namespace Dumux
{
/*! \ingroup FV2p
 *
 * \brief Velocity calculation for the MPFA-O method
 */
template<class TypeTag> class FVMPFAOVelocity2P: public FVMPFAOPressure2P<TypeTag>
{
    typedef FVMPFAOVelocity2P<TypeTag> ThisType;
    typedef FVMPFAOPressure2P<TypeTag> ParentType;typedef typename GET_PROP_TYPE(TypeTag, PTAG(GridView)) GridView;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Scalar)) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Problem)) Problem;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(SpatialParameters)) SpatialParameters;
    typedef typename SpatialParameters::MaterialLaw MaterialLaw;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(TwoPIndices)) Indices;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(FluidSystem)) FluidSystem;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(FluidState)) FluidState;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(BoundaryTypes)) BoundaryTypes;
    typedef typename GET_PROP(TypeTag, PTAG(SolutionTypes)) SolutionTypes;
    typedef typename SolutionTypes::PrimaryVariables PrimaryVariables;

    typedef typename GridView::Traits::template Codim<0>::Entity Element;
    typedef typename GridView::Grid Grid;
    typedef typename GridView::IndexSet IndexSet;
    typedef typename GridView::template Codim<0>::Iterator ElementIterator;
    typedef typename GridView::IntersectionIterator IntersectionIterator;
    typedef typename Grid::template Codim<0>::EntityPointer ElementPointer;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(GridTypeIndices)) GridTypeIndices;

    enum
    {
        dim = GridView::dimension, dimWorld = GridView::dimensionworld
    };
    enum
    {
        Sw = Indices::saturationW,
        Sn = Indices::saturationNW,
        vw = Indices::velocityW,
        vn = Indices::velocityNW,
        vt = Indices::velocityTotal,
    };
    enum
    {
        wPhaseIdx = Indices::wPhaseIdx,
        nPhaseIdx = Indices::nPhaseIdx,
        pressureIdx = Indices::pressureIdx,
        saturationIdx = Indices::saturationIdx,
        pressEqIdx = Indices::pressEqIdx,
        satEqIdx = Indices::satEqIdx
    };

    typedef Dune::FieldVector<Scalar, dim> LocalPosition;
    typedef Dune::FieldVector<Scalar, dimWorld> GlobalPosition;
    typedef Dune::FieldMatrix<Scalar, dim, dim> FieldMatrix;
    typedef Dune::FieldVector<Scalar, dim> FieldVector;

public:
    //! Constructs a FVMPFAOVelocity2P object
    /*!
     * \param problem a problem class object
     */
    FVMPFAOVelocity2P(Problem& problem) :
        ParentType(problem)
    {
    }

    //! \copydoc Dumux::FVVelocity2P::calculateVelocity()
    void calculateVelocity();

}; // end of template

template<class TypeTag>
void FVMPFAOVelocity2P<TypeTag>::calculateVelocity()
{
    // introduce matrix R for vector rotation and R is initialized as zero matrix
    FieldMatrix R(0);

    // evaluate matrix R
    if (dim == 2)
        for (int i = 0; i < dim; ++i)
        {
            R[0][1] = 1;
            R[1][0] = -1;
        }

    BoundaryTypes bcType;

    // run through all elements
    ElementIterator eItEnd = this->problem().gridView().template end<0> ();
    for (ElementIterator eIt = this->problem().gridView().template begin<0> (); eIt != eItEnd; ++eIt)
    {
        // get common geometry information for the following computation

        // cell 1 geometry type
        //Dune::GeometryType gt1 = eIt->geometry().type();

        // get global coordinate of cell 1 center
        GlobalPosition globalPos1 = eIt->geometry().center();

        // cell 1 volume
        Scalar volume1 = eIt->geometry().volume();

        // cell 1 index
        int globalIdx1 = this->problem().variables().index(*eIt);

        // get pressure value
        Scalar press1 = this->problem().variables().pressure()[globalIdx1];

        // get right hand side
        PrimaryVariables source(0.0);
        this->problem().source(source, *eIt);
        Scalar q1 = source[wPhaseIdx] + source[nPhaseIdx];

        // get absolute permeability of cell 1
        FieldMatrix K1(this->problem().spatialParameters().intrinsicPermeability(*eIt));

        // compute total mobility of cell 1
        Scalar lambda1 = this->problem().variables().mobilityWetting(globalIdx1)
                    + this->problem().variables().mobilityNonwetting(globalIdx1);

        // this 'for' loop can be deleted since velocity is
        // initiated in variableclass.hh
        for (int i = 0; i < 2 * dim; i++)
        {
            this->problem().variables().velocity()[globalIdx1][i] = 0;
        }

        //get the densities
        Scalar densityW = this->problem().variables().densityWetting(globalIdx1);
        Scalar densityNW = this->problem().variables().densityNonwetting(globalIdx1);

        // the following two variables are used to check local conservation
        Scalar facevol[2 * dim];
        GlobalPosition unitOuterNormal[2 * dim];

        IntersectionIterator isItBegin = this->problem().gridView().ibegin(*eIt);
        IntersectionIterator isItEnd = this->problem().gridView().iend(*eIt);
        for (IntersectionIterator isIt = isItBegin; isIt != isItEnd; ++isIt)
        {
            // intersection iterator 'nextisIt' is used to get geometry information
            IntersectionIterator tempisIt = isIt;
            IntersectionIterator tempisItBegin = isItBegin;

            IntersectionIterator nextisIt = ++tempisIt;

            //get nextisIt
            switch (GET_PROP_VALUE(TypeTag, PTAG(GridImplementation)))
            {
                // for SGrid
                case GridTypeIndices::sGrid:
                {
                    if (nextisIt == isItEnd)
                    {
                        nextisIt = isItBegin;
                    }
                    else
                    {
                        nextisIt = ++tempisIt;

                        if (nextisIt == isItEnd)
                        {
                            nextisIt = ++tempisItBegin;
                        }
                    }

                    break;
                }
                // for YaspGrid
                case GridTypeIndices::yaspGrid:
                {
                    if (nextisIt == isItEnd)
                    {
                        nextisIt = isItBegin;
                    }
                    else
                    {
                        nextisIt = ++tempisIt;

                        if (nextisIt == isItEnd)
                        {
                            nextisIt = ++tempisItBegin;
                        }
                    }

                    break;
                }
                // for UGGrid
                case GridTypeIndices::ugGrid:
                {
                    if (nextisIt == isItEnd)
                    nextisIt = isItBegin;

                    break;
                }
                default:
                {
                    DUNE_THROW(Dune::NotImplemented, "GridType can not be used with MPFAO implementation!");
                    break;
                }
            }

            // get geometry type of face 'isIt', i.e., the face between cell1 and cell2 (locally numbered)
            //Dune::GeometryType gtf12 = isIt->geometryInInside().type();

            // center of face in global coordinates, i.e., the midpoint of edge 'isIt'
            GlobalPosition globalPosFace12 = isIt->geometry().center();

            // get local number of facet 'isIt'
            int indexInInside = isIt->indexInInside();

            // get face volume
            Scalar face12vol = isIt->geometry().volume();

            // get face volume to check if local mass conservative
            facevol[indexInInside] = isIt->geometry().volume();

            // get outer normal vector scaled with half volume of face 'isIt'
            GlobalPosition integrationOuterNormaln1 = isIt->centerUnitOuterNormal();
            integrationOuterNormaln1 *= face12vol / 2.0;

            // get unit outer normal vector of face 'isIt'
            const GlobalPosition& unitOuterNormaln1 = isIt->centerUnitOuterNormal();

            // get unit outer normal vector of face 'isIt' to check if local mass conservative
            unitOuterNormal[indexInInside] = unitOuterNormaln1;

            // get geometry type of 'nextisIt', i.e., face between cell1 and cell3 (locally numbered)
            //Dune::GeometryType gtf13 = nextisIt->geometryInInside().type();

            // center of face in global coordinates, i.e., the midpoint of edge 'nextisIt'
            GlobalPosition globalPosFace13 = nextisIt->geometry().center();

            // get local number of facet 'nextisIt'
            int nextindexInInside = nextisIt->indexInInside();

            // get face volume
            Scalar face13vol = nextisIt->geometry().volume();

            // get outer normal vector scaled with half volume of face 'nextisIt'
            GlobalPosition integrationOuterNormaln3 = nextisIt->centerUnitOuterNormal();
            integrationOuterNormaln3 *= face13vol / 2.0;

            // get unit outer normal vector of face 'nextisIt'
            const GlobalPosition& unitOuterNormaln3 = nextisIt->centerUnitOuterNormal();

            // get the intersection node /bar^{x_3} between 'isIt' and 'nextisIt', denoted as 'corner1234'
            // initialization of corner1234
            GlobalPosition corner1234(0);

            // get the global coordinate of corner1234
            for (int i = 0; i < isIt->geometry().corners(); ++i)
            {
                GlobalPosition isItcorner = isIt->geometry().corner(i);

                for (int j = 0; j < nextisIt->geometry().corners(); ++j)
                {
                    GlobalPosition nextisItcorner = nextisIt->geometry().corner(j);

                    if (nextisItcorner == isItcorner)
                    {
                        corner1234 = isItcorner;
                        continue;
                    }
                }
            }

            // handle interior face
            if (isIt->neighbor())
            {
                // access neighbor cell 2 of 'isIt'
                ElementPointer outside = isIt->outside();
                int globalIdx2 = this->problem().variables().index(*outside);

                // get pressure value
                Scalar press2 = this->problem().variables().pressure()[globalIdx2];

                // neighbor cell 2 geometry type
                //Dune::GeometryType gt2 = outside->geometry().type();

                // get global coordinate of neighbor cell 2 center
                GlobalPosition globalPos2 = outside->geometry().center();

                // get absolute permeability of neighbor cell 2
                FieldMatrix K2(this->problem().spatialParameters().intrinsicPermeability(*outside));

                // get total mobility of neighbor cell 2
                Scalar lambda2 = this->problem().variables().mobilityWetting(globalIdx2)
                            + this->problem().variables().mobilityNonwetting(globalIdx2);

                // 'nextisIt' is an interior face
                if (nextisIt->neighbor())
                {
                    // get basic information of cell 1,2's neighbor cell 3,4
                    // neighbor cell 3
                    // access neighbor cell 3
                    ElementPointer nextisItoutside = nextisIt->outside();
                    int globalIdx3 = this->problem().variables().index(*nextisItoutside);

                    // get pressure value
                    Scalar press3 = this->problem().variables().pressure()[globalIdx3];

                    // neighbor cell 3 geometry type
                    //Dune::GeometryType gt3 = nextisItoutside->geometry().type();

                    // get global coordinate of neighbor cell 3 center
                    GlobalPosition globalPos3 = nextisItoutside->geometry().center();

                    // get absolute permeability of neighbor cell 3
                    FieldMatrix K3(this->problem().spatialParameters().intrinsicPermeability(*nextisItoutside));

                    // get total mobility of neighbor cell 3
                    Scalar lambda3 = this->problem().variables().mobilityWetting(globalIdx3)
                                + this->problem().variables().mobilityNonwetting(globalIdx3);

                    // neighbor cell 4
                    GlobalPosition globalPos4(0);
                    FieldMatrix K4(0);
                    Scalar lambda4 = 0;
                    int globalIdx4 = 0;

                    IntersectionIterator innerisItEnd = this->problem().gridView().iend(*outside);
                    IntersectionIterator innernextisItEnd = this->problem().gridView().iend(*nextisItoutside);
                    for (IntersectionIterator innerisIt = this->problem().gridView().ibegin(*outside); innerisIt
                            != innerisItEnd; ++innerisIt)
                        for (IntersectionIterator innernextisIt = this->problem().gridView().ibegin(
                                *nextisItoutside); innernextisIt != innernextisItEnd; ++innernextisIt)
                        {
                            if (innerisIt->neighbor() && innernextisIt->neighbor())
                            {
                                ElementPointer innerisItoutside = innerisIt->outside();
                                ElementPointer innernextisItoutside = innernextisIt->outside();

                                // find the common neighbor cell between cell 2 and cell 3, except cell 1
                                if (innerisItoutside == innernextisItoutside && innerisItoutside != isIt->inside())
                                {
                                    // access neighbor cell 4
                                    globalIdx4 = this->problem().variables().index(*innerisItoutside);

                                    // neighbor cell 4 geometry type
                                    //Dune::GeometryType gt4 = innerisItoutside->geometry().type();

                                    // get global coordinate of neighbor cell 4 center
                                    globalPos4 = innerisItoutside->geometry().center();

                                    // get absolute permeability of neighbor cell 4
                                    K4 += this->problem().spatialParameters().intrinsicPermeability(*innerisItoutside);

                                    lambda4 = this->problem().variables().mobilityWetting(globalIdx4)
                                                + this->problem().variables().mobilityNonwetting(globalIdx4);
                                }
                            }
                        }

                    // get pressure value
                    Scalar press4 = this->problem().variables().pressure()[globalIdx4];

                    // computation of flux through the first half edge of 'isIt' and the flux
                    // through the second half edge of 'nextisIt'

                    // get the information of the face 'isIt24' between cell2 and cell4 (locally numbered)
                    IntersectionIterator isIt24 = this->problem().gridView().ibegin(*outside);

                    for (IntersectionIterator innerisIt = this->problem().gridView().ibegin(*outside); innerisIt
                            != innerisItEnd; ++innerisIt)
                    {
                        if (innerisIt->neighbor())
                        {
                            if (innerisIt->outside() != isIt->inside())
                            {
                                for (int i = 0; i < innerisIt->geometry().corners(); ++i)
                                {
                                    GlobalPosition innerisItcorner = innerisIt->geometry().corner(i);

                                    if (innerisItcorner == corner1234)
                                    {
                                        isIt24 = innerisIt;
                                        continue;
                                    }
                                }
                            }
                        }
                    }

                    // get geometry type of face 'isIt24'
                    //Dune::GeometryType gtf24 = isIt24->geometryInInside().type();

                    // center of face in global coordinates, i.e., the midpoint of edge 'isIt24'
                    GlobalPosition globalPosFace24 = isIt24->geometry().center();

                    // get face volume
                    Scalar face24vol = isIt24->geometry().volume();

                    // get outer normal vector scaled with half volume of face 'isIt24'
                    GlobalPosition integrationOuterNormaln4 = isIt24->centerUnitOuterNormal();
                    integrationOuterNormaln4 *= face24vol / 2.0;

                    // get the information of the face 'isIt34' between cell3 and cell4 (locally numbered)
                    IntersectionIterator isIt34 = this->problem().gridView().ibegin(*nextisItoutside);

                    for (IntersectionIterator innerisIt = this->problem().gridView().ibegin(*nextisItoutside); innerisIt
                            != innernextisItEnd; ++innerisIt)
                    {
                        if (innerisIt->neighbor())
                        {
                            if (innerisIt->outside() != isIt->inside())
                            {
                                for (int i = 0; i < innerisIt->geometry().corners(); ++i)
                                {
                                    GlobalPosition innerisItcorner = innerisIt->geometry().corner(i);

                                    if (innerisItcorner == corner1234)
                                    {
                                        isIt34 = innerisIt;
                                        continue;
                                    }
                                }
                            }
                        }
                    }

                    // get geometry type of face 'isIt34'
                    //Dune::GeometryType gtf34 = isIt34->geometryInInside().type();

                    // center of face in global coordinates, i.e., the midpoint of edge 'isIt34'
                    GlobalPosition globalPosFace34 = isIt34->geometry().center();

                    // get face volume
                    Scalar face34vol = isIt34->geometry().volume();

                    // get outer normal vector scaled with half volume of face 'isIt34'
                    GlobalPosition integrationOuterNormaln2 = isIt34->centerUnitOuterNormal();
                    integrationOuterNormaln2 *= face34vol / 2.0;

                    // compute normal vectors nu11,nu21; nu12, nu22; nu13, nu23; nu14, nu24;
                    FieldVector nu11(0);
                    R.umv(globalPosFace13 - globalPos1, nu11);

                    FieldVector nu21(0);
                    R.umv(globalPos1 - globalPosFace12, nu21);

                    FieldVector nu12(0);
                    R.umv(globalPosFace24 - globalPos2, nu12);

                    FieldVector nu22(0);
                    R.umv(globalPosFace12 - globalPos2, nu22);

                    FieldVector nu13(0);
                    R.umv(globalPos3 - globalPosFace13, nu13);

                    FieldVector nu23(0);
                    R.umv(globalPos3 - globalPosFace34, nu23);

                    FieldVector nu14(0);
                    R.umv(globalPos4 - globalPosFace24, nu14);

                    FieldVector nu24(0);
                    R.umv(globalPosFace34 - globalPos4, nu24);

                    // compute dF1, dF2, dF3, dF4 i.e., the area of quadrilateral made by normal vectors 'nu'
                    FieldVector Rnu21(0);
                    R.umv(nu21, Rnu21);
                    Scalar dF1 = fabs(nu11 * Rnu21);

                    FieldVector Rnu22(0);
                    R.umv(nu22, Rnu22);
                    Scalar dF2 = fabs(nu12 * Rnu22);

                    FieldVector Rnu23(0);
                    R.umv(nu23, Rnu23);
                    Scalar dF3 = fabs(nu13 * Rnu23);

                    FieldVector Rnu24(0);
                    R.umv(nu24, Rnu24);
                    Scalar dF4 = fabs(nu14 * Rnu24);

                    // compute components needed for flux calculation, denoted as 'g'
                    FieldVector K1nu11(0);
                    K1.umv(nu11, K1nu11);
                    FieldVector K1nu21(0);
                    K1.umv(nu21, K1nu21);
                    FieldVector K2nu12(0);
                    K2.umv(nu12, K2nu12);
                    FieldVector K2nu22(0);
                    K2.umv(nu22, K2nu22);
                    FieldVector K3nu13(0);
                    K3.umv(nu13, K3nu13);
                    FieldVector K3nu23(0);
                    K3.umv(nu23, K3nu23);
                    FieldVector K4nu14(0);
                    K4.umv(nu14, K4nu14);
                    FieldVector K4nu24(0);
                    K4.umv(nu24, K4nu24);
                    Scalar g111 = lambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                    Scalar g121 = lambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                    Scalar g211 = lambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                    Scalar g221 = lambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;
                    Scalar g112 = lambda2 * (integrationOuterNormaln1 * K2nu12) / dF2;
                    Scalar g122 = lambda2 * (integrationOuterNormaln1 * K2nu22) / dF2;
                    Scalar g212 = lambda2 * (integrationOuterNormaln4 * K2nu12) / dF2;
                    Scalar g222 = lambda2 * (integrationOuterNormaln4 * K2nu22) / dF2;
                    Scalar g113 = lambda3 * (integrationOuterNormaln2 * K3nu13) / dF3;
                    Scalar g123 = lambda3 * (integrationOuterNormaln2 * K3nu23) / dF3;
                    Scalar g213 = lambda3 * (integrationOuterNormaln3 * K3nu13) / dF3;
                    Scalar g223 = lambda3 * (integrationOuterNormaln3 * K3nu23) / dF3;
                    Scalar g114 = lambda4 * (integrationOuterNormaln2 * K4nu14) / dF4;
                    Scalar g124 = lambda4 * (integrationOuterNormaln2 * K4nu24) / dF4;
                    Scalar g214 = lambda4 * (integrationOuterNormaln4 * K4nu14) / dF4;
                    Scalar g224 = lambda4 * (integrationOuterNormaln4 * K4nu24) / dF4;

                    // compute transmissibility matrix T = CA^{-1}B+F
                    Dune::FieldMatrix<Scalar, 2 * dim, 2 * dim> C(0), F(0), A(0), B(0);

                    // evaluate matrix C, F, A, B
                    C[0][0] = -g111;
                    C[0][2] = -g121;
                    C[1][1] = g114;
                    C[1][3] = g124;
                    C[2][1] = -g213;
                    C[2][2] = g223;
                    C[3][0] = g212;
                    C[3][3] = -g222;

                    F[0][0] = g111 + g121;
                    F[1][3] = -g114 - g124;
                    F[2][2] = g213 - g223;
                    F[3][1] = -g212 + g222;

                    A[0][0] = g111 + g112;
                    A[0][2] = g121;
                    A[0][3] = -g122;
                    A[1][1] = g114 + g113;
                    A[1][2] = -g123;
                    A[1][3] = g124;
                    A[2][0] = g211;
                    A[2][1] = -g213;
                    A[2][2] = g223 + g221;
                    A[3][0] = -g212;
                    A[3][1] = g214;
                    A[3][3] = g222 + g224;

                    B[0][0] = g111 + g121;
                    B[0][1] = g112 - g122;
                    B[1][2] = g113 - g123;
                    B[1][3] = g114 + g124;
                    B[2][0] = g211 + g221;
                    B[2][2] = -g213 + g223;
                    B[3][1] = -g212 + g222;
                    B[3][3] = g214 + g224;

                    // compute T
                    A.invert();
                    F += B.leftmultiply(C.rightmultiply(A));
                    Dune::FieldMatrix<Scalar, 2 * dim, 2 * dim> T(F);

                    // use the pressure values to compute the fluxes
                    Dune::FieldVector<Scalar, 2 * dim> Tu(0);
                    Dune::FieldVector<Scalar, 2 * dim> u(0);
                    u[0] = press1;
                    u[1] = press2;
                    u[2] = press3;
                    u[3] = press4;

                    T.umv(u, Tu);

                    // evaluate velocity of facet 'isIt'
                    FieldVector vector1 = unitOuterNormaln1;
                    vector1 *= Tu[0] / face12vol;
                    this->problem().variables().velocity()[globalIdx1][indexInInside] += vector1;

                    // evaluate velocity of facet 'nextisIt'
                    FieldVector vector3 = unitOuterNormaln3;
                    vector3 *= Tu[2] / face13vol;
                    this->problem().variables().velocity()[globalIdx1][nextindexInInside] += vector3;

                }
                // 'nextisIt' is on the boundary
                else
                {
                    // computation of flux through the first half edge of 'isIt' and the flux
                    // through the second half edge of 'nextisIt'

                    // get common geometry information for the following computation
                    // get the information of the face 'isIt24' between cell2 and cell4 (locally numbered)
                    IntersectionIterator isIt24 = this->problem().gridView().ibegin(*outside);
                    IntersectionIterator innerisItEnd = this->problem().gridView().iend(*outside);
                    for (IntersectionIterator innerisIt = this->problem().gridView().ibegin(*outside); innerisIt
                            != innerisItEnd; ++innerisIt)
                    {
                        if (innerisIt->boundary())
                        {
                            for (int i = 0; i < innerisIt->geometry().corners(); ++i)
                            {
                                GlobalPosition innerisItcorner = innerisIt->geometry().corner(i);

                                if (innerisItcorner == corner1234)
                                {
                                    isIt24 = innerisIt;
                                    continue;
                                }
                            }
                        }
                    }

                    // get geometry type of face 'isIt24'
                    //Dune::GeometryType gtf24 = isIt24->geometryInInside().type();

                    // center of face in global coordinates, i.e., the midpoint of edge 'isIt24'
                    GlobalPosition globalPosFace24 = isIt24->geometry().center();

                    // get face volume
                    Scalar face24vol = isIt24->geometry().volume();

                    // get outer normal vector scaled with half volume of face 'isIt24'
                    GlobalPosition integrationOuterNormaln4 = isIt24->centerUnitOuterNormal();
                    integrationOuterNormaln4 *= face24vol / 2.0;

                    BoundaryTypes nextIsItBcType;
                    this->problem().boundaryTypes(nextIsItBcType, *nextisIt);

                    // get boundary condition for boundary face (isIt24) center
                    BoundaryTypes isIt24BcType;
                    this->problem().boundaryTypes(isIt24BcType, *isIt24);

                    PrimaryVariables boundValues(0.0);

                    // 'nextisIt': Neumann boundary
                    if (nextIsItBcType.isNeumann(pressEqIdx))
                    {
                        // get Neumann boundary value of 'nextisIt'
                        this->problem().neumann(boundValues, *nextisIt);
                        Scalar J3 = (boundValues[wPhaseIdx]/densityW+boundValues[nPhaseIdx]/densityNW);

                        // 'isIt24': Neumann boundary
                        if (isIt24BcType.isNeumann(pressEqIdx))
                        {
                            // get neumann boundary value of 'isIt24'
                            this->problem().neumann(boundValues, *isIt24);
                            Scalar J4 = (boundValues[wPhaseIdx]/densityW+boundValues[nPhaseIdx]/densityNW);

                            // compute normal vectors nu11,nu21; nu12, nu22;
                            FieldVector nu11(0);
                            R.umv(globalPosFace13 - globalPos1, nu11);

                            FieldVector nu21(0);
                            R.umv(globalPos1 - globalPosFace12, nu21);

                            FieldVector nu12(0);
                            R.umv(globalPosFace24 - globalPos2, nu12);

                            FieldVector nu22(0);
                            R.umv(globalPosFace12 - globalPos2, nu22);

                            // compute dF1, dF2 i.e., the area of quadrilateral made by normal vectors 'nu'
                            FieldVector Rnu21(0);
                            R.umv(nu21, Rnu21);
                            Scalar dF1 = fabs(nu11 * Rnu21);

                            FieldVector Rnu22(0);
                            R.umv(nu22, Rnu22);
                            Scalar dF2 = fabs(nu12 * Rnu22);

                            // compute components needed for flux calculation, denoted as 'g'
                            FieldVector K1nu11(0);
                            K1.umv(nu11, K1nu11);
                            FieldVector K1nu21(0);
                            K1.umv(nu21, K1nu21);
                            FieldVector K2nu12(0);
                            K2.umv(nu12, K2nu12);
                            FieldVector K2nu22(0);
                            K2.umv(nu22, K2nu22);
                            Scalar g111 = lambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                            Scalar g121 = lambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                            Scalar g211 = lambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                            Scalar g221 = lambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;
                            Scalar g112 = lambda2 * (integrationOuterNormaln1 * K2nu12) / dF2;
                            Scalar g122 = lambda2 * (integrationOuterNormaln1 * K2nu22) / dF2;
                            Scalar g212 = lambda2 * (integrationOuterNormaln4 * K2nu12) / dF2;
                            Scalar g222 = lambda2 * (integrationOuterNormaln4 * K2nu22) / dF2;

                            // compute the matrix T & vector r in v = A^{-1}(Bu + r1) = Tu + r
                            Dune::FieldMatrix<Scalar, 2 * dim - 1, 2 * dim - 1> A(0);
                            Dune::FieldMatrix<Scalar, 2 * dim - 1, dim> B(0);
                            Dune::FieldVector<Scalar, 2 * dim - 1> r1(0), r(0);

                            // evaluate matrix A, B
                            A[0][0] = g111 + g112;
                            A[0][1] = g121;
                            A[0][2] = -g122;
                            A[1][0] = g211;
                            A[1][1] = g221;
                            A[2][0] = -g212;
                            A[2][2] = g222;

                            B[0][0] = g111 + g121;
                            B[0][1] = g112 - g122;
                            B[1][0] = g211 + g221;
                            B[2][1] = g222 - g212;

                            // evaluate vector r1
                            r1[1] = -J3 * nextisIt->geometry().volume() / 2.0;
                            r1[2] = -J4 * isIt24->geometry().volume() / 2.0;

                            // compute T and r
                            A.invert();
                            B.leftmultiply(A);
                            Dune::FieldMatrix<Scalar, 2 * dim - 1, dim> T(B);
                            A.umv(r1, r);

                            // use the pressure values to compute the fluxes
                            Scalar f1 = (g111 + g121 - g111 * T[0][0] - g121 * T[1][0]) * press1 - (g111 * T[0][1]
                                    + g121 * T[1][1]) * press2 - (g111 * r[0] + g121 * r[1]);

                            // evaluate velocity of facet 'isIt'
                            FieldVector vector1 = unitOuterNormaln1;
                            vector1 *= f1 / face12vol;
                            this->problem().variables().velocity()[globalIdx1][indexInInside] += vector1;

                        }
                        // 'isIt24': Dirichlet boundary
                        else if (isIt24BcType.isDirichlet(pressEqIdx))
                        {
                            // get Dirichlet boundary value on 'isIt24'
                            this->problem().dirichlet(boundValues, *isIt24);
                             Scalar g4 = boundValues[pressureIdx];

                            // compute total mobility for Dirichlet boundary 'isIt24'
                            //determine lambda at the boundary -> if no saturation is known directly at the boundary use the cell saturation
                            Scalar alambda2 = 0;
                            if (isIt24BcType.isDirichlet(satEqIdx))
                            {
                                Scalar satBound = boundValues[saturationIdx];

                                //determine phase saturations from primary saturation variable
                                Scalar satW = 0;
                                switch (this->saturationType)
                                {
                                case Sw:
                                {
                                    satW = satBound;
                                    break;
                                }
                                case Sn:
                                {
                                    satW = 1 - satBound;
                                    break;
                                }
                                }

                                Scalar temperature = this->problem().temperature(*eIt);
                                Scalar referencePressure =  this->problem().referencePressure(*eIt);

                                Scalar lambdaWBound = 0;
                                Scalar lambdaNWBound = 0;

                                FluidState fluidState;
                                fluidState.update(satW, referencePressure, referencePressure, temperature);

                                Scalar viscosityWBound = FluidSystem::phaseViscosity(wPhaseIdx, temperature, referencePressure, fluidState) ;
                                Scalar viscosityNWBound = FluidSystem::phaseViscosity(nPhaseIdx, temperature, referencePressure, fluidState) ;
                                lambdaWBound = MaterialLaw::krw(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityWBound;
                                lambdaNWBound = MaterialLaw::krn(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityNWBound;
                                alambda2 = lambdaWBound + lambdaNWBound;
                            }
                            else
                            {
                                alambda2 = lambda2;
                            }

                            // compute normal vectors nu11,nu21; nu12, nu22;
                            FieldVector nu11(0);
                            R.umv(globalPosFace13 - globalPos1, nu11);

                            FieldVector nu21(0);
                            R.umv(globalPos1 - globalPosFace12, nu21);

                            FieldVector nu12(0);
                            R.umv(globalPosFace24 - globalPos2, nu12);

                            FieldVector nu22(0);
                            R.umv(globalPosFace12 - globalPos2, nu22);

                            // compute dF1, dF2 i.e., the area of quadrilateral made by normal vectors 'nu'
                            FieldVector Rnu21(0);
                            R.umv(nu21, Rnu21);
                            Scalar dF1 = fabs(nu11 * Rnu21);

                            FieldVector Rnu22(0);
                            R.umv(nu22, Rnu22);
                            Scalar dF2 = fabs(nu12 * Rnu22);

                            // compute components needed for flux calculation, denoted as 'g'
                            FieldVector K1nu11(0);
                            K1.umv(nu11, K1nu11);
                            FieldVector K1nu21(0);
                            K1.umv(nu21, K1nu21);
                            FieldVector K2nu12(0);
                            K2.umv(nu12, K2nu12);
                            FieldVector K2nu22(0);
                            K2.umv(nu22, K2nu22);
                            Scalar g111 = lambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                            Scalar g121 = lambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                            Scalar g211 = lambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                            Scalar g221 = lambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;
                            Scalar g112 = alambda2 * (integrationOuterNormaln1 * K2nu12) / dF2;
                            Scalar g122 = alambda2 * (integrationOuterNormaln1 * K2nu22) / dF2;

                            // compute the matrix T & vector r in v = A^{-1}(Bu + r1) = Tu + r
                            FieldMatrix A(0), B(0);
                            FieldVector r1(0), r(0);

                            // evaluate matrix A, B
                            A[0][0] = g111 + g112;
                            A[0][1] = g121;
                            A[1][0] = g211;
                            A[1][1] = g221;

                            B[0][0] = g111 + g121;
                            B[0][1] = g112 - g122;
                            B[1][0] = g211 + g221;

                            // evaluate vector r1
                            r1[0] = g122 * g4;
                            r1[1] = -J3 * nextisIt->geometry().volume() / 2.0;

                            // compute T and r
                            A.invert();
                            B.leftmultiply(A);
                            FieldMatrix T(B);
                            A.umv(r1, r);

                            // use the pressure values to compute the fluxes
                            Scalar f1 = (g111 + g121 - g111 * T[0][0] - g121 * T[1][0]) * press1 - (g111 * T[0][1]
                                    + g121 * T[1][1]) * press2 - (g111 * r[0] + g121 * r[1]);

                            // evaluate velocity of facet 'isIt'
                            FieldVector vector1 = unitOuterNormaln1;
                            vector1 *= f1 / face12vol;
                            this->problem().variables().velocity()[globalIdx1][indexInInside] += vector1;

                        }
                    }
                    // 'nextisIt': Dirichlet boundary
                    else if (nextIsItBcType.isDirichlet(pressEqIdx))
                    {
                        // get Dirichlet boundary value of 'nextisIt'
                        this->problem().dirichlet(boundValues, *nextisIt);
                        Scalar g3 = boundValues[pressureIdx];

                        // compute total mobility for Dirichlet boundary 'nextisIt'
                        //determine lambda at the boundary -> if no saturation is known directly at the boundary use the cell saturation
                        Scalar alambda1 = 0;
                        if (nextIsItBcType.isDirichlet(satEqIdx))
                        {
                            Scalar satBound = boundValues[saturationIdx];

                            //determine phase saturations from primary saturation variable
                            Scalar satW = 0;
                            switch (this->saturationType)
                            {
                            case Sw:
                            {
                                satW = satBound;
                                break;
                            }
                            case Sn:
                            {
                                satW = 1 - satBound;
                                break;
                            }
                            }

                            Scalar temperature = this->problem().temperature(*eIt);
                            Scalar referencePressure =  this->problem().referencePressure(*eIt);

                            Scalar lambdaWBound = 0;
                            Scalar lambdaNWBound = 0;

                            FluidState fluidState;
                            fluidState.update(satW, referencePressure, referencePressure, temperature);

                            Scalar viscosityWBound = FluidSystem::phaseViscosity(wPhaseIdx, temperature, referencePressure, fluidState) ;
                            Scalar viscosityNWBound = FluidSystem::phaseViscosity(nPhaseIdx, temperature, referencePressure, fluidState) ;
                            lambdaWBound = MaterialLaw::krw(
                                    this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                    / viscosityWBound;
                            lambdaNWBound = MaterialLaw::krn(
                                    this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                    / viscosityNWBound;
                            alambda1 = lambdaWBound + lambdaNWBound;
                        }
                        else
                        {
                            alambda1 = lambda1;
                        }

                        // 'isIt24': Neumann boundary
                        if (isIt24BcType.isNeumann(pressEqIdx))
                        {
                            // get Neumann boundary value of 'isIt24'
                            this->problem().neumann(boundValues, *isIt24);
                            Scalar J4 = (boundValues[wPhaseIdx]/densityW+boundValues[nPhaseIdx]/densityNW);

                            // compute normal vectors nu11,nu21; nu12, nu22;
                            FieldVector nu11(0);
                            R.umv(globalPosFace13 - globalPos1, nu11);

                            FieldVector nu21(0);
                            R.umv(globalPos1 - globalPosFace12, nu21);

                            FieldVector nu12(0);
                            R.umv(globalPosFace24 - globalPos2, nu12);

                            FieldVector nu22(0);
                            R.umv(globalPosFace12 - globalPos2, nu22);

                            // compute dF1, dF2 i.e., the area of quadrilateral made by normal vectors 'nu'
                            FieldVector Rnu21(0);
                            R.umv(nu21, Rnu21);
                            Scalar dF1 = fabs(nu11 * Rnu21);

                            FieldVector Rnu22(0);
                            R.umv(nu22, Rnu22);
                            Scalar dF2 = fabs(nu12 * Rnu22);

                            // compute components needed for flux calculation, denoted as 'g'
                            FieldVector K1nu11(0);
                            K1.umv(nu11, K1nu11);
                            FieldVector K1nu21(0);
                            K1.umv(nu21, K1nu21);
                            FieldVector K2nu12(0);
                            K2.umv(nu12, K2nu12);
                            FieldVector K2nu22(0);
                            K2.umv(nu22, K2nu22);
                            Scalar g111 = alambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                            Scalar g121 = alambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                            Scalar g211 = alambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                            Scalar g221 = alambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;
                            Scalar g112 = lambda2 * (integrationOuterNormaln1 * K2nu12) / dF2;
                            Scalar g122 = lambda2 * (integrationOuterNormaln1 * K2nu22) / dF2;
                            Scalar g212 = lambda2 * (integrationOuterNormaln4 * K2nu12) / dF2;
                            Scalar g222 = lambda2 * (integrationOuterNormaln4 * K2nu22) / dF2;

                            // compute the matrix T & vector r in v = A^{-1}(Bu + r1) = Tu + r
                            FieldMatrix A(0), B(0);
                            FieldVector r1(0), r(0);

                            // evaluate matrix A, B
                            A[0][0] = g111 + g112;
                            A[0][1] = -g122;
                            A[1][0] = -g212;
                            A[1][1] = g222;

                            B[0][0] = g111 + g121;
                            B[0][1] = g112 - g122;
                            B[1][1] = g222 - g212;

                            // evaluate vector r1
                            r1[0] = -g121 * g3;
                            r1[1] = -J4 * isIt24->geometry().volume() / 2.0;

                            // compute T and r
                            A.invert();
                            B.leftmultiply(A);
                            FieldMatrix T(B);
                            A.umv(r1, r);

                            // use the pressure values to compute the fluxes
                            Scalar f1 = (g111 + g121 - g111 * T[0][0]) * press1 - g111 * T[0][1] * press2 - g121 * g3
                                    - g111 * r[0];
                            Scalar f3 = (g211 + g221 - g211 * T[0][0]) * press1 - g211 * T[0][1] * press2 - g221 * g3
                                    - g211 * r[0];

                            // evaluate velocity of facet 'isIt'
                            FieldVector vector1 = unitOuterNormaln1;
                            vector1 *= f1 / face12vol;
                            this->problem().variables().velocity()[globalIdx1][indexInInside] += vector1;

                            // evaluate velocity of facet 'nextisIt'
                            FieldVector vector3 = unitOuterNormaln3;
                            vector3 *= f3 / face13vol;
                            this->problem().variables().velocity()[globalIdx1][nextindexInInside] += vector3;

                        }
                        // 'isIt24': Dirichlet boundary
                        else if (isIt24BcType.isDirichlet(pressEqIdx))
                        {
                            // get Dirichlet boundary value on 'isIt24'
                            this->problem().dirichlet(boundValues, *isIt24);
                                Scalar g4 = boundValues[pressureIdx];

                            // compute total mobility for Dirichlet boundary 'isIt24'
                            //determine lambda at the boundary -> if no saturation is known directly at the boundary use the cell saturation
                            Scalar alambda2 = 0;
                            if (isIt24BcType.isDirichlet(satEqIdx))
                            {
                                Scalar satBound = boundValues[saturationIdx];

                                //determine phase saturations from primary saturation variable
                                Scalar satW = 0;
                                switch (this->saturationType)
                                {
                                case Sw:
                                {
                                    satW = satBound;
                                    break;
                                }
                                case Sn:
                                {
                                    satW = 1 - satBound;
                                    break;
                                }
                                }

                                Scalar temperature = this->problem().temperature(*eIt);
                                Scalar referencePressure =  this->problem().referencePressure(*eIt);

                                Scalar lambdaWBound = 0;
                                Scalar lambdaNWBound = 0;

                                FluidState fluidState;
                                fluidState.update(satW, referencePressure, referencePressure, temperature);

                                Scalar viscosityWBound = FluidSystem::phaseViscosity(wPhaseIdx, temperature, referencePressure, fluidState) ;
                                Scalar viscosityNWBound = FluidSystem::phaseViscosity(nPhaseIdx, temperature, referencePressure, fluidState) ;
                                lambdaWBound = MaterialLaw::krw(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityWBound;
                                lambdaNWBound = MaterialLaw::krn(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityNWBound;
                                alambda2 = lambdaWBound + lambdaNWBound;
                            }
                            else
                            {
                                alambda2 = lambda2;
                            }

                            // compute normal vectors nu11,nu21; nu12, nu22;
                            FieldVector nu11(0);
                            R.umv(globalPosFace13 - globalPos1, nu11);

                            FieldVector nu21(0);
                            R.umv(globalPos1 - globalPosFace12, nu21);

                            FieldVector nu12(0);
                            R.umv(globalPosFace24 - globalPos2, nu12);

                            FieldVector nu22(0);
                            R.umv(globalPosFace12 - globalPos2, nu22);

                            // compute dF1, dF2 i.e., the area of quadrilateral made by normal vectors 'nu'
                            FieldVector Rnu21(0);
                            R.umv(nu21, Rnu21);
                            Scalar dF1 = fabs(nu11 * Rnu21);

                            FieldVector Rnu22(0);
                            R.umv(nu22, Rnu22);
                            Scalar dF2 = fabs(nu12 * Rnu22);

                            // compute components needed for flux calculation, denoted as 'g'
                            FieldVector K1nu11(0);
                            K1.umv(nu11, K1nu11);
                            FieldVector K1nu21(0);
                            K1.umv(nu21, K1nu21);
                            FieldVector K2nu12(0);
                            K2.umv(nu12, K2nu12);
                            FieldVector K2nu22(0);
                            K2.umv(nu22, K2nu22);
                            Scalar g111 = alambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                            Scalar g121 = alambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                            Scalar g211 = alambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                            Scalar g221 = alambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;
                            Scalar g112 = alambda2 * (integrationOuterNormaln1 * K2nu12) / dF2;
                            Scalar g122 = alambda2 * (integrationOuterNormaln1 * K2nu22) / dF2;

                            // compute the matrix T & vector r
                            FieldMatrix T(0);
                            FieldVector r(0);

                            Scalar coe = g111 + g112;

                            // evaluate matrix T
                            T[0][0] = g112 * (g111 + g121) / coe;
                            T[0][1] = -g111 * (g112 - g122) / coe;
                            T[1][0] = g221 + g211 * (g112 - g121) / coe;
                            T[1][1] = -g211 * (g112 - g122) / coe;

                            // evaluate vector r
                            r[0] = -(g4 * g122 * g111 + g3 * g112 * g121) / coe;
                            r[1] = -g221 * g3 + (g3 * g211 * g121 - g4 * g211 * g122) / coe;

                            // use the pressure values to compute the fluxes
                            Scalar f1 = T[0][0] * press1 + T[0][1] * press2 + r[0];
                            Scalar f3 = T[1][0] * press1 + T[1][1] * press2 + r[1];

                            // evaluate velocity of facet 'isIt'
                            FieldVector vector1 = unitOuterNormaln1;
                            vector1 *= f1 / face12vol;
                            this->problem().variables().velocity()[globalIdx1][indexInInside] += vector1;

                            // evaluate velocity of facet 'nextisIt'
                            FieldVector vector3 = unitOuterNormaln3;
                            vector3 *= f3 / face13vol;
                            this->problem().variables().velocity()[globalIdx1][nextindexInInside] += vector3;

                        }
                    }
                }
            }
            // handle boundary face 'isIt'
            else
            {
                // get boundary condition for boundary face center of 'isIt'
                BoundaryTypes isItBcType;
                this->problem().boundaryTypes(isItBcType, *isIt);

                PrimaryVariables boundValues(0.0);

                // 'isIt' is on Neumann boundary
                if (isItBcType.isNeumann(pressEqIdx))
                {
                    // get Neumann boundary value
                    this->problem().neumann(boundValues, *isIt);
                    Scalar J1 = (boundValues[wPhaseIdx]/densityW+boundValues[nPhaseIdx]/densityNW);

                    // evaluate velocity of facet 'isIt'
                    FieldVector vector1 = unitOuterNormaln1;
                    vector1 *= -J1;
                    this->problem().variables().velocity()[globalIdx1][indexInInside] -= vector1;

                    // 'nextisIt' is on boundary
                    if (nextisIt->boundary())
                    {
                        // get boundary condition for boundary face center of 'nextisIt'
                        BoundaryTypes nextIsItBcType;
                        this->problem().boundaryTypes(nextIsItBcType, *nextisIt);

                        if (nextIsItBcType.isDirichlet(pressEqIdx))
                        {
                            this->problem().dirichlet(boundValues, *nextisIt);

                            // compute total mobility for Dirichlet boundary 'nextisIt'
                            //determine lambda at the boundary -> if no saturation is known directly at the boundary use the cell saturation
                            Scalar alambda1 = 0;
                            if (nextIsItBcType.isDirichlet(satEqIdx))
                            {
                                Scalar satBound = boundValues[saturationIdx];

                                //determine phase saturations from primary saturation variable
                                Scalar satW = 0;
                                switch (this->saturationType)
                                {
                                case Sw:
                                {
                                    satW = satBound;
                                    break;
                                }
                                case Sn:
                                {
                                    satW = 1 - satBound;
                                    break;
                                }
                                }

                                Scalar temperature = this->problem().temperature(*eIt);
                                Scalar referencePressure =  this->problem().referencePressure(*eIt);

                                Scalar lambdaWBound = 0;
                                Scalar lambdaNWBound = 0;

                                FluidState fluidState;
                                fluidState.update(satW, referencePressure, referencePressure, temperature);

                                Scalar viscosityWBound = FluidSystem::phaseViscosity(wPhaseIdx, temperature, referencePressure, fluidState) ;
                                Scalar viscosityNWBound = FluidSystem::phaseViscosity(nPhaseIdx, temperature, referencePressure, fluidState) ;
                                lambdaWBound = MaterialLaw::krw(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityWBound;
                                lambdaNWBound = MaterialLaw::krn(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityNWBound;
                                alambda1 = lambdaWBound + lambdaNWBound;
                            }
                            else
                            {
                                alambda1 = lambda1;
                            }

                            // get Dirichlet boundary value
                            Scalar g3 = boundValues[pressureIdx];

                            // compute normal vectors nu11,nu21;
                            FieldVector nu11(0);
                            R.umv(globalPosFace13 - globalPos1, nu11);

                            FieldVector nu21(0);
                            R.umv(globalPos1 - globalPosFace12, nu21);

                            // compute dF1, dF2 i.e., the area of quadrilateral made by normal vectors 'nu'
                            FieldVector Rnu21(0);
                            R.umv(nu21, Rnu21);
                            Scalar dF1 = fabs(nu11 * Rnu21);

                            // compute components needed for flux calculation, denoted as 'g'
                            FieldVector K1nu11(0);
                            K1.umv(nu11, K1nu11);
                            FieldVector K1nu21(0);
                            K1.umv(nu21, K1nu21);
                            Scalar g111 = alambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                            Scalar g121 = alambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                            Scalar g211 = alambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                            Scalar g221 = alambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;

                            // use the pressure values to compute the fluxes
                            Scalar f3 = (g221 - g211 * g121 / g111) * press1 + (g211 * g121 / g111 - g221) * g3 - (g211
                                    * (-J1) * face12vol) / (2.0 * g111);

                            // evaluate velocity of facet 'nextisIt'
                            FieldVector vector3 = unitOuterNormaln3;
                            vector3 *= f3 / face13vol;
                            this->problem().variables().velocity()[globalIdx1][nextindexInInside] += vector3;

                        }
                    }
                    // 'nextisIt' is inside
                    else
                    {
                        // neighbor cell 3
                        // access neighbor cell 3
                        ElementPointer nextisItoutside = nextisIt->outside();
                        int globalIdx3 = this->problem().variables().index(*nextisItoutside);

                        // get pressure value
                        Scalar press3 = this->problem().variables().pressure()[globalIdx3];

                        // neighbor cell 3 geometry type
                        //Dune::GeometryType gt3 = nextisItoutside->geometry().type();

                        // get global coordinate of neighbor cell 3 center
                        GlobalPosition globalPos3 = nextisItoutside->geometry().center();

                        // get absolute permeability of neighbor cell 3
                        FieldMatrix K3(this->problem().spatialParameters().intrinsicPermeability(*nextisItoutside));

                        // get total mobility of neighbor cell 3
                        Scalar lambda3 = this->problem().variables().mobilityWetting(globalIdx3)
                                    + this->problem().variables().mobilityNonwetting(globalIdx3);

                        // get the information of the face 'isIt34' between cell3 and cell4 (locally numbered)
                        IntersectionIterator isIt34 = this->problem().gridView().ibegin(*nextisItoutside);
                        IntersectionIterator innernextisItEnd = this->problem().gridView().iend(
                                *nextisItoutside);
                        for (IntersectionIterator innerisIt = this->problem().gridView().ibegin(
                                *nextisItoutside); innerisIt != innernextisItEnd; ++innerisIt)
                        {
                            if (innerisIt->boundary())
                            {
                                for (int i = 0; i < innerisIt->geometry().corners(); ++i)
                                {
                                    GlobalPosition innerisItcorner = innerisIt->geometry().corner(i);

                                    if (innerisItcorner == corner1234)
                                    {
                                        isIt34 = innerisIt;
                                        continue;
                                    }
                                }
                            }
                        }

                        // get geometry type of face 'isIt34'
                        //Dune::GeometryType gtf34 = isIt34->geometryInInside().type();

                        // center of face in global coordinates, i.e., the midpoint of edge 'isIt34'
                        GlobalPosition globalPosFace34 = isIt34->geometry().center();

                        // get face volume
                        Scalar face34vol = isIt34->geometry().volume();

                        // get outer normal vector scaled with half volume of face 'isIt34'
                        GlobalPosition integrationOuterNormaln2 = isIt34->centerUnitOuterNormal();
                        integrationOuterNormaln2 *= face34vol / 2.0;

                        // get boundary condition for boundary face center of 'isIt34'
                        BoundaryTypes isIt34BcType;
                        this->problem().boundaryTypes(isIt34BcType, *isIt34);

                        // 'isIt34': Neumann boundary
                        if (isIt34BcType.isNeumann(pressEqIdx))
                        {
                            // get Neumann boundary value
                            this->problem().neumann(boundValues, *isIt34);
                            Scalar J2 = (boundValues[wPhaseIdx]/densityW+boundValues[nPhaseIdx]/densityNW);

                            // compute normal vectors nu11,nu21; nu13, nu23;
                            FieldVector nu11(0);
                            R.umv(globalPosFace13 - globalPos1, nu11);

                            FieldVector nu21(0);
                            R.umv(globalPos1 - globalPosFace12, nu21);

                            FieldVector nu13(0);
                            R.umv(globalPos3 - globalPosFace13, nu13);

                            FieldVector nu23(0);
                            R.umv(globalPos3 - globalPosFace34, nu23);

                            // compute dF1, dF3 i.e., the area of quadrilateral made by normal vectors 'nu'
                            FieldVector Rnu21(0);
                            R.umv(nu21, Rnu21);
                            Scalar dF1 = fabs(nu11 * Rnu21);

                            FieldVector Rnu23(0);
                            R.umv(nu23, Rnu23);
                            Scalar dF3 = fabs(nu13 * Rnu23);

                            // compute components needed for flux calculation, denoted as 'g'
                            FieldVector K1nu11(0);
                            K1.umv(nu11, K1nu11);
                            FieldVector K1nu21(0);
                            K1.umv(nu21, K1nu21);
                            FieldVector K3nu13(0);
                            K3.umv(nu13, K3nu13);
                            FieldVector K3nu23(0);
                            K3.umv(nu23, K3nu23);
                            Scalar g111 = lambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                            Scalar g121 = lambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                            Scalar g211 = lambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                            Scalar g221 = lambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;
                            Scalar g113 = lambda3 * (integrationOuterNormaln2 * K3nu13) / dF3;
                            Scalar g123 = lambda3 * (integrationOuterNormaln2 * K3nu23) / dF3;
                            Scalar g213 = lambda3 * (integrationOuterNormaln3 * K3nu13) / dF3;
                            Scalar g223 = lambda3 * (integrationOuterNormaln3 * K3nu23) / dF3;

                            // compute transmissibility matrix T = CA^{-1}B+F
                            Dune::FieldMatrix<Scalar, 2 * dim - 1, 2 * dim - 1> C(0), A(0);
                            Dune::FieldMatrix<Scalar, 2 * dim - 1, dim> F(0), B(0);

                            // evaluate matrix C, F, A, B
                            C[0][0] = -g111;
                            C[0][2] = -g121;
                            C[1][1] = -g113;
                            C[1][2] = g123;
                            C[2][1] = -g213;
                            C[2][2] = g223;

                            F[0][0] = g111 + g121;
                            F[1][1] = g113 - g123;
                            F[2][1] = g213 - g223;

                            A[0][0] = g111;
                            A[0][2] = g121;
                            A[1][1] = g113;
                            A[1][2] = -g123;
                            A[2][0] = g211;
                            A[2][1] = -g213;
                            A[2][2] = g223 + g221;

                            B[0][0] = g111 + g121;
                            B[1][1] = g113 - g123;
                            B[2][0] = g211 + g221;
                            B[2][1] = g223 - g213;

                            // compute T
                            A.invert();
                            Dune::FieldMatrix<Scalar, 2 * dim - 1, 2 * dim - 1> CAinv(C.rightmultiply(A));
                            F += B.leftmultiply(CAinv);
                            Dune::FieldMatrix<Scalar, 2 * dim - 1, dim> T(F);

                            // compute vector r
                            // evaluate r1
                            Dune::FieldVector<Scalar, 2 * dim - 1> r1(0);
                            r1[0] = -J1 * face12vol / 2.0;
                            r1[1] = -J2 * isIt34->geometry().volume() / 2.0;

                            // compute r = CA^{-1}r1
                            Dune::FieldVector<Scalar, 2 * dim - 1> r(0);
                            CAinv.umv(r1, r);

                            // use the pressure values to compute the fluxes
                            Scalar f3 = T[2][0] * press1 + T[2][1] * press3 + r[2];

                            // evaluate velocity of facet 'nextisIt'
                            FieldVector vector3 = unitOuterNormaln3;
                            vector3 *= f3 / face13vol;
                            this->problem().variables().velocity()[globalIdx1][nextindexInInside] += vector3;

                        }
                        // 'isIt34': Dirichlet boundary
                        else if (isIt34BcType.isDirichlet(pressEqIdx))
                        {
                            // get Dirichlet boundary value
                            this->problem().dirichlet(boundValues, *isIt34);
                            Scalar g2 = boundValues[pressureIdx];

                            // compute total mobility for Dirichlet boundary 'isIt24'
                            //determine lambda at the boundary -> if no saturation is known directly at the boundary use the cell saturation
                            Scalar alambda3 = 0;
                            if (isIt34BcType.isDirichlet(satEqIdx))
                            {
                                Scalar satBound = boundValues[saturationIdx];

                                //determine phase saturations from primary saturation variable
                                Scalar satW = 0;
                                switch (this->saturationType)
                                {
                                case Sw:
                                {
                                    satW = satBound;
                                    break;
                                }
                                case Sn:
                                {
                                    satW = 1 - satBound;
                                    break;
                                }
                                }

                                Scalar temperature = this->problem().temperature(*eIt);
                                Scalar referencePressure =  this->problem().referencePressure(*eIt);

                                Scalar lambdaWBound = 0;
                                Scalar lambdaNWBound = 0;

                                FluidState fluidState;
                                fluidState.update(satW, referencePressure, referencePressure, temperature);

                                Scalar viscosityWBound = FluidSystem::phaseViscosity(wPhaseIdx, temperature, referencePressure, fluidState) ;
                                Scalar viscosityNWBound = FluidSystem::phaseViscosity(nPhaseIdx, temperature, referencePressure, fluidState) ;
                                lambdaWBound = MaterialLaw::krw(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityWBound;
                                lambdaNWBound = MaterialLaw::krn(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityNWBound;
                                alambda3 = lambdaWBound + lambdaNWBound;
                            }
                            else
                            {
                                alambda3 = lambda3;
                            }

                            // compute normal vectors nu11,nu21; nu13, nu23;
                            FieldVector nu11(0);
                            R.umv(globalPosFace13 - globalPos1, nu11);

                            FieldVector nu21(0);
                            R.umv(globalPos1 - globalPosFace12, nu21);

                            FieldVector nu13(0);
                            R.umv(globalPos3 - globalPosFace13, nu13);

                            FieldVector nu23(0);
                            R.umv(globalPos3 - globalPosFace34, nu23);

                            // compute dF1, dF3 i.e., the area of quadrilateral made by normal vectors 'nu'
                            FieldVector Rnu21(0);
                            R.umv(nu21, Rnu21);
                            Scalar dF1 = fabs(nu11 * Rnu21);

                            FieldVector Rnu23(0);
                            R.umv(nu23, Rnu23);
                            Scalar dF3 = fabs(nu13 * Rnu23);

                            // compute components needed for flux calculation, denoted as 'g'
                            FieldVector K1nu11(0);
                            K1.umv(nu11, K1nu11);
                            FieldVector K1nu21(0);
                            K1.umv(nu21, K1nu21);
                            FieldVector K3nu13(0);
                            K3.umv(nu13, K3nu13);
                            FieldVector K3nu23(0);
                            K3.umv(nu23, K3nu23);
                            Scalar g111 = lambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                            Scalar g121 = lambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                            Scalar g211 = lambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                            Scalar g221 = lambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;
                            Scalar g213 = alambda3 * (integrationOuterNormaln3 * K3nu13) / dF3;
                            Scalar g223 = alambda3 * (integrationOuterNormaln3 * K3nu23) / dF3;

                            // compute transmissibility matrix T = CA^{-1}B+F
                            FieldMatrix C(0), A(0), F(0), B(0);

                            // evaluate matrix C, F, A, B
                            C[0][0] = -g111;
                            C[0][1] = -g121;
                            C[1][1] = g223;

                            F[0][0] = g111 + g121;
                            F[1][1] = g213 - g223;

                            A[0][0] = g111;
                            A[0][1] = g121;
                            A[1][0] = g211;
                            A[1][1] = g223 + g221;

                            B[0][0] = g111 + g121;
                            B[1][0] = g211 + g221;
                            B[1][1] = g223 - g213;

                            // compute T
                            A.invert();
                            FieldMatrix CAinv(C.rightmultiply(A));
                            F += B.leftmultiply(CAinv);
                            FieldMatrix T(F);

                            // compute vector r
                            // evaluate r1, r2
                            FieldVector r1(0), r2(0);
                            r1[1] = -g213 * g2;
                            r2[0] = -J1 * face12vol / 2.0;
                            r2[1] = g213 * g2;

                            // compute r = CA^{-1}r1
                            FieldVector r(0);
                            CAinv.umv(r2, r);
                            r += r1;

                            // use the pressure values to compute the fluxes
                            Scalar f3 = T[1][0] * press1 + T[1][1] * press3 + r[1];

                            // evaluate velocity of facet 'nextisIt'
                            FieldVector vector3 = unitOuterNormaln3;
                            vector3 *= f3 / face13vol;
                            this->problem().variables().velocity()[globalIdx1][nextindexInInside] += vector3;

                        }
                    }
                }
                // 'isIt' is on Dirichlet boundary
                else if (isItBcType.isDirichlet(pressEqIdx))
                {
                    // get Dirichlet boundary value
                    this->problem().dirichlet(boundValues, *isIt);
                    Scalar g1 = boundValues[pressureIdx];

                    // compute total mobility for Dirichlet boundary 'isIt'
                    //determine lambda at the boundary -> if no saturation is known directly at the boundary use the cell saturation
                    Scalar alambda1 = 0;
                    if (isItBcType.isDirichlet(satEqIdx))
                    {
                        Scalar satBound = boundValues[saturationIdx];

                        //determine phase saturations from primary saturation variable
                        Scalar satW = 0;
                        switch (this->saturationType)
                        {
                        case Sw:
                        {
                            satW = satBound;
                            break;
                        }
                        case Sn:
                        {
                            satW = 1 - satBound;
                            break;
                        }
                        }

                        Scalar temperature = this->problem().temperature(*eIt);
                        Scalar referencePressure =  this->problem().referencePressure(*eIt);

                        Scalar lambdaWBound = 0;
                        Scalar lambdaNWBound = 0;

                        FluidState fluidState;
                        fluidState.update(satW, referencePressure, referencePressure, temperature);

                        Scalar viscosityWBound = FluidSystem::phaseViscosity(wPhaseIdx, temperature, referencePressure, fluidState) ;
                        Scalar viscosityNWBound = FluidSystem::phaseViscosity(nPhaseIdx, temperature, referencePressure, fluidState) ;
                        lambdaWBound = MaterialLaw::krw(
                                this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                / viscosityWBound;
                        lambdaNWBound = MaterialLaw::krn(
                                this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                / viscosityNWBound;
                        alambda1 = lambdaWBound + lambdaNWBound;
                    }
                    else
                    {
                        alambda1 = lambda1;
                    }

                    // 'nextisIt' is on boundary
                    if (nextisIt->boundary())
                    {
                        // get boundary condition for boundary face (nextisIt) center
                        BoundaryTypes nextIsItBcType;
                        this->problem().boundaryTypes(nextIsItBcType, *nextisIt);

                        // 'nextisIt': Dirichlet boundary
                        if (nextIsItBcType.isDirichlet(pressEqIdx))
                        {
                            // get Dirichlet boundary value of 'nextisIt'
                            this->problem().dirichlet(boundValues, *nextisIt);
                            Scalar g3 = boundValues[pressureIdx];

                            // compute total mobility for Dirichlet boundary 'nextisIt'
                            //determine lambda at the boundary -> if no saturation is known directly at the boundary use the cell saturation
                            Scalar alambda1 = 0;
                            if (nextIsItBcType.isDirichlet(satEqIdx))
                            {
                                Scalar satBound = boundValues[saturationIdx];

                                //determine phase saturations from primary saturation variable
                                Scalar satW = 0;
                                switch (this->saturationType)
                                {
                                case Sw:
                                {
                                    satW = satBound;
                                    break;
                                }
                                case Sn:
                                {
                                    satW = 1 - satBound;
                                    break;
                                }
                                }

                                Scalar temperature = this->problem().temperature(*eIt);
                                Scalar referencePressure =  this->problem().referencePressure(*eIt);

                                Scalar lambdaWBound = 0;
                                Scalar lambdaNWBound = 0;

                                FluidState fluidState;
                                fluidState.update(satW, referencePressure, referencePressure, temperature);

                                Scalar viscosityWBound = FluidSystem::phaseViscosity(wPhaseIdx, temperature, referencePressure, fluidState) ;
                                Scalar viscosityNWBound = FluidSystem::phaseViscosity(nPhaseIdx, temperature, referencePressure, fluidState) ;
                                lambdaWBound = MaterialLaw::krw(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityWBound;
                                lambdaNWBound = MaterialLaw::krn(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityNWBound;
                                alambda1 = lambdaWBound + lambdaNWBound;
                            }
                            else
                            {
                                alambda1 = lambda1;
                            }

                            // compute normal vectors nu11,nu21;
                            FieldVector nu11(0);
                            R.umv(globalPosFace13 - globalPos1, nu11);

                            FieldVector nu21(0);
                            R.umv(globalPos1 - globalPosFace12, nu21);

                            // compute dF1 i.e., the area of quadrilateral made by normal vectors 'nu'
                            FieldVector Rnu21(0);
                            R.umv(nu21, Rnu21);
                            Scalar dF1 = fabs(nu11 * Rnu21);

                            // compute components needed for flux calculation, denoted as 'g'
                            FieldVector K1nu11(0);
                            K1.umv(nu11, K1nu11);
                            FieldVector K1nu21(0);
                            K1.umv(nu21, K1nu21);
                            Scalar g111 = alambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                            Scalar g121 = alambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                            Scalar g211 = alambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                            Scalar g221 = alambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;

                            // evaluate T1, T3, r1, r3
                            Scalar T1 = g111 + g121;
                            Scalar T3 = g211 + g221;
                            Scalar r1 = g111 * g1 + g121 * g3;
                            Scalar r3 = g211 * g1 + g221 * g3;

                            // use the pressure values to compute the fluxes
                            Scalar f1 = T1 * press1 - r1;
                            Scalar f3 = T3 * press1 - r3;

                            // evaluate velocity of facet 'isIt'
                            FieldVector vector1 = unitOuterNormaln1;
                            vector1 *= f1 / face12vol;
                            this->problem().variables().velocity()[globalIdx1][indexInInside] += vector1;

                            // evaluate velocity of facet 'nextisIt'
                            FieldVector vector3 = unitOuterNormaln3;
                            vector3 *= f3 / face13vol;
                            this->problem().variables().velocity()[globalIdx1][nextindexInInside] += vector3;

                        }
                        // 'nextisIt': Neumann boundary
                        else if (nextIsItBcType.isNeumann(pressEqIdx))
                        {
                            // get Neumann boundary value of 'nextisIt'
                            this->problem().neumann(boundValues, *nextisIt);
                            Scalar J3 = (boundValues[wPhaseIdx]/densityW+boundValues[nPhaseIdx]/densityNW);

                            // compute normal vectors nu11,nu21;
                            FieldVector nu11(0);
                            R.umv(globalPosFace13 - globalPos1, nu11);

                            FieldVector nu21(0);
                            R.umv(globalPos1 - globalPosFace12, nu21);

                            // compute dF1 i.e., the area of quadrilateral made by normal vectors 'nu'
                            FieldVector Rnu21(0);
                            R.umv(nu21, Rnu21);
                            Scalar dF1 = fabs(nu11 * Rnu21);

                            // compute components needed for flux calculation, denoted as 'g'
                            FieldVector K1nu11(0);
                            K1.umv(nu11, K1nu11);
                            FieldVector K1nu21(0);
                            K1.umv(nu21, K1nu21);
                            Scalar g111 = alambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                            Scalar g121 = alambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                            Scalar g211 = alambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                            Scalar g221 = alambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;

                            // evaluate T, r
                            Scalar T = g111 - g211 * g121 / g221;
                            Scalar r = -T * g1 - g121 * (-J3) * nextisIt->geometry().volume() / (2.0 * g221);

                            // use the pressure values to compute the fluxes
                            Scalar f1 = T * press1 + r;

                            // evaluate velocity of facet 'isIt'
                            FieldVector vector1 = unitOuterNormaln1;
                            vector1 *= f1 / face12vol;
                            this->problem().variables().velocity()[globalIdx1][indexInInside] += vector1;

                        }
                    }
                    // 'nextisIt' is inside
                    else
                    {
                        // neighbor cell 3
                        // access neighbor cell 3
                        ElementPointer nextisItoutside = nextisIt->outside();
                        int globalIdx3 = this->problem().variables().index(*nextisItoutside);

                        // get pressure value
                        Scalar press3 = this->problem().variables().pressure()[globalIdx3];

                        // neighbor cell 3 geometry type
                        //Dune::GeometryType gt3 = nextisItoutside->geometry().type();

                        // get global coordinate of neighbor cell 3 center
                        GlobalPosition globalPos3 = nextisItoutside->geometry().center();

                        // get absolute permeability of neighbor cell 3
                        FieldMatrix K3(this->problem().spatialParameters().intrinsicPermeability(*nextisItoutside));

                        // get total mobility of neighbor cell 3
                        Scalar lambda3 = this->problem().variables().mobilityWetting(globalIdx3)
                                    + this->problem().variables().mobilityNonwetting(globalIdx3);

                        // get the information of the face 'isIt34' between cell3 and cell4 (locally numbered)
                        IntersectionIterator isIt34 = this->problem().gridView().ibegin(*nextisItoutside);
                        IntersectionIterator innernextisItEnd = this->problem().gridView().iend(
                                *nextisItoutside);
                        for (IntersectionIterator innerisIt = this->problem().gridView().ibegin(
                                *nextisItoutside); innerisIt != innernextisItEnd; ++innerisIt)
                        {
                            if (innerisIt->boundary())
                            {
                                for (int i = 0; i < innerisIt->geometry().corners(); ++i)
                                {
                                    GlobalPosition innerisItcorner = innerisIt->geometry().corner(i);

                                    if (innerisItcorner == corner1234)
                                    {
                                        isIt34 = innerisIt;
                                        continue;
                                    }
                                }
                            }
                        }

                        // get geometry type of face 'isIt34'
                        //Dune::GeometryType gtf34 = isIt34->geometryInInside().type();

                        // center of face in global coordinates, i.e., the midpoint of edge 'isIt34'
                        GlobalPosition globalPosFace34 = isIt34->geometry().center();

                        // get face volume
                        Scalar face34vol = isIt34->geometry().volume();

                        // get outer normal vector scaled with half volume of face 'isIt34'
                        GlobalPosition integrationOuterNormaln2 = isIt34->centerUnitOuterNormal();
                        integrationOuterNormaln2 *= face34vol / 2.0;

                        // get boundary condition for boundary face (isIt34) center
                        BoundaryTypes isIt34BcType;
                        this->problem().boundaryTypes(isIt34BcType, *isIt34);

                        // 'isIt34': Dirichlet boundary
                        if (isIt34BcType.isDirichlet(pressEqIdx))
                        {
                            // get Dirichlet boundary value of 'isIt34'
                            this->problem().dirichlet(boundValues, *isIt34);
                            Scalar g2 = boundValues[pressureIdx];

                            // compute total mobility for Dirichlet boundary 'isIt34'
                            //determine lambda at the boundary -> if no saturation is known directly at the boundary use the cell saturation
                            Scalar alambda3 = 0;
                            if (isIt34BcType.isDirichlet(satEqIdx))
                            {
                                Scalar satBound = boundValues[saturationIdx];

                                //determine phase saturations from primary saturation variable
                                Scalar satW = 0;
                                switch (this->saturationType)
                                {
                                case Sw:
                                {
                                    satW = satBound;
                                    break;
                                }
                                case Sn:
                                {
                                    satW = 1 - satBound;
                                    break;
                                }
                                }

                                Scalar temperature = this->problem().temperature(*eIt);
                                Scalar referencePressure =  this->problem().referencePressure(*eIt);

                                Scalar lambdaWBound = 0;
                                Scalar lambdaNWBound = 0;

                                FluidState fluidState;
                                fluidState.update(satW, referencePressure, referencePressure, temperature);

                                Scalar viscosityWBound = FluidSystem::phaseViscosity(wPhaseIdx, temperature, referencePressure, fluidState) ;
                                Scalar viscosityNWBound = FluidSystem::phaseViscosity(nPhaseIdx, temperature, referencePressure, fluidState) ;
                                lambdaWBound = MaterialLaw::krw(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityWBound;
                                lambdaNWBound = MaterialLaw::krn(
                                        this->problem().spatialParameters().materialLawParams(*eIt), satW)
                                        / viscosityNWBound;
                                alambda3 = lambdaWBound + lambdaNWBound;
                            }
                            else
                            {
                                alambda3 = lambda3;
                            }

                            // compute normal vectors nu11,nu21; nu13, nu23;
                            FieldVector nu11(0);
                            R.umv(globalPosFace13 - globalPos1, nu11);

                            FieldVector nu21(0);
                            R.umv(globalPos1 - globalPosFace12, nu21);

                            FieldVector nu13(0);
                            R.umv(globalPos3 - globalPosFace13, nu13);

                            FieldVector nu23(0);
                            R.umv(globalPos3 - globalPosFace34, nu23);

                            // compute dF1, dF3 i.e., the area of quadrilateral made by normal vectors 'nu'
                            FieldVector Rnu21(0);
                            R.umv(nu21, Rnu21);
                            Scalar dF1 = fabs(nu11 * Rnu21);

                            FieldVector Rnu23(0);
                            R.umv(nu23, Rnu23);
                            Scalar dF3 = fabs(nu13 * Rnu23);

                            // compute components needed for flux calculation, denoted as 'g'
                            FieldVector K1nu11(0);
                            K1.umv(nu11, K1nu11);
                            FieldVector K1nu21(0);
                            K1.umv(nu21, K1nu21);
                            FieldVector K3nu13(0);
                            K3.umv(nu13, K3nu13);
                            FieldVector K3nu23(0);
                            K3.umv(nu23, K3nu23);
                            Scalar g111 = alambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                            Scalar g121 = alambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                            Scalar g211 = alambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                            Scalar g221 = alambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;
                            Scalar g213 = alambda3 * (integrationOuterNormaln3 * K3nu13) / dF3;
                            Scalar g223 = alambda3 * (integrationOuterNormaln3 * K3nu23) / dF3;

                            // compute the matrix T & vector r
                            FieldMatrix T(0);
                            FieldVector r(0);

                            Scalar coe = g221 + g223;

                            // evaluate matrix T
                            T[0][0] = g111 + g121 * (g223 - g211) / coe;
                            T[0][1] = -g121 * (g223 - g213) / coe;
                            T[1][0] = g223 * (g211 + g221) / coe;
                            T[1][1] = -g221 * (g223 - g213) / coe;

                            // evaluate vector r
                            r[0] = -g111 * g1 + (g1 * g121 * g211 - g2 * g213 * g121) / coe;
                            r[1] = -(g1 * g211 * g223 + g2 * g221 * g213) / coe;

                            // use the pressure values to compute the fluxes
                            Scalar f1 = T[0][0] * press1 + T[0][1] * press3 + r[0];
                            Scalar f3 = T[1][0] * press1 + T[1][1] * press3 + r[1];

                            // evaluate velocity of facet 'isIt'
                            FieldVector vector1 = unitOuterNormaln1;
                            vector1 *= f1 / face12vol;
                            this->problem().variables().velocity()[globalIdx1][indexInInside] += vector1;

                            // evaluate velocity of facet 'nextisIt'
                            FieldVector vector3 = unitOuterNormaln3;
                            vector3 *= f3 / face13vol;
                            this->problem().variables().velocity()[globalIdx1][nextindexInInside] += vector3;

                        }
                        // 'isIt34': Neumann boundary
                        else if (isIt34BcType.isNeumann(pressEqIdx))
                        {
                            // get Neumann boundary value of 'isIt34'
                            this->problem().neumann(boundValues, *isIt34);
                            Scalar J2 = (boundValues[wPhaseIdx]/densityW+boundValues[nPhaseIdx]/densityNW);

                            // compute normal vectors nu11,nu21; nu13, nu23;
                            FieldVector nu11(0);
                            R.umv(globalPosFace13 - globalPos1, nu11);

                            FieldVector nu21(0);
                            R.umv(globalPos1 - globalPosFace12, nu21);

                            FieldVector nu13(0);
                            R.umv(globalPos3 - globalPosFace13, nu13);

                            FieldVector nu23(0);
                            R.umv(globalPos3 - globalPosFace34, nu23);

                            // compute dF1, dF3 i.e., the area of quadrilateral made by normal vectors 'nu'
                            FieldVector Rnu21(0);
                            R.umv(nu21, Rnu21);
                            Scalar dF1 = fabs(nu11 * Rnu21);

                            FieldVector Rnu23(0);
                            R.umv(nu23, Rnu23);
                            Scalar dF3 = fabs(nu13 * Rnu23);

                            // compute components needed for flux calculation, denoted as 'g'
                            FieldVector K1nu11(0);
                            K1.umv(nu11, K1nu11);
                            FieldVector K1nu21(0);
                            K1.umv(nu21, K1nu21);
                            FieldVector K3nu13(0);
                            K3.umv(nu13, K3nu13);
                            FieldVector K3nu23(0);
                            K3.umv(nu23, K3nu23);
                            Scalar g111 = alambda1 * (integrationOuterNormaln1 * K1nu11) / dF1;
                            Scalar g121 = alambda1 * (integrationOuterNormaln1 * K1nu21) / dF1;
                            Scalar g211 = alambda1 * (integrationOuterNormaln3 * K1nu11) / dF1;
                            Scalar g221 = alambda1 * (integrationOuterNormaln3 * K1nu21) / dF1;
                            Scalar g113 = lambda3 * (integrationOuterNormaln2 * K3nu13) / dF3;
                            Scalar g123 = lambda3 * (integrationOuterNormaln2 * K3nu23) / dF3;
                            Scalar g213 = lambda3 * (integrationOuterNormaln3 * K3nu13) / dF3;
                            Scalar g223 = lambda3 * (integrationOuterNormaln3 * K3nu23) / dF3;

                            // compute the matrix T & vector r in v = A^{-1}(Bu + r1) = Tu + r
                            FieldMatrix A(0), B(0);
                            FieldVector r1(0), r(0);

                            // evaluate matrix A, B
                            A[0][0] = g113;
                            A[0][1] = -g123;
                            A[1][0] = -g213;
                            A[1][1] = g221 + g223;

                            B[0][1] = g113 - g123;
                            B[1][0] = g211 + g221;
                            B[1][1] = g223 - g213;

                            // evaluate vector r1
                            r1[0] = -J2 * isIt34->geometry().volume() / 2.0;
                            r1[1] = -g211 * g1;

                            // compute T and r
                            A.invert();
                            B.leftmultiply(A);
                            FieldMatrix T(B);
                            A.umv(r1, r);

                            // use the pressure values to compute the fluxes
                            Scalar f1 = (g111 + g121 - g121 * T[1][0]) * press1 - g121 * T[1][1] * press3 - (g111 * g1
                                    + g121 * r[1]);
                            Scalar f3 = (g211 + g221 - g221 * T[1][0]) * press1 - g221 * T[1][1] * press3 - (g211 * g1
                                    + g221 * r[1]);

                            // evaluate velocity of facet 'isIt'
                            FieldVector vector1 = unitOuterNormaln1;
                            vector1 *= f1 / face12vol;
                            this->problem().variables().velocity()[globalIdx1][indexInInside] += vector1;

                            // evaluate velocity of facet 'nextisIt'
                            FieldVector vector3 = unitOuterNormaln3;
                            vector3 *= f3 / face13vol;
                            this->problem().variables().velocity()[globalIdx1][nextindexInInside] += vector3;

                        }
                    }
                }
            }
        } // end all intersections

        for (int i = 0; i < this->problem().variables().velocity()[globalIdx1].N(); i++)
        {
            this->problem().variables().potentialWetting(globalIdx1, i) = this->problem().variables().velocity()[globalIdx1][i] * unitOuterNormal[i];
            this->problem().variables().potentialNonwetting(globalIdx1, i) = this->problem().variables().velocity()[globalIdx1][i] * unitOuterNormal[i];
//            std::cout<<"potentialW = "<<this->problem().variables().potentialWetting(globalIdx1, i)
//                    <<" ,potentialNW = "<<this->problem().variables().potentialNonwetting(globalIdx1, i)<<"\n";
        }

        // check if local mass conservative
        if (dim == 2 && GET_PROP_VALUE(TypeTag, PTAG(VelocityFormulation)) == vt)
        {
            Scalar diff = fabs(this->problem().variables().velocity()[globalIdx1][0] * unitOuterNormal[0] * facevol[0]
                    + this->problem().variables().velocity()[globalIdx1][1] * unitOuterNormal[1] * facevol[1]
                    + this->problem().variables().velocity()[globalIdx1][2] * unitOuterNormal[2] * facevol[2]
                    + this->problem().variables().velocity()[globalIdx1][3] * unitOuterNormal[3] * facevol[3] - q1
                    * volume1) / (fabs(this->problem().variables().velocity()[globalIdx1][0] * unitOuterNormal[0]
                    * facevol[0]) + fabs(this->problem().variables().velocity()[globalIdx1][1] * unitOuterNormal[1]
                    * facevol[1]) + fabs(this->problem().variables().velocity()[globalIdx1][2] * unitOuterNormal[2]
                    * facevol[2]) + fabs(this->problem().variables().velocity()[globalIdx1][3] * unitOuterNormal[3]
                    * facevol[3]) + fabs(q1 * volume1));

            // without source/sink
            if (diff > 1e-8)
            {
                std::cout << "NOT conservative!!! diff = " << diff << ", globalIdxI = " << globalIdx1 << std::endl;
                std::cout << this->problem().variables().velocity()[globalIdx1][0] * unitOuterNormal[0] * facevol[0]
                        << ", " << this->problem().variables().velocity()[globalIdx1][1] * unitOuterNormal[1]
                        * facevol[1] << ", " << this->problem().variables().velocity()[globalIdx1][2]
                        * unitOuterNormal[2] * facevol[2] << ", "
                        << this->problem().variables().velocity()[globalIdx1][3] * unitOuterNormal[3] * facevol[3]
                        << std::endl;
            }
        }
    } // end grid traversal
//    printvector(std::cout, this->problem().variables().velocity(), "velocity", "row", 4, 1, 3);
    return;
} // end method calcTotalVelocity

} // end of Dune namespace
#endif
