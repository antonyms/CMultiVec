// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of Jeremy Salwen nor the name of any other
//    contributor may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY Jeremy Salwen AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL Jeremy Salwen OR ANY OTHER
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include <iostream>
#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/unordered_map.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>

#include <armadillo>

#include "common.hpp"
#include "mlpack/cosinesqrkernel.hpp"

namespace po=boost::program_options;
namespace fs=boost::filesystem;

int convert_word(int index, const boost::circular_buffer<int>& context, const std::vector<float>& idfs, const arma::fmat&  origvects,const arma::fmat& newvects, std::vector<unsigned int>& xref, unsigned int vecdim, unsigned int contextsize) {
	arma::fvec c;
	compute_context(context, idfs,origvects, c,vecdim,contextsize);
	unsigned int starti=xref[index];
	unsigned int endi=xref[index+1];

	int nclust=endi-starti;
	std::vector<float> sim(endi-starti);
	int max=0;
	for(int i=0; i<nclust; i++) {
		sim[i]=CosineSqrKernel().Evaluate(newvects.unsafe_col(starti+i),c);
	}
	return starti+max;
}

int relabel_corpus(fs::ifstream& vocabstream,fs::ifstream& newvocabstream,fs::ifstream& idfstream, fs::ifstream& vecstream, fs::ifstream& newvecstream, fs::path& icorpus, fs::path& ocorpus, unsigned int vecdim, unsigned int contextsize, std::string eodmarker, bool indexed) {

	boost::unordered_map<std::string, int> vocabmap;
	std::vector<std::string> vocab;
	std::vector<float> idfs;
	arma::fmat origvects(vecdim, 5);

	std::vector<unsigned int> crossreference;
	arma::fmat newvects(vecdim,5);
	
	unsigned int index=0;
	unsigned int xrefindex=0;
	std::string word;
	std::string newword;
	getline(newvocabstream,newword);
	while(getline(vocabstream,word)) {
		vocab.push_back(word);
		vocabmap[word]=index;
		
		float idf;
		idfstream >> idf;
		idfs.push_back(idf);
		
		if(index>=origvects.n_cols) { 
			origvects.resize(vecdim, origvects.n_cols*2);
		}
		for(unsigned int i=0; i<vecdim; i++) {
			vecstream >> origvects(i,index);
		}
		
		crossreference.push_back(xrefindex);
		do {
			if(xrefindex>=newvects.n_cols) {
				newvects.resize(vecdim,newvects.n_cols*2);
			}
			for(unsigned int i=0; i<vecdim; i++) {
				newvecstream >>newvects(i,xrefindex);
			}
			getline(newvocabstream,newword);
			xrefindex++;
		} while(boost::algorithm::ends_with(newword,word));
		index++;
	}

	int startdoci=lookup_word(vocabmap,"<s>",false);
	int enddoci=lookup_word(vocabmap,"<\\s>",false);
	
	for (boost::filesystem::directory_iterator itr(icorpus); itr!=boost::filesystem::directory_iterator(); ++itr) {
		if(itr->path().extension()!=".txt") {
			continue;
		}

		fs::ifstream corpusreader(itr->path());
		if(!corpusreader.good()) {
			return 7;
		}
		fs::ofstream corpuswriter(ocorpus / itr->path().filename());
		if(!corpusreader.good()) {
			return 8;
		}
			std::cout << "Reading corpus file " << itr->path() << std::endl;

		//Keeps track of the accumulated contexts of the previous 5 words, the current word, and the next 5 words
		boost::circular_buffer<int > context(2*contextsize+1);

		do {
			context.clear();
			for(unsigned int i=0; i<contextsize; i++) {
				context.push_back(startdoci);
			}
			std::string word;
			for(unsigned int i=0; i<contextsize+1; i++) {
				if(getline(corpusreader,word)) {
					if(word==eodmarker) goto EOD;
					int wind=lookup_word(vocabmap,word,indexed);
					context.push_back(wind);
				}
			}

			while(getline(corpusreader,word)) {
				if(word==eodmarker) goto EOD;
				
				int newword=convert_word(context[contextsize], context,  idfs, origvects, newvects, crossreference, vecdim, contextsize);
				context.pop_front();
				int newind=lookup_word(vocabmap,word,indexed);
				context.push_back(newind);
			}
			EOD:
			unsigned int k=0;
			while(context.size()<2*contextsize+1) {
					context.push_back(enddoci);
					k++;
			}
			for(; k<contextsize; k++) {
				
				std::vector<float> outvec(vecdim);
				int newword=convert_word(context[contextsize], context,  idfs, origvects, newvects, crossreference, vecdim, contextsize);
				context.pop_front();
				context.push_back(enddoci);
			}
		} while(!corpusreader.eof());
	}
	return 0;
}

