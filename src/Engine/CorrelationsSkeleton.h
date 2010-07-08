// BEGIN LICENSE BLOCK
/*
Copyright � 2008 , UT-Battelle, LLC
All rights reserved

[DMRG++, Version 1.0.0]
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

/*! \file CorrelationsSkeleton.h
 *
 *  Helper class for Observables
 *
 */
#ifndef CORRELATIONS_SK_H
#define CORRELATIONS_SK_H
#include "Precomputed.h"
#include "CrsMatrix.h"


namespace Dmrg {
	
	//! Don't add functions to this class
	template<typename FieldType>
	struct CorrelationData {
		size_t ni;
		size_t nj;
		std::vector<psimag::Matrix<FieldType> > correlationVector;
		SparseVector<FieldType> wavefunction;
	};
	
	//! Companion function:
	template<typename FieldType>
	std::ostream& operator<<(std::ostream& os,CorrelationData<FieldType>& c)
	{
		os<<"ni="<<c.ni<<"\n";
		os<<"nj="<<c.nj<<"\n";
		os<<c.correlationVector;
		os<<c.wavefunction;
		return os;
	}
	
	template<typename RealType,typename FieldType,typename IoType,
 		typename MatrixType,template<typename> class VectorTemplate>
	class CorrelationsSkeleton {
		//typedef typename MatrixType::value_type FieldType;
		typedef size_t IndexType;
		typedef Precomputed<RealType,FieldType,IoType,MatrixType,VectorTemplate> PrecomputedType;
		
	public:
		enum {GROW_RIGHT,GROW_LEFT};
		enum {DIAGONAL,NON_DIAGONAL};
		
		CorrelationsSkeleton(PrecomputedType& precomp,bool verbose = false) : precomp_(precomp),verbose_(verbose) { } 
		
		void createSigns(std::vector<int>& signs,int f)
		{
			signs.resize(precomp_.fermionicSigns());
			for (size_t i=0;i<signs.size();i++) signs[i] = precomp_.fermionicSign(i,f);
		}
		
		//! i can be zero here!!
		void growDirectly(MatrixType& Odest,const MatrixType& Osrc,size_t i,int fermionicSign,size_t ns)
		{
			Odest =Osrc;
			std::vector<int> signs;
			// from 0 --> i
			int nt=i-1;
			if (nt<0) nt=0;
			
			for (size_t s=nt;s<ns;s++) {
				// set appropriate privates which are:
				// SpermutationInverse_(s) and transform_(s)
				/*io_.rewind();
				calcSpermutation(s);
				//std::cerr<<"*****************======="<<transform_.n_row()<<"\n";
				io_.readMatrix(transform_,"#TRANSFORM_sites",s);*/
				precomp_.setPointer(s);
				//std::cerr<<"%%%%%%%%%%%%%%%%%======="<<transform_.n_row()<<"\n";
				int growOption = GROW_RIGHT;
				//if (i==1 && s==0) growOption = GROW_LEFT;// <-- not needed since nt>0
				if (s==size_t(nt)) {
					growOption = GROW_LEFT;
					if (i==0) growOption = GROW_RIGHT;
				}
				/*io_.rewind();
				io_.read(electrons_,"#ELECTRONS_sites=",s);*/
				createSigns(signs,fermionicSign);
				MatrixType Onew(precomp_.transform().n_col(),precomp_.transform().n_col());
				fluffUp(Onew,Odest,signs,growOption);
				Odest = Onew;
				
			}
		}
		
		// Perfomance critical:	
		void fluffUp(MatrixType& ret2,const MatrixType& O,const std::vector<int>& signs,int growOption=GROW_RIGHT)
		{
			size_t n = precomp_.SpermutationInverse().size();
			MatrixType ret(n,n);

			for (size_t e=0;e<n;e++) {
				for (size_t e2=0;e2<n;e2++) {
					ret(e,e2) = fluffUp(O,e,e2,signs,growOption);
				}
			}
			precomp_.transform(ret2,ret);
		}

		// Perfomance critical:
		FieldType fluffUp(const MatrixType& O,size_t e,size_t e2,const std::vector<int>& signs,
					  int growOption=GROW_RIGHT)	
		{
			
			size_t n = O.n_row();
			size_t m = size_t(precomp_.Spermutation().size()/n);
			FieldType sign = static_cast<FieldType>(1.0);
			
			// Sperm[e] = i +k*n or e= k + i*m
			// Sperm[e2] = j+k*n or e2=k+j*m
			size_t i,j,k,k2;
			if (growOption==GROW_RIGHT) {	
				if (size_t(precomp_.Spermutation(e)/n)!=size_t(precomp_.Spermutation(e2)/n)) return 0;
				utils::getCoordinates(i,k,precomp_.Spermutation(e),n);
				utils::getCoordinates(j,k2,precomp_.Spermutation(e2),n);
			} else {
				if (size_t(precomp_.Spermutation(e)%m)!=size_t(precomp_.Spermutation(e2)%m)) return 0;
				utils::getCoordinates(k,i,precomp_.Spermutation(e),m);
				utils::getCoordinates(k2,j,precomp_.Spermutation(e2),m);
				sign = signs[k];
			}
			FieldType ret=0;
			if (k==k2) ret=O(i,j)*sign;
			return ret;
		}
		
