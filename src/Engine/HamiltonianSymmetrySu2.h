/*
Copyright (c) 2009-2013, UT-Battelle, LLC
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
/** \ingroup DMRG */
/*@{*/

/*! \file HamiltonianSymmetrySu2.h
	*
	*  An object of this class is meant to be contained by a Hilbert Space basis
	*  and then this class help creating the outer product states necessary to preserve the SU(2) symmetry
	*  THe counterpart to this class is HamiltonianSymmetryLocal.h for local symmetries only
	*
	*/
#ifndef HAM_SYMM_SU2_H
#define HAM_SYMM_SU2_H

#include "Sort.h" // in PsimagLite
#include "JmPairs.h"
#include "VerySparseMatrix.h"
#include "JmSubspace.h"
#include "ProgramGlobals.h"
#include "CrsMatrix.h"

namespace Dmrg {

	template<typename RealType,typename SparseMatrixType>
	class	HamiltonianSymmetrySu2 {
		public:
			typedef std::pair<SizeType,SizeType> PairType;
		private:
			typedef JmPairs<PairType> JmPairsType;
			typedef VerySparseMatrix<RealType> VerySparseMatrixType;
			typedef HamiltonianSymmetrySu2<RealType,SparseMatrixType> ThisType;
			typedef JmSubspace<RealType,VerySparseMatrixType,ThisType> JmSubspaceType;
			typedef typename JmSubspaceType::FlavorType FlavorType;
			typedef  BasisData<PairType> BasisDataType;
			
		public:
			typedef PsimagLite::CrsMatrix<RealType> FactorsType;
			
			static SizeType const MAX = ProgramGlobals::MaxNumberOfSites;

			HamiltonianSymmetrySu2()
			: flavors_(0),
			  flavorsOld_(0),
			  flavorsMax_(0),
			  electronsMax_(0),
			  jMax_(0),
			  factors_(0,0),
			  statesReduced_(0),
			  jvals_(0)
			{}

			PairType jmValue(SizeType i) const { return jmValues_[i]; }

			void set(const BasisDataType& basisData)
			{
				jmValues_=basisData.jmValues;
				flavors_=basisData.flavors;
				flavorsMax_= *(std::max_element(flavors_.begin(),flavors_.end()));
				electronsMax_ = *(std::max_element(basisData.electrons.begin(),basisData.electrons.end()));
				jMax_=0;
				jmValues_.maxFirst<std::greater<SizeType> >(jMax_);
				jMax_++;
				calcReducedBasis();
			}

			static void findQuantumNumbers(typename PsimagLite::Vector<SizeType> ::Type&q,
			                               const BasisDataType& basisData)
			{
				q.resize(basisData.electrons.size());
				for (SizeType i=0;i<q.size();i++) {
					SizeType ne = basisData.electrons[i];
					PairType jmpair = basisData.jmValues[i];
					q[i]=neJmToIndex(ne,jmpair);
				}
			}

			static SizeType neJmToIndex(SizeType ne,const PairType& jm) 
			{
				typename PsimagLite::Vector<SizeType>::Type v(3);
				RealType m = jm.second+0.5*(ne-jm.first);
				v[0]=static_cast<SizeType>(m);
				assert(ne>=v[0]);
				v[1] = ne - v[0];
				v[2]=jm.first;
				return encodeQuantumNumber(v);
			}

			static SizeType encodeQuantumNumber(const typename PsimagLite::Vector<SizeType>::Type& v)
			{
				SizeType x= v[0] + v[1]*MAX;
				if (v[0]>=MAX || v[1]>=MAX || v[2]>=MAX)
					throw PsimagLite::RuntimeError("encodeQuantumNumber\n");
				if (v.size()==3) x += v[2]*MAX*MAX;
				return x;
			}

			static typename PsimagLite::Vector<SizeType>::Type decodeQuantumNumber(SizeType q)
			{
				typename PsimagLite::Vector<SizeType>::Type v(3);
				v[2] = SizeType(q/(MAX*MAX));
				SizeType tmp = q - v[2]*MAX*MAX;
				v[1] = SizeType(tmp/MAX);
				v[0] = tmp % MAX;
				return v;
			}

			//! targets[0]=nup, targets[1]=ndown,  targets[2]=2j
			static SizeType pseudoQuantumNumber(const typename PsimagLite::Vector<SizeType>::Type& v)
			{
				SizeType x= v[0] + v[1];
				x += v[2]*2*MAX;
				return x;
			}

