/*
  Copyright (C) 2014 by Andreas Lauser

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/
/*!
 * \file
 *
 * \copydoc Ewoms::BlackOilProblem
 */
#ifndef EWOMS_BLACKOIL_PROBLEM_HH
#define EWOMS_BLACKOIL_PROBLEM_HH

#include "blackoilproperties.hh"

#include <ewoms/disc/common/fvbaseproblem.hh>
#include <ewoms/io/eclipsewriter.hh>

namespace Ewoms {

/*!
 * \ingroup Discretization
 *
 * \brief Base class for all problems which use the black-oil model.
 */
template<class TypeTag>
class BlackOilProblem : public FvBaseProblem<TypeTag>
{
private:
    typedef FvBaseProblem<TypeTag> ParentType;
    typedef typename GET_PROP_TYPE(TypeTag, Problem) Implementation;

public:
    /*!
     * \copydoc Doxygen::defaultProblemConstructor
     *
     * \param simulator The time manager of the simulation
     * \param gridView The view on the DUNE grid which ought to be
     *                 used (normally the leaf grid view)
     */
    BlackOilProblem(Simulator &simulator)
        : ParentType(simulator)
    {
    }

    /*!
     * \brief Registers all available parameters for the problem and
     *        the model.
     */
    static void registerParameters()
    {
        Model::registerParameters();
        EWOMS_REGISTER_PARAM(TypeTag, bool, EnableEclipseOutput,
                             "Write binary output which is compatible with the commercial Eclipse simulator");
    }

    /*!
     * \brief Called by the Ewoms::Simulator in order to
     *        initialize the problem.
     *
     * If you overload this method don't forget to call
     * ParentType::init()
     */
    void init()
    {
        // set the initial condition of the model
        model().init();

        assembleTime_ = 0.0;
        solveTime_ = 0.0;
        updateTime_ = 0.0;
    }


    /*!
     * \brief Called after the simulation has been finished
     *        sucessfully.
     */
    void finalize()
    {
        if (gridView().comm().rank() == 0) {
            Scalar totalTime = std::max(1e-100, assembleTime_ + solveTime_ + updateTime_);
            int numCores = this->gridView().comm().size();
            std::cout << "Simulation of problem '" << asImp_().name() << "' finished.\n"
                      << "Timing receipt [s] (solve total/assemble/linear solve/update): "
                      << totalTime  << " (" << totalTime*numCores
                      << " cummulative, " << numCores <<" processes) / "
                      << assembleTime_  << " (" << assembleTime_/totalTime*100 << "%) / "
                      << solveTime_ << " (" << solveTime_/totalTime*100 << "%) / "
                      << updateTime_ << " (" << updateTime_/totalTime*100 << "%)"
                      << "\n" << std::flush;
        }
    }

    /*!
     * \brief Returns the total wall time spend on solving the
     *        system [s].
     */
    Scalar solveTime() const
    { return solveTime_; }

    /*!
     * \brief Returns the total wall time spend on updating the
     *        iterative solutions [s].
     */
    Scalar updateTime() const
    { return updateTime_; }

    /*!
     * \brief Evaluate the boundary conditions for a boundary segment.
     *
     * \param values Stores the fluxes over the boundary segment.
     * \param context The object representing the execution context from
     *                which this method is called.
     * \param spaceIdx The local index of the spatial entity which represents the boundary segment.
     * \param timeIdx The index used for the time discretization
     */
    template <class Context>
    void boundary(BoundaryRateVector &values,
                  const Context &context,
                  int spaceIdx, int timeIdx) const
    { OPM_THROW(std::logic_error, "Problem does not provide a boundary() method"); }

    /*!
     * \brief Evaluate the constraints for a control volume.
     *
     * \param constraints Stores the values of the primary variables at a
     *                    given spatial and temporal location.
     * \param context The object representing the execution context from
     *                which this method is called.
     * \param spaceIdx The local index of the spatial entity which represents the boundary segment.
     * \param timeIdx The index used for the time discretization
     */
    template <class Context>
    void constraints(Constraints &constraints,
                     const Context &context,
                     int spaceIdx, int timeIdx) const
    { OPM_THROW(std::logic_error, "Problem does not provide a constraints() method"); }