		void dmrgMultiply(MatrixType& result,
					const MatrixType& O1,
					const MatrixType& O2,
					int fermionicSign,
				 	size_t ns)
		{
			size_t ni=O1.n_row();
			size_t nj=O2.n_row();
			/*std::cerr<<"Mult,O1\n";
			std::cerr<<O1;
			
			std::cerr<<"Mult,O2\n";
			std::cerr<<O2;*/
			
			precomp_.setPointer(ns);
			//size_t wfsize = precomp_.wavefunction().size();
			//size_t envSize = wfsize/(ni*nj);
			size_t sprime = ni*nj;
			result.resize(sprime,sprime);
			
			for (size_t r=0;r<result.n_row();r++)
				for (size_t r2=0;r2<result.n_col();r2++)
					result(r,r2)=0;
			
			if (precomp_.Spermutation().size()!=sprime) {
				std::cerr<<"WARNING: "<<precomp_.Spermutation().size()<<"!="<<sprime<<"\n";
				throw std::runtime_error("problem\n");
			}
			
			for (size_t r=0;r<sprime;r++) {
				//size_t r,eta;
				//utils::getCoordinates(r,eta,precomp_.SEpermutation(x),ni*nj);
				size_t e,u;
				utils::getCoordinates(e,u,precomp_.Spermutation(r),ni);
				FieldType f = precomp_.fermionicSign(e,fermionicSign);
				for (size_t e2=0;e2<ni;e2++) {	
					for (size_t u2=0;u2<nj;u2++) {
						size_t r2 = precomp_.SpermutationInverse()[e2 + u2*ni];
						//size_t x2 = r2 + eta*envSize;
						result(r,r2) += O1(e,e2)*O2(u,u2)*f;
					}
				}
			}
			if (result.n_row()!=precomp_.transform().n_row()) {
				std::cerr<<result.n_row()<<" "<<precomp_.transform().n_row()<<"\n";
				throw std::runtime_error("dmrgMultiply: mismatch in transform\n");
			}
			/*size_t trunc = precomp_.transform().n_col();
			result2.resize(trunc,trunc);
			precomp_.transform(result2,result);*/
		}
		
		FieldType bracket(const MatrixType& A,size_t useTimeVector=PrecomputedType::NOTIMEVECTOR)
		{
			if (useTimeVector == PrecomputedType::NOTIMEVECTOR) {
				const typename PrecomputedType::VectorType& v = precomp_.wavefunction();
				return bracket_(A,v);
			} else {
				const typename PrecomputedType::VectorType& v2 = precomp_.timeVector();
				return bracket_(A,v2);
			}
		}
		
		template<typename SomeVectorType>
		FieldType bracket_(const MatrixType& A,const SomeVectorType& vec)
		{
			//typedef typename SomeVectorType::value_type ComplexOrRealType;
			FieldType zeroc = 0;
			
			RealType norma = std::norm(vec);
			//std::cerr<<"MatrixA\n";
			//std::cerr<<A;
			if (verbose_) std::cerr<<"SE.size="<<precomp_.SEpermutation()<<"\n";
			
			//if (precomp_.SEpermutation()!=A.size()) throw std::runtime_error("problem in bracket\n");
			CrsMatrix<FieldType> Acrs(A);
			FieldType sum=0;
			//size_t counter=0;
			if (vec.size()!=precomp_.SEpermutation()) throw std::runtime_error("Error\n");
			for (size_t x=0;x<vec.indices();x++) {
				size_t t=vec.index(x);
				size_t eta,r;
				
				utils::getCoordinates(r,eta,precomp_.SEpermutation(t),precomp_.Spermutation().size());
				//for (size_t r2=0;r2<A.n_col();r2++) {
				for (int k=Acrs.getRowPtr(r);k<Acrs.getRowPtr(r+1);k++) {
					size_t r2 = Acrs.getCol(k);
					size_t t2 = precomp_.SEpermutationInverse(r2+eta*A.n_col());
					if (vec[t2]==zeroc) continue;
					//if (A(r,r2)==0) continue;
					//counter++;
					//try {
					sum += //A(r,r2)
						Acrs.getValue(k)*vec.value(x)*vec[t2]/norma;
					//} catch (std::exception& exception) {
						
					//}
				}
			}
			//std::cerr<<"counter="<<counter<<"\n";
			return std::real(sum);
		}
		
		void createWithModification(MatrixType& Om,const MatrixType& O,char mod)
		{
			if (mod == 'n' || mod == 'N') {
				Om = O;
				return;
			}
			utils::transposeConjugate(Om,O);
		}
		
	private:
		PrecomputedType& precomp_; //<-- NB: We are not the owner
		bool verbose_;
		
	};  //class CorrelationsSkeleton
} // namespace Dmrg

/*@}*/
#endif
