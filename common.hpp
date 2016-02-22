/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Jeremy Salwen nor the name of any other
 *    contributor may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Jeremy Salwen AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL Jeremy Salwen OR ANY OTHER
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef COMMON_H
#define COMMON_H
#include "boost/circular_buffer.hpp"
#include "boost/unordered_map.hpp"

#include <armadillo>

enum ClusterAlgos {
  SphericalKMeans,
  HaliteAlgo
};

int lookup_word(const boost::unordered_map<std::string, int>& vocabmap, const std::string& word, bool indexed) {
	if(indexed) {
		return std::stoi(word)-1;
	} else {
		boost::unordered_map<std::string,int>::const_iterator index=vocabmap.find(word);
		if(index==vocabmap.end()) {
			return 0;  //Unknown words are mapped to 0, so the first word in your vocab better be unknown
		}
		return index->second;}
}

void compute_context(const boost::circular_buffer<int>& context, const std::vector<float>& idfs, const arma::fmat&  origvects, arma::fvec& outvec, unsigned int vecdim, unsigned int contextsize) {
	
	float idfc[2*contextsize+1]; //idf context window

	//Look up the idfs of the words in context
	std::transform(context.begin(),context.end(),idfc,[&idfs](int c) ->float {return idfs[c];});

	float idfsum=std::accumulate(idfc,&idfc[contextsize],0)+std::accumulate(&idfc[contextsize+1],&idfc[2*contextsize+1],(float)0);
	if(idfsum==0) {
		return;
	}
	float invidfsum=1/idfsum; 

	for(unsigned int i=0; i<contextsize; i++) {
		float idfterm=idfc[i]*invidfsum;
		std::transform(outvec.begin(), outvec.end(),origvects.unsafe_col(context[i]).begin(), outvec.begin(),  [idfterm](float f1, float f2) -> float { return f1+f2*idfterm; });
	}
	for(unsigned int i=contextsize+1; i<2*contextsize+1; i++) {
		float idfterm=idfc[i]*invidfsum;
		std::transform(outvec.begin(), outvec.end(),origvects.unsafe_col(context[i]).begin(), outvec.begin(),  [idfterm](float f1, float f2) -> float { return f1+f2*idfterm; });
	}
}
#endif