    /*!
     * \brief Evaluate the source term for all phases within a given
     *        sub-control-volume.
     *
     * \param rate Stores the values of the volumetric creation/anihilition
     *             rates of the conserved quantities.
     * \param context The object representing the execution context from which
     *                this method is called.
     * \param spaceIdx The local index of the spatial entity which represents
     *                 the boundary segment.
     * \param timeIdx The index used for the time discretization
     */
    template <class Context>
    void source(RateVector &rate,
                const Context &context,
                int spaceIdx, int timeIdx) const
    { OPM_THROW(std::logic_error, "Problem does not provide a source() method"); }

    /*!
     * \brief Evaluate the initial value for a control volume.
     *
     * \param values Stores the primary variables.
     * \param context The object representing the execution context from which
     *                this method is called.
     * \param spaceIdx The local index of the spatial entity which represents
     *                 the boundary segment.
     * \param timeIdx The index used for the time discretization
     */
    template <class Context>
    void initial(PrimaryVariables &values,
                 const Context &context,
                 int spaceIdx, int timeIdx) const
    { OPM_THROW(std::logic_error, "Problem does not provide a initial() method"); }

    /*!
     * \brief Return how much the domain is extruded at a given sub-control volume.
     *
     * This means the factor by which a lower-dimensional (1D or 2D)
     * entity needs to be expanded to get a full dimensional cell. The
     * default is 1.0 which means that 1D problems are actually
     * thought as pipes with a cross section of 1 m^2 and 2D problems
     * are assumed to extend 1 m to the back.
     *
     * \param context The object representing the execution context from which
     *                this method is called.
     * \param spaceIdx The local index of the spatial entity which represents
     *                 the boundary segment.
     * \param timeIdx The index used for the time discretization
     */
    template <class Context>
    Scalar extrusionFactor(const Context &context,
                           int spaceIdx, int timeIdx) const
    { return asImp_().extrusionFactor(); }

    Scalar extrusionFactor() const
    { return 1.0; }

    /*!
     * \name Simulation steering
     */
    // \{

    /*!
     * \brief Called by the simulator before the time integration.
     */
    void preTimeStep()
    {}

    /*!
     * \brief Called by Ewoms::Simulator in order to do a time
     *        integration on the model.
     */
    void timeIntegration()
    {
        int maxFails = EWOMS_GET_PARAM(TypeTag, unsigned, MaxTimeStepDivisions);
        Scalar minTimeStepSize = EWOMS_GET_PARAM(TypeTag, Scalar, MinTimeStepSize);

        // if the time step size of the simulator is smaller than
        // the specified minimum size and we're not going to finish
        // the simulation or an episode, try with the minimum size.
        if (simulator().timeStepSize() < minTimeStepSize &&
            !simulator().episodeWillBeOver() &&
            !simulator().willBeFinished())
        {
            simulator().setTimeStepSize(minTimeStepSize);
        }

        for (int i = 0; i < maxFails; ++i) {
            bool converged = model().update(newtonMethod());
            if (converged) {
                assembleTime_ += newtonMethod().assembleTime();
                solveTime_ += newtonMethod().solveTime();
                updateTime_ += newtonMethod().updateTime();

                return;
            }

            assembleTime_ += newtonMethod().assembleTime();
            solveTime_ += newtonMethod().solveTime();
            updateTime_ += newtonMethod().updateTime();

            Scalar dt = simulator().timeStepSize();
            Scalar nextDt = dt / 2;
            if (nextDt < minTimeStepSize)
                break; // give up: we can't make the time step smaller anymore!
            simulator().setTimeStepSize(nextDt);

            // update failed
            if (gridView().comm().rank() == 0)
                std::cout << "Newton solver did not converge with "
                          << "dt=" << dt << " seconds. Retrying with time step of "
                          << nextDt << " seconds\n" << std::flush;
        }

        OPM_THROW(std::runtime_error,
                   "Newton solver didn't converge after "
                   << maxFails
                   << " time-step divisions. dt="
                   << simulator().timeStepSize());
    }

