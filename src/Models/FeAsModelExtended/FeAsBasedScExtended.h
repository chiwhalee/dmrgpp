// BEGIN LICENSE BLOCK
/*
Copyright (c) 2009, UT-Battelle, LLC
All rights reserved

[DMRG++, Version 2.0.0]
[by G.A., Oak Ridge National Laboratory]

UT Battelle Open Source Software License 11242008

OPEN SOURCE LICENSE

Subject to the conditions of this License, each
contributor to this software hereby grants, free of
charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), a
perpetual, worldwide, non-exclusive, no-charge,
royalty-free, irrevocable copyright license to use, copy,
modify, merge, publish, distribute, and/or sublicense
copies of the Software.

1. Redistributions of Software must retain the above
copyright and license notices, this list of conditions,
and the following disclaimer.  Changes or modifications
to, or derivative works of, the Software should be noted
with comments and the contributor and organization's
name.

2. Neither the names of UT-Battelle, LLC or the
Department of Energy nor the names of the Software
contributors may be used to endorse or promote products
derived from this software without specific prior written
permission of UT-Battelle.

3. The software and the end-user documentation included
with the redistribution, with or without modification,
must include the following acknowledgment:

"This product includes software produced by UT-Battelle,
LLC under Contract No. DE-AC05-00OR22725  with the
Department of Energy."
 
*********************************************************
DISCLAIMER

THE SOFTWARE IS SUPPLIED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER, CONTRIBUTORS, UNITED STATES GOVERNMENT,
OR THE UNITED STATES DEPARTMENT OF ENERGY BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

NEITHER THE UNITED STATES GOVERNMENT, NOR THE UNITED
STATES DEPARTMENT OF ENERGY, NOR THE COPYRIGHT OWNER, NOR
ANY OF THEIR EMPLOYEES, REPRESENTS THAT THE USE OF ANY
INFORMATION, DATA, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED WOULD NOT INFRINGE PRIVATELY OWNED RIGHTS.

*********************************************************


*/
// END LICENSE BLOCK
/** \ingroup DMRG */
/*@{*/

/*! \file FeBasedScExtedned.h
 *
 *  An implementation of a Hubbard model for Fe-based superconductors
 *  to use with the DmrgSolver
 *  This extends the FeAsBasedSc model to include JNN and JNNN couplings
 *
 */
#ifndef FEAS_BASED_SC_EX
#define FEAS_BASED_SC_EX
#include "ModelFeAsBasedSc.h"
#include "LinkProductFeAsExtended.h"

