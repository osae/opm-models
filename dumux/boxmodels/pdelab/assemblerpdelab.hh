#ifdef HAVE_DUNE_PDELAB

#ifndef DUMUX_ASSEMBLERPDELAB_HH
#define DUMUX_ASSEMBLERPDELAB_HH

#include<dune/pdelab/finiteelementmap/p1fem.hh>
#include<dune/pdelab/finiteelementmap/q1fem.hh>
#include<dune/pdelab/gridfunctionspace/gridfunctionspace.hh>
#include<dune/pdelab/gridfunctionspace/genericdatahandle.hh>
#include<dune/pdelab/finiteelementmap/conformingconstraints.hh>
#include<dune/pdelab/backend/istlvectorbackend.hh>
#include<dune/pdelab/backend/istlmatrixbackend.hh>
#include"boundarytypespdelab.hh"
#include"boxjacobianpdelab.hh"


template<class TypeTag>
class AssemblerPDELab
{
public:
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Model))    Model;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Problem))  Problem;
    enum{numEq = GET_PROP_VALUE(TypeTag, PTAG(NumEq))};
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(GridView))   GridView;
    enum{dim = GridView::dimension};
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Scalar))  Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(LocalFEMSpace)) FEM;
    typedef typename GET_PROP(TypeTag, PTAG(PDELabTypes)) PDELabTypes;
    typedef typename PDELabTypes::Constraints Constraints;
    typedef typename PDELabTypes::ScalarGridFunctionSpace ScalarGridFunctionSpace;
    typedef typename PDELabTypes::GridFunctionSpace GridFunctionSpace;
	typedef typename PDELabTypes::ConstraintsTrafo ConstraintsTrafo;
    typedef typename PDELabTypes::LocalOperator LocalOperator;
    typedef typename PDELabTypes::GridOperatorSpace GridOperatorSpace;
    typedef BoundaryIndexHelperPDELab<TypeTag> BoundaryFunction;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(LocalJacobian)) LocalJacobian;
    typedef typename GET_PROP(TypeTag, PTAG(SolutionTypes)) SolutionTypes;
    typedef typename SolutionTypes::SolutionFunction        SolutionFunction;
	typedef typename GridFunctionSpace::template VectorContainer<Scalar>::Type Vector;
    typedef typename GridOperatorSpace::template MatrixContainer<Scalar>::Type Matrix;
    typedef Matrix RepresentationType;

    AssemblerPDELab(Model &model, Problem& problem)
    : problem_(problem)
    {
    	fem_ = 0;
    	cn_ = 0;
    	scalarGridFunctionSpace_ = 0;
    	gridFunctionSpace_ = 0;
    	bTypes_ = 0;
    	constraintsTrafo_ = 0;
    	localOperator_ = 0;
    	gridOperatorSpace_ = 0;
    	matrix_ = 0;

    	fem_ = new FEM();
        cn_ = new Constraints(problem_);
    	scalarGridFunctionSpace_ = new ScalarGridFunctionSpace(problem_.gridView(), *fem_, *cn_);
    	gridFunctionSpace_ = new GridFunctionSpace(*scalarGridFunctionSpace_);

    	cn_->compute_ghosts(*gridFunctionSpace_);

    	bTypes_ = new BoundaryFunction();
    	constraintsTrafo_ = new ConstraintsTrafo();
    	Dune::PDELab::constraints(*bTypes_, *gridFunctionSpace_, *constraintsTrafo_, false);

    	localOperator_ = new LocalOperator(model);
    	gridOperatorSpace_ = new GridOperatorSpace(*gridFunctionSpace_, *constraintsTrafo_,
												   *gridFunctionSpace_, *constraintsTrafo_, *localOperator_);

    	matrix_ = new Matrix(*gridOperatorSpace_);
    	*matrix_ = 0;
    }

    //! return const reference to matrix
    const Matrix& operator* () const
    {
        return *matrix_;
    }

    //! return reference to matrix
    Matrix& operator* ()
    {
        return *matrix_;
    }

    void assemble(LocalJacobian& loc, SolutionFunction &u, SolutionFunction &f)
    {
    	*matrix_ = 0;
    	gridOperatorSpace_->jacobian(*u, *matrix_);

    	*f = 0;
    	gridOperatorSpace_->residual(*u, *f);

    	set_constrained_dofs(*constraintsTrafo_, 0.0, *f);
    	set_constrained_dofs(*constraintsTrafo_, 0.0, *u);

#if 1
//    	Vector numProcessesVec(*f);
//    	numProcessesVec = 1.0;
//    	Dune::PDELab::AddDataHandle<GridFunctionSpace,Vector> adddh(*gridFunctionSpace_, numProcessesVec);
//    	if (gridFunctionSpace_->gridview().comm().size()>1)
//    	  gridFunctionSpace_->gridview().communicate(adddh,
//    	      Dune::InteriorBorder_InteriorBorder_Interface,
//    	      Dune::ForwardCommunication);


        // rescale jacobian and right hand side to the largest 
        // entry on the main diagonal block matrix
    	typedef typename Matrix::RowIterator RowIterator;
    	typedef typename Matrix::ColIterator ColIterator;
        typedef typename Matrix::block_type  BlockType;
    	const typename Matrix::block_type::size_type rowsInBlock = Matrix::block_type::rows;
    	const typename Matrix::block_type::size_type colsInBlock = Matrix::block_type::cols;
        Scalar diagonalEntry[rowsInBlock];
        Vector diagonalEntries(*f);
    	RowIterator endIBlock = matrix_->end();
    	for (RowIterator iBlock = matrix_->begin(); iBlock != endIBlock; ++iBlock) {
            BlockType &diagBlock = (*iBlock)[iBlock.index()];
            
            for (int i = 0; i < rowsInBlock; ++i) {
                diagonalEntry[i] = 0;
                for (int j = 0; j < colsInBlock; ++j) {
                    diagonalEntry[i] = std::max(diagonalEntry[i], 
                                                std::abs(diagBlock[i][j]));
                }

                if (diagonalEntry[i] < 1e-14)
                    diagonalEntry[i] = 1.0;

                diagonalEntries[iBlock.index()][i] = diagonalEntry[i];
            }
    	}

        Dune::PDELab::AddDataHandle<GridFunctionSpace,Vector> adddh(*gridFunctionSpace_, diagonalEntries);
        if (gridFunctionSpace_->gridview().comm().size()>1)
          gridFunctionSpace_->gridview().communicate(adddh,
              Dune::InteriorBorder_InteriorBorder_Interface,
              Dune::ForwardCommunication);

        for (RowIterator iBlock = matrix_->begin(); iBlock != endIBlock; ++iBlock) {
            // divide right-hand side
            for (int i = 0; i < rowsInBlock; i++) {
                (*f)[iBlock.index()][i] /= diagonalEntries[iBlock.index()][i];
            }
            
            // divide row of the jacobian
            ColIterator endJBlock = iBlock->end();
            for (ColIterator jBlock = iBlock->begin(); jBlock != endJBlock; ++jBlock) {
                for (int i = 0; i < rowsInBlock; i++) {
                    for (int j = 0; j < colsInBlock; j++) {
                        (*jBlock)[i][j] /= diagonalEntries[iBlock.index()][i];
                    }
                }
            }
        }
#endif
    }

    const GridFunctionSpace& gridFunctionSpace() const
    {
        return *gridFunctionSpace_;
    }

    const ConstraintsTrafo& constraintsTrafo() const
    {
        return *constraintsTrafo_;
    }

    ~AssemblerPDELab()
    {
    	delete matrix_;
    	delete gridOperatorSpace_;
    	delete localOperator_;
    	delete constraintsTrafo_;
    	delete bTypes_;
    	delete gridFunctionSpace_;
    	delete scalarGridFunctionSpace_;
    	delete fem_;
    	delete cn_;
    }

private:
	Problem& problem_;
	Constraints *cn_;
    FEM *fem_;
    ScalarGridFunctionSpace *scalarGridFunctionSpace_;
    GridFunctionSpace *gridFunctionSpace_;
    BoundaryFunction *bTypes_;
    ConstraintsTrafo *constraintsTrafo_;
    LocalOperator *localOperator_;
    GridOperatorSpace *gridOperatorSpace_;
    Matrix *matrix_;
};

#endif

#endif