			SizeType pseudoEffectiveNumber(SizeType nelectrons,SizeType jtilde) const
			{
				typename PsimagLite::Vector<SizeType>::Type v(3);
				v[0]=nelectrons;
				v[1]=0;
				v[2]=jtilde;
				return pseudoQuantumNumber(v);
			}

			void setToProduct(
					const HamiltonianSymmetrySu2& symm1,
					const HamiltonianSymmetrySu2& symm2,
					int pseudoQn,
					const typename PsimagLite::Vector<SizeType>::Type& electrons1,
     					const typename PsimagLite::Vector<SizeType>::Type& electrons2,
					typename PsimagLite::Vector<SizeType>::Type& electrons,
					typename PsimagLite::Vector<SizeType>::Type& quantumNumbers)
			{
				SizeType ns = symm1.jmValues_.size();
				SizeType ne = symm2.jmValues_.size();

				JmSubspaceType::setToProduct(&symm1,&symm2,electrons1,electrons2);

				findAllowedJm(symm1,symm2,electrons1,electrons2,pseudoQn);;
				createFactors(ns,ne);
				setFlavors(quantumNumbers);
				assert(quantumNumbers.size()==(ns*ne));
			
				jMax_=0;
				jmValues_.maxFirst<std::greater<SizeType> >(jMax_);
				jMax_++;
				calcReducedBasis();
				normalizeFlavors();
				setElectrons(electrons,quantumNumbers);
				electronsMax_ = *(std::max_element(
						electrons.begin(),electrons.end()));
		//				electrons[utils::vectorMax<SizeType,std::greater<SizeType> >(electrons,0)];
			}

			PairType getJmValue(SizeType alpha) const
			{
				return jmValues_[alpha];
			}

			SizeType getFlavor(SizeType alpha) const
			{
				return flavors_[alpha];
			}

			SizeType flavorsMax() const { return flavorsMax_; }

			SizeType electronsMax() const { return electronsMax_; }

			SizeType jMax() const { return jMax_; }

			template<typename SolverParametersType>
			void calcRemovedIndices(typename PsimagLite::Vector<SizeType>::Type& removedIndices,
						typename PsimagLite::Vector<RealType>::Type& eigs,
						SizeType kept,
						const SolverParametersType& solverParams) const
			{
//				normalizeFlavors();

				// we sort the eigenvalues
				// note: eigenvalues are not ordered because DensityMatrix is diagonalized in blocks
				typename PsimagLite::Vector<SizeType>::Type perm(eigs.size());
				PsimagLite::Sort<typename PsimagLite::Vector<RealType>::Type > sort;
				sort.sort(eigs,perm);
				
				if (eigs.size()<=kept) return;
				SizeType target = eigs.size()-kept;

				removedIndices.clear();

				if (solverParams.options.find("inflate")!=PsimagLite::String::npos)
					inclusiveRemoval(removedIndices,perm,eigs,target);
				else exclusiveRemoval(removedIndices,perm,eigs,target);
			}

			const FactorsType& getFactors() const 
			{
				return factors_;
			}

			void reorder(const typename PsimagLite::Vector<SizeType>::Type& permutationVector)
			{
				// reorder jmValues
				jmValues_.reorder(permutationVector);

				// reorder flavors
				utils::reorder(flavors_,permutationVector);
				utils::reorder(flavorsOld_,permutationVector);
			}

			void truncate(const typename PsimagLite::Vector<SizeType>::Type& removedIndices,
			              const typename PsimagLite::Vector<SizeType>::Type& electrons)
			{
				electronsMax_= * (std::max_element(
						electrons.begin(),electrons.end()));
				//electrons[utils::vectorMax<SizeType,std::greater<SizeType> >(electrons,0)];

				utils::truncateVector(flavors_,removedIndices);
				flavorsMax_=* (std::max_element(
						flavors_.begin(),flavors_.end()));
				//flavors_[utils::vectorMax<SizeType,std::greater<SizeType> >(flavors_,0)];

				jmValues_.truncate(removedIndices);
				jMax_=0;
				jmValues_.maxFirst<std::greater<SizeType> >(jMax_);
				jMax_++;
				calcReducedBasis();
			}

			SizeType size() const {return jmValues_.size(); }