namespace Dmrg {
	template<
		typename ModelHelperType_,
		typename SparseMatrixType,
		typename GeometryType,
  		template<typename> class SharedMemoryTemplate>
	class FeAsBasedScExtended : public ModelBase<
			ModelHelperType_,SparseMatrixType,GeometryType,
			LinkProductFeAsExtended<ModelHelperType_>,
			SharedMemoryTemplate> {
		
	public:
		typedef ModelFeAsBasedSc<ModelHelperType_,SparseMatrixType,
			GeometryType,LinkProductFeAsExtended<ModelHelperType_>
			ModelFeAsType;

		typedef ModelHelperType_ ModelHelperType;
		typedef typename ModelHelperType::OperatorsType OperatorsType;
		typedef typename OperatorsType::OperatorType OperatorType;
		typedef typename ModelHelperType::RealType RealType;
		typedef typename SparseMatrixType::value_type SparseElementType;

		typedef typename ModelHelperType::ConcurrencyType ConcurrencyType;
		typedef LinkProductFeAsExtended<ModelHelperType> LinkProductType;
		typedef ModelBase<ModelHelperType,SparseMatrixType,GeometryType,
				LinkProductType,SharedMemoryTemplate> ModelBaseType;
		typedef	 typename ModelBaseType::MyBasis MyBasis;
		typedef	 typename ModelBaseType::BasisWithOperatorsType
				MyBasisWithOperators;
		typedef typename MyBasis::BasisDataType BasisDataType;

		FeAsBasedScExtended(ParametersModelFeAs<RealType> const &mp,
				GeometryType const &geometry)
			: ModelBaseType(geometry),modelParameters_(mp), geometry_(geometry),
			  modelFeAs_(mp,geometry)
		{}

		size_t orbitals() const { return modelFeAs_.orbitals(); }

		size_t hilbertSize() const { return modelFeAs_.hilbertSize(); }

		void print(std::ostream& os) const { modelFeAs_.print(os); }

		//! find creation operator matrices for (i,sigma) in the natural basis,
		//! find quantum numbers and number of electrons
		//! for each state in the basis
		void setNaturalBasis(
				std::vector<OperatorType> &creationMatrix,
				SparseMatrixType &hamiltonian,
				BasisDataType &q,
				Block const &block)  const
		{
			modelFeAs_.setNaturalBasis(creationMatrix,hamiltonian,q,block);

			// add S^+_i to creationMatrix
			setSplus(creationMatrix,block);

			// add S^z_i to creationMatrix
			setSz(creationMatrix,block);

			// add J_{ij} S^+_i S^-_j + S^-_i S^+_j to Hamiltonia
			addSplusSminus(hamiltonian,creationMatrix,block);

			// add J_{ij} S^z_i S^z_j to Hamiltonian
			addSzSz(hamiltonian,creationMatrix,block);

		}

		//! set creation matrices for sites in block
		void setOperatorMatrices(
				std::vector<OperatorType> &creationMatrix,
				Block const &block) const
		{
			modelFeAs_.setOperatorMatrices(creationMatrix,block);

			// add S^+_i to creationMatrix
			setSplus(creationMatrix,block);

			// add S^z_i to creationMatrix
			setSz(creationMatrix,block);
		}

		PsimagLite::Matrix<SparseElementType> getOperator(
				const std::string& what,
				size_t orbital=0,
				size_t spin=0) const
		{
			Block block;
			block.resize(1);
			block[0]=0;
			std::vector<OperatorType> creationMatrix;
			setOperatorMatrices(creationMatrix,block);

			if (what=="z") {
				PsimagLite::Matrix<SparseElementType> tmp;
				size_t x = 2*NUMBER_OF_ORBITALS+1;
				crsMatrixToFullMatrix(tmp,creationMatrix[x].data);
				return tmp;
			}

			if (what=="+") {
				PsimagLite::Matrix<SparseElementType> tmp;
				size_t x = 2*NUMBER_OF_ORBITALS;
				crsMatrixToFullMatrix(tmp,creationMatrix[x].data);
				return tmp;
			}

			if (what=="-") { // delta = c^\dagger * c^dagger
				PsimagLite::Matrix<SparseElementType> tmp;
				size_t x = 2*NUMBER_OF_ORBITALS;
				crsMatrixToFullMatrix(tmp,creationMatrix[x].data);
				transposeConjugate(creationMatrix,tmp);
				return tmp;
			}
			return modelFeAs_.getOperator(what,orbital,spin);
		}
		
		//! find all states in the natural basis for a block of n sites
		//! N.B.: HAS BEEN CHANGED TO ACCOMODATE FOR MULTIPLE BANDS
		void setNaturalBasis(std::vector<HilbertState>  &basis,int n) const
		{
			modelFeAs_.setNaturalBasis(basis,n);
		}
		
		void findElectrons(
				std::vector<size_t>& electrons,
				const std::vector<HilbertState>  &basis) const
		{
			modelFeAs_.findElectrons(electrons,basis);
		}
		

	private:
		const ParametersModelFeAs<RealType>&  modelParameters_;
		GeometryType const &geometry_;
		ModelFeAsType modelFeAs_;

	};     //class FeAsBasedScExtended

	template<
		typename ModelHelperType,
		typename SparseMatrixType,
		typename GeometryType,
  		template<typename> class SharedMemoryTemplate
		>
	std::ostream &operator<<(std::ostream &os,const FeBasedScExtended<
		ModelHelperType,
		SparseMatrixType,
		GeometryType,
  		SharedMemoryTemplate
		>& model)
	{
		model.print(os);
		return os;
	}
} // namespace Dmrg
/*@}*/
#endif // FEAS_BASED_SC_EX