    /*!
     * \brief Called by Ewoms::Simulator whenever a solution for a
     *        time step has been computed and the simulation time has
     *        been updated.
     */
    Scalar nextTimeStepSize()
    {
        return std::min(EWOMS_GET_PARAM(TypeTag, Scalar, MaxTimeStepSize),
                        newtonMethod().suggestTimeStepSize(simulator().timeStepSize()));
    }

    /*!
     * \brief Returns true if a restart file should be written to
     *        disk.
     *
     * The default behavior is to write one restart file every 10 time
     * steps. This method should be overwritten by the
     * implementation if the default behavior is deemed insufficient.
     */
    bool shouldWriteRestartFile() const
    {
        return simulator().timeStepIndex() > 0 &&
            (simulator().timeStepIndex() % 10 == 0);
    }

    /*!
     * \brief Returns true if the current solution should be written to
     *        disk (i.e. as a VTK file)
     *
     * The default behavior is to write out the solution for every
     * time step. This method is should be overwritten by the
     * implementation if the default behavior is deemed insufficient.
     */
    bool shouldWriteOutput() const
    { return true; }

    /*!
     * \brief Called by the simulator after the time integration to
     *        do some post processing on the solution.
     */
    void postTimeStep()
    { }

    /*!
     * \brief Called by the simulator after everything which can be
     *        done about the current time step is finished and the
     *        model should be prepared to do the next time integration.
     */
    void advanceTimeLevel()
    { model().advanceTimeLevel(); }

    /*!
     * \brief Called when the end of an simulation episode is reached.
     *
     * Typically, a new episode is started in this method.
     */
    void episodeEnd()
    {
        std::cerr << "The end of an episode is reached, but the problem "
                  << "does not override the episodeEnd() method. "
                  << "Doing nothing!\n";
    }
    // \}

    /*!
     * \brief The problem name.
     *
     * This is used as a prefix for files generated by the simulation.
     * It is highly recommend to overwrite this method in the concrete
     * problem which is simulated.
     */
    static std::string name()
    { return "sim"; }

    /*!
     * \brief The GridView which used by the problem.
     */
    const GridView &gridView() const
    { return gridView_; }

    /*!
     * \brief The coordinate of the corner of the GridView's bounding
     *        box with the smallest values.
     */
    const GlobalPosition &boundingBoxMin() const
    { return boundingBoxMin_; }

    /*!
     * \brief The coordinate of the corner of the GridView's bounding
     *        box with the largest values.
     */
    const GlobalPosition &boundingBoxMax() const
    { return boundingBoxMax_; }

    /*!
     * \brief Returns the mapper for vertices to indices.
     */
    const VertexMapper &vertexMapper() const
    { return vertexMapper_; }

    /*!
     * \brief Returns the mapper for elements to indices.
     */
    const ElementMapper &elementMapper() const
    { return elementMapper_; }

    /*!
     * \brief Returns Simulator object used by the simulation
     */
    Simulator &simulator()
    { return simulator_; }

    /*!
     * \copydoc simulator()
     */
    const Simulator &simulator() const
    { return simulator_; }

    /*!
     * \brief Returns numerical model used for the problem.
     */
    Model &model()
    { return simulator_.model(); }

    /*!
     * \copydoc model()
     */
    const Model &model() const
    { return simulator_.model(); }

    /*!
     * \brief Returns object which implements the Newton method.
     */
    NewtonMethod &newtonMethod()
    { return model().newtonMethod(); }

    /*!
     * \brief Returns object which implements the Newton method.
     */
    const NewtonMethod &newtonMethod() const
    { return model().newtonMethod(); }
    // \}

    /*!
     * \name Restart mechanism
     */
    // \{