			template<typename IoInputter>
			void load(IoInputter& io) 
			{
				jmValues_.load(io); 
				io.read(flavors_,"#su2flavors");
				io.readline(flavorsMax_,"#su2FlavorsMax=");
				io.readline(electronsMax_,"#su2ElectronsMax=");
				io.readline(jMax_,"#su2JMax=");
				io.read(statesReduced_,"#su2StatesReduced");
				io.read(jvals_,"#su2Jvals");
			}

			template<typename IoOutputter>
			void save(IoOutputter& io) const
			{
				jmValues_.save(io);
				io.printVector(flavors_,"#su2flavors");
				PsimagLite::String s="#su2FlavorsMax=" + ttos(flavorsMax_)+"\n";
				io.print(s); 
				s="#su2ElectronsMax="+ttos(electronsMax_)+"\n";
				io.print(s); 
				s="#su2JMax="+ttos(jMax_)+"\n";
				io.print(s);
				io.printVector(statesReduced_,"#su2StatesReduced");
				io.printVector(jvals_,"#su2Jvals");
			}

			SizeType flavor2Index(SizeType f1,SizeType f2,SizeType ne1,SizeType ne2,SizeType j1,SizeType j2) const
			{
				return JmSubspaceType::flavor(f1,f2,ne1,ne2,j1,j2);
				
			}

			void flavor2Index(PsimagLite::Map<SizeType,SizeType>::Type& flavorsOldInverse, const PairType& jm) const
			{
				for (SizeType i=0;i<flavorsOld_.size();i++) {
					if (jmValues_[i]!=jm) continue;
					flavorsOldInverse[flavorsOld_[i]]=i;
				}
				
			}

			const typename PsimagLite::Vector<SizeType>::Type& flavorsOld() const
			{
				return flavorsOld_;
				
			}

			// reduced:
			SizeType reducedIndex(SizeType i) const { return statesReduced_[i]; }

			SizeType reducedSize() const { return statesReduced_.size(); }

			SizeType jVals(SizeType i) const { return jvals_[i]; }

			SizeType jVals() const { return jvals_.size(); }

		private:

			template<typename JmSubspaceType>
			SizeType  setFlavors(typename PsimagLite::Vector<SizeType>::Type& quantumNumbers,JmSubspaceType& jmSubspace,SizeType offset)
			{
				// order is important here, electrons must be set after quantumNumbers
				SizeType flavors = jmSubspace.numberOfFlavors();
				if (offset==0) {
					quantumNumbers.clear();
					jmValues_.clear();
					flavors_.clear();
				}
				for (SizeType i=0;i<flavors;i++ ) {
					PairType jm = jmSubspace.getJmValue();
					quantumNumbers.push_back(neJmToIndex(jmSubspace.getNe(),jm));
					jmValues_.push(jm,i+offset);
					flavors_.push_back(jmSubspace.getFlavor(i));
				}
				offset += flavors;

				return offset;
			}

			void setElectrons(typename PsimagLite::Vector<SizeType>::Type& electrons,const typename PsimagLite::Vector<SizeType>::Type& qns)
			{
				electrons.resize(qns.size());
				for (SizeType i=0;i<qns.size();i++) {
					typename PsimagLite::Vector<SizeType>::Type v = decodeQuantumNumber(qns[i]);
					electrons[i]=v[0] + v[1];
				}
			}

			void normalizeFlavors()
			{
				flavorsOld_=flavors_;
				typename PsimagLite::Vector<SizeType>::Type perm(flavors_.size());
				PsimagLite::Sort<typename PsimagLite::Vector<SizeType>::Type > sort;
				sort.sort(flavors_,perm);

				SizeType counter=0;
				SizeType flavorSaved=flavors_[0];
				typename PsimagLite::Vector<SizeType>::Type flavorsTmp(flavors_.size());

				for (SizeType i=0;i<flavors_.size();i++) {
					if (flavorSaved!=flavors_[i]) {
						counter++;
						flavorSaved=flavors_[i];
					}
					flavorsTmp[i]=counter;
				}

				for (SizeType i=0;i<flavors_.size();i++) 
					flavors_[perm[i]]=flavorsTmp[i];

				flavorsMax_=counter+1;

			}

			void setFlavors(typename PsimagLite::Vector<SizeType>::Type& quantumNumbers) 
			{
				SizeType offset=0;
				for (SizeType i=0;i<jmSubspaces_.size();i++) {
					offset = setFlavors(quantumNumbers,jmSubspaces_[i],offset);
					jmSubspaces_[i].clear();
				}
			}