int main(int argc, char** argv) {
	std::string vocabf;
	std::string expandedvocabf;
	std::string idff;
	std::string vecf;
	std::string newvecf;
	std::string icorpusf;
	std::string ocorpusf;

	unsigned int vecdim;
	unsigned int contextsize;
	std::string eod;
	
	po::options_description desc("CRelabelCorpus Options");
	desc.add_options()
    ("help,h", "produce help message")
    ("oldvocab,v", po::value<std::string>(&vocabf)->value_name("<filename>")->required(), "original vocab file")
	("newvocab,e", po::value<std::string>(&expandedvocabf)->value_name("<filename>")->required(), "new vocabulary file")
	("idf,f", po::value<std::string>(&idff)->value_name("<filename>")->required(), "original idf file")
	("oldvec,w", po::value<std::string>(&vecf)->value_name("<filename>")->required(), "original word vectors")
	("newvec,n", po::value<std::string>(&newvecf)->value_name("<filename>")->required(), "new word vectors")
	("icorpus,i", po::value<std::string>(&icorpusf)->value_name("<directory>")->required(), "input corpus")
	("ocorpus,o", po::value<std::string>(&ocorpusf)->value_name("<directory>")->required(), "output relabeled corpus")
	("dim,d", po::value<unsigned int>(&vecdim)->value_name("<number>")->default_value(50), "dimension of word vectors")
	("contextsize,s", po::value<unsigned int>(&contextsize)->value_name("<number>")->default_value(5),"size of context (# of words before and after)")
	("eodmarker,e",po::value<std::string>(&eod)->value_name("<string>")->default_value("eeeoddd"),"end of document marker")
	("preindexed","indicates the corpus is pre-indexed with the vocab file")
	;

	
	po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

	if (vm.count("help")) {
            std::cout << desc << "\n";
            return 0;
   }
	try {
		po::notify(vm);
	} catch(po::required_option& exception) {
		std::cerr << "Error: " << exception.what() << "\n";
		std::cout << desc << "\n";
        return 1;
	}
	
	fs::ifstream oldvocab(vocabf);
	if(!oldvocab.good()) {
		std::cerr << "Original vocab file no good" <<std::endl;
		return 2;
	}
	
	fs::ifstream newvocab(expandedvocabf);
	if(!newvocab.good()) {
		std::cerr << "New vocab file no good" <<std::endl;
		return 3;
	}
		
	fs::ifstream idf(idff);
	if(!newvocab.good()) {
		std::cerr << "Original idf file no good" <<std::endl;
		return 4;
	}
	fs::ifstream vectors(vecf);
	if(!vectors.good()) {
		std::cerr << "Original vectors file no good" <<std::endl;
		return 5;
	}
	fs::ifstream newvectors(newvecf);
	if(!vectors.good()) {
		std::cerr << "New vectors file no good" <<std::endl;
		return 6;
	}
	fs::path icorpus(icorpusf);
	if(!fs::is_directory(icorpus)) {
		std::cerr << "Input corpus directory does not exist" <<std::endl;
		return 7;
	}
	fs::path ocorpus(ocorpusf);
	if(!fs::is_directory(ocorpus)) {
		std::cerr << "Output corpus directory does not exist" <<std::endl;
		return 8;
	}
	return relabel_corpus(oldvocab,newvocab,idf,vectors,newvectors,icorpus,ocorpus,vecdim,contextsize, eod, vm.count("preindexed")>0);
}