    /*!
     * \brief This method writes the complete state of the simulation
     *        to the harddisk.
     *
     * The file will start with the prefix returned by the name()
     * method, has the current time of the simulation clock in it's
     * name and uses the extension <tt>.ers</tt>. (Ewoms ReStart
     * file.)  See Ewoms::Restart for details.
     */
    void serialize()
    {
        typedef Ewoms::Restart Restarter;
        Restarter res;
        res.serializeBegin(simulator_);
        if (gridView().comm().rank() == 0)
            std::cout << "Serialize to file '" << res.fileName() << "'"
                      << ", next time step size: " << asImp_().simulator().timeStepSize()
                      << "\n" << std::flush;

        simulator().serialize(res);
        asImp_().serialize(res);
        res.serializeEnd();
    }

    /*!
     * \brief This method writes the complete state of the problem
     *        to the harddisk.
     *
     * The file will start with the prefix returned by the name()
     * method, has the current time of the simulation clock in it's
     * name and uses the extension <tt>.ers</tt>. (Ewoms ReStart
     * file.)  See Ewoms::Restart for details.
     *
     * \tparam Restarter The serializer type
     *
     * \param res The serializer object
     */
    template <class Restarter>
    void serialize(Restarter &res)
    {
        defaultVtkWriter_.serialize(res);
        model().serialize(res);
    }

    /*!
     * \brief Load a previously saved state of the whole simulation
     *        from disk.
     *
     * \param tRestart The simulation time on which the program was
     *                 written to disk.
     */
    void restart(Scalar tRestart)
    {
        typedef Ewoms::Restart Restarter;

        Restarter res;

        res.deserializeBegin(simulator_, tRestart);
        if (gridView().comm().rank() == 0)
            std::cout << "Deserialize from file '" << res.fileName() << "'\n" << std::flush;
        simulator().deserialize(res);
        asImp_().deserialize(res);
        res.deserializeEnd();
    }

    /*!
     * \brief This method restores the complete state of the problem
     *        from disk.
     *
     * It is the inverse of the serialize() method.
     *
     * \tparam Restarter The deserializer type
     *
     * \param res The deserializer object
     */
    template <class Restarter>
    void deserialize(Restarter &res)
    {
        defaultVtkWriter_.deserialize(res);
        model().deserialize(res);
    }

    // \}

    /*!
     * \brief Write the relevant secondary variables of the current
     *        solution into an VTK output file.
     *
     * \param verbose If true, then a message will be printed to stdout if a file is written
     */
    void writeOutput(bool verbose = true)
    {
        // write the current result to disk
        if (asImp_().shouldWriteOutput()) {
            if (verbose && gridView().comm().rank() == 0)
                std::cout << "Writing result file for \"" << asImp_().name() << "\""
                          << "\n" << std::flush;

            // calculate the time _after_ the time was updated
            Scalar t = simulator().time() + simulator().timeStepSize();
            defaultVtkWriter_.beginWrite(t);
            model().addOutputFields(defaultVtkWriter_);
            defaultVtkWriter_.endWrite();
        }
    }

    /*!
     * \brief Method to retrieve the VTK writer which should be used
     *        to write the default ouput after each time step to disk.
     */
    VtkMultiWriter &defaultVtkWriter() const
    { return defaultVtkWriter_; }

private:
    //! Returns the implementation of the problem (i.e. static polymorphism)
    Implementation &asImp_()
    { return *static_cast<Implementation *>(this); }

    //! \copydoc asImp_()
    const Implementation &asImp_() const
    { return *static_cast<const Implementation *>(this); }

    // Grid management stuff
    const GridView gridView_;
    ElementMapper elementMapper_;
    VertexMapper vertexMapper_;
    GlobalPosition boundingBoxMin_;
    GlobalPosition boundingBoxMax_;

    // Attributes required for the actual simulation
    Simulator &simulator_;
    mutable VtkMultiWriter defaultVtkWriter_;

    // CPU time keeping
    Scalar assembleTime_;
    Scalar solveTime_;
    Scalar updateTime_;
};

} // namespace Ewoms

#endif