			// note: j is actually 2j and m is actually m+j
			// note: this is so that j and m are both always SizeType
			void findAllowedJm(const ThisType& symm1,const ThisType& symm2,const typename PsimagLite::Vector<SizeType>::Type& electrons1,
					   const typename PsimagLite::Vector<SizeType>::Type& electrons2,int pseudoQn) 
			{
				SizeType ns = symm1.jmValues_.size();
				SizeType ne = symm2.jmValues_.size();

				jmSubspaces_.clear();
				for (SizeType i=0;i<ns;i++) {
					PairType jm1 = symm1.getJmValue(i);
					for (SizeType j=0;j<ne;j++) {
						PairType jm2 = symm2.getJmValue(j);
						SizeType nelectrons = electrons1[i]+electrons2[j];
						addAllowedJms(jm1,jm2,i,j,ns,nelectrons,pseudoQn);
					}
				}
			}
			
			void createFactors(SizeType ns,SizeType ne)
			{
				VerySparseMatrixType factors(ns*ne);
				SizeType offset=0;
				for (SizeType i=0;i<jmSubspaces_.size();i++) {
					SizeType s=0;
					s= jmSubspaces_[i].createFactors(factors,offset);
					offset += s;
				}
				if (factors.nonZero()==0) {
					for (SizeType i=0;i<jmSubspaces_.size();i++) {
						std::cerr<<"subspace number "<<i;
						SizeType nelectrons=jmSubspaces_[i].getNe();
						std::cerr<<" nelectrons="<<nelectrons;
						PairType jm = jmSubspaces_[i].getJmValue();
						std::cerr<<" pseudo="<<pseudoEffectiveNumber(nelectrons,jm.first);
						
						std::cerr<<" jm=("<<jm.first<<","<<jm.second<<")\n";
						std::cerr<<" heavy="<<jmSubspaces_[i].heavy()<<"\n";
						std::cerr<<"--------------------------------------------\n";
					}
					throw PsimagLite::RuntimeError("HSSU2.h::createFactors(): factors are empty\n");
				}
				factors.sort();
				factors_ = factors;
			}

			// note: j is actually 2j and m is actually m+j
			// note: this is so that j and m are both always SizeType
			void addAllowedJms(const PairType& jm1,const PairType& jm2,SizeType alpha,SizeType beta,SizeType ns,SizeType nelectrons,
					int pseudoQn)
			{
				int j1 = jm1.first, j2=jm2.first;
				int jinitial = j1-j2;
				if (jinitial<0) jinitial = -jinitial;
				for (int j=jinitial;j<=j1+j2;j++) {
					// go over all hurdles:
					// first hurdle (j1+j2+j is even)
					//if (alpha==6 && beta==0) std::cerr<<__LINE__<<"\n";
					if ((j1+j2+j) %2 !=0) continue;
					// calculate m
					// m = m1+m2+(j1+j2-j)/2
					// note: (j1+j2-j) is even
					// note: (j1+j2-j)>=0
					int m = jm1.second+ jm2.second + int((-j1-j2+j)/2);
					// second hurdle |2m-j| <= j
					int tmp = 2*m-j;
					if (tmp<0) tmp= -tmp;
					if (tmp>j) continue;
					PairType jm(j,m);
					int heavy=1;
					if (pseudoQn>=0 &&  pseudoEffectiveNumber(nelectrons,jm.first) !=SizeType(pseudoQn)) {
						heavy=0;
					}
					addJmPair(alpha+beta*ns,jm1,jm2,jm,nelectrons,heavy);
				}
			}

			void addJmPair(SizeType index,const PairType& jm1,const PairType& jm2,const PairType& jm,SizeType nelectrons,int heavy)
			{
				std::pair<PairType,SizeType> triplet;
				triplet.first=jm;
				triplet.second=nelectrons;
				int x = PsimagLite::isInVector(jmSubspaces_,triplet);
				if (x<0) { // add new jmSubspace
					JmSubspace<RealType,VerySparseMatrixType,ThisType> jmSubspace(jm,index,jm1,jm2,nelectrons,heavy);
					jmSubspaces_.push_back(jmSubspace);
				} else {
					jmSubspaces_[x].push(index,jm1,jm2,nelectrons);
				}
			}

