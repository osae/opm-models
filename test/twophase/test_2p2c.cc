#include "config.h"
#include <iostream>
#ifdef HAVE_UG
#include <iomanip>
#include <dune/grid/common/gridinfo.hh>
#include <dune/grid/uggrid.hh>
#include <dune/grid/io/file/dgfparser/dgfparser.hh>
#include <dune/grid/io/file/dgfparser/dgfug.hh>
#include <dune/grid/io/file/vtk/vtkwriter.hh>
#include <dune/istl/io.hh>
#include <dune/common/timer.hh>
//#include "dumux/2p2c/problems/lensproblem2p2c.hh"
#include "dumux/2p2c/problems/layerproblem.hh"
#include "dumux/2p2c/problems/uniformtwophaseproblem.hh"
#include "dumux/2p2c/fv/box2p2c.hh"
//#include "dumux/twophase/fv/boxpwsn.hh"
#include "dumux/timedisc/timeloop.hh"
#include "dumux/material/vangenuchtenlaw.hh"
#include "dumux/material/solubilities.hh"
#include "dumux/material/properties.hh"
#include "dumux/material/constrel/constrelwater.hh"
//#include "dumux/material/water_props.hh"


int main(int argc, char** argv) 
{
  try{
    // define the problem dimensions (geometry of problem)  
    const int dim=2;
    typedef double NumberType;
    Dune::FieldVector<NumberType, dim> outerLowerLeft(0);
    Dune::FieldVector<NumberType, dim> outerUpperRight(6);
    outerUpperRight[1] = 4;
    Dune::FieldVector<NumberType, dim> innerLowerLeft(0);
    innerLowerLeft[1] = 1;
    Dune::FieldVector<NumberType, dim> innerUpperRight(6);
    innerUpperRight[1] = 1.5;
    double depthBOR = 5;

    // count number of arguments
    if (argc != 3) {
      std::cout << "usage: test_twophase tEnd dt" << std::endl;
      return 0;
    }
    // define tEnd
    std::string arg1(argv[1]);
    std::istringstream is1(arg1);
    double tEnd; is1 >> tEnd;
    // define dt
    std::string arg2(argv[2]);
    std::istringstream is2(arg2);
    double dt; is2 >> dt;


    // create a grid object
    //typedef Dune::SGrid<dim,dim> GridType; 
    //typedef Dune::YaspGrid<dim,dim> GridType; 
    typedef Dune::UGGrid<dim> GridType; 

    // use unitcube from grids (UGGrid)
    std::stringstream dgfFileName;
    dgfFileName << "/temp/klaus/DUMUX/dune-mux/test/twophase/grids/unitcube" 
    	<< GridType :: dimension << ".dgf";

    // create grid pointer, GridType is defined by gridtype.hh
    Dune::GridPtr<GridType> gridPtr( dgfFileName.str() );

    // grid reference 
    GridType& grid = *gridPtr;

    Dune::gridinfo(grid);

    // choose fluids and properties
    Air air; Water water;
    Dune::VanGenuchtenLaw law(water, air);
    //Dune::LinearLaw law(water, air);
    
    // create problem properties and geometry
    Dune::LayerProblem<GridType, NumberType> problem(law, outerLowerLeft, outerUpperRight, 
    		innerLowerLeft, innerUpperRight, depthBOR);

    // create two-phase two-component problem
    typedef Dune::Box2P2C<GridType, NumberType> TwoPhaseTwoComp;
    TwoPhaseTwoComp twoPhasetwoComp(grid, problem);
    
    Dune::TimeLoop<GridType, TwoPhaseTwoComp> timeloop(0, tEnd, dt, "lens", 5);
    
    Dune::Timer timer;
    timer.reset();
    timeloop.execute(twoPhasetwoComp);
    std::cout << "timeloop.execute took " << timer.elapsed() << " seconds" << std::endl;

// COMPARSION TO TWOPHASE MODEL    
//    Dune::LensProblemOld<GridType, NumberType> problemOld(law, outerLowerLeft, outerUpperRight, 
//    		innerLowerLeft, innerUpperRight, depthBOR);
//
//    typedef Dune::BoxPwSn<GridType, NumberType> TwoPhaseOld;
//    TwoPhaseOld twoPhaseOld(grid, problemOld);
//    
//    Dune::TimeLoop<GridType, TwoPhaseOld> timeloopOld(0, tEnd, dt, "lensOld", 1);
//    
//    //Dune::Timer timer;
//    timer.reset();
//    timeloopOld.execute(twoPhaseOld);
//    std::cout << "timeloop.execute took " << timer.elapsed() << " seconds" << std::endl;
//
//    *twoPhasetwoComp.u -= *twoPhaseOld.u; 
//    std::cout << "difference = " << (*twoPhasetwoComp.u).two_norm()/(*twoPhaseOld.u).two_norm() 
//    	<< std::endl;

////////////////////////////////
    
    
//    
//    	printvector(std::cout, *twoPhase.u, "u", "row", 2, 1, 3);

    return 0;
  }
  catch (Dune::Exception &e){
    std::cerr << "Dune reported error: " << e << std::endl;
  }
  catch (...){
    std::cerr << "Unknown exception thrown!" << std::endl;
  }
}
#else 

int main (int argc , char **argv) try
{
  std::cout << "Please install the UG library." << std::endl;

  return 1;
}
catch (...) 
{
    std::cerr << "Generic exception!" << std::endl;
    return 2;
}
#endif 