			void inclusiveRemoval(typename PsimagLite::Vector<SizeType>::Type& removedIndices,
					      const typename PsimagLite::Vector<SizeType>::Type& perm,
					      const typename PsimagLite::Vector<RealType>::Type& eigs,
					      SizeType target) const
			{
				typename PsimagLite::Vector<SizeType>::Type permInverse(perm.size());
				for (SizeType i=0;i<permInverse.size();i++) permInverse[perm[i]]=i;

				for (SizeType i=0;i<target;i++) {
					if (PsimagLite::isInVector(removedIndices,perm[i])>=0) continue;
					removedIndices.push_back(perm[i]);
				}

				for (SizeType i=0;i<target;i++)	{
					for (SizeType j=0;j<eigs.size();j++) {
						
						if (flavors_[j]==flavors_[perm[i]] && jmValues_[j].first==jmValues_[perm[i]].first) {
							int x = PsimagLite::isInVector(removedIndices,j);
							if (x<0) {
								typename PsimagLite::Vector<SizeType>::Type::iterator p1 =
										find(removedIndices.begin(),removedIndices.end(),perm[i]);
								if (p1==removedIndices.end()) continue;
								removedIndices.erase(p1);
								if (fabs(eigs[permInverse[j]]-eigs[i])>1e-6) {
									std::cerr<<"ind="<<perm[i]<<" j="<<permInverse[j];
									std::cerr<<" e[ind]="<<eigs[i];
									std::cerr<<" e[j]="<<eigs[permInverse[j]]<<"\n";
									std::cerr<<"flavor="<<flavors_[j];
									std::cerr<<" jm=("<<jmValues_[j].first<<",";
									std::cerr<<jmValues_[j].second<<") ";
									std::cerr<<" jm[ind]=(";
									std::cerr<<jmValues_[perm[i]].first<<",";
									std::cerr<<jmValues_[perm[i]].second<<")\n";
								}
								break;
							}
						}
					}
				}
			}

			void exclusiveRemoval(typename PsimagLite::Vector<SizeType>::Type& removedIndices,
					      const typename PsimagLite::Vector<SizeType>::Type& perm,
					      const typename PsimagLite::Vector<RealType>::Type& eigs,
					      SizeType target) const
			{
				typename PsimagLite::Vector<SizeType>::Type permInverse(perm.size());
				for (SizeType i=0;i<permInverse.size();i++) permInverse[perm[i]]=i;

				for (SizeType i=0;i<target;i++) {
					if (removedIndices.size()>=target) break;
					if (PsimagLite::isInVector(removedIndices,perm[i])>=0) continue;
						removedIndices.push_back(perm[i]);

					for (SizeType j=0;j<eigs.size();j++) {

						if (flavors_[j]==flavors_[perm[i]] && jmValues_[j].first==jmValues_[perm[i]].first) {
							int x = PsimagLite::isInVector(removedIndices,j);
							if (x<0) {
								removedIndices.push_back(j);
								if (fabs(eigs[permInverse[j]]-eigs[i])>1e-6) {
									std::cerr<<"ind="<<perm[i]<<" j="<<permInverse[j];
									std::cerr<<" e[ind]="<<eigs[i];
									std::cerr<<" e[j]="<<eigs[permInverse[j]]<<"\n";
									std::cerr<<"flavor="<<flavors_[j]<<" jm=";
									std::cerr<<jmValues_[j].first<<" ";
									std::cerr<<jmValues_[j].second<<" ";
									std::cerr<<" jm[ind]=";
									std::cerr<<jmValues_[perm[i]].first<<" ";
									std::cerr<<jmValues_[perm[i]].second<<"\n";
								}
							}
						}
					}
				}
			}

			void calcReducedBasis()
			{
				jvals_.clear();
				statesReduced_.clear();
				for (SizeType i1=0;i1<jmValues_.size();i1++) {
					PairType jm1 = jmValues_[i1];
					if (jm1.first!=jm1.second) continue;
					statesReduced_.push_back(i1);
					int x = PsimagLite::isInVector(jvals_,jm1.first);
					if (x<0) jvals_.push_back(jm1.first);
				}
			}

			JmPairsType jmValues_;
			typename PsimagLite::Vector<SizeType>::Type flavors_,flavorsOld_;
			SizeType flavorsMax_,electronsMax_,jMax_;
			FactorsType factors_;
			typename PsimagLite::Vector<JmSubspaceType>::Type jmSubspaces_;
			// reduced:
			typename PsimagLite::Vector<SizeType>::Type statesReduced_;
			typename PsimagLite::Vector<SizeType>::Type jvals_;
	}; //class HamiltonianSymmetrySu2
} // namespace Dmrg

/*@}*/
#endif
