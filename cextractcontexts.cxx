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
#include <fstream>
#include <cstdio>
#include <algorithm>
#include <numeric>
#include <sstream>

#include <sys/resource.h>

#include <boost/filesystem.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/unordered_map.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/program_options.hpp>

#define EODWORD "eeeoddd"

namespace po=boost::program_options;

void compute_and_output_context(const boost::circular_buffer<int>& context, const std::vector<float>& idfs, const std::vector<float>& origvects, std::vector<FILE*>& outfiles,unsigned int vecdim, unsigned int contextsize) {
	float idfc[2*contextsize+1]; //idf context window

	//Look up the idfs of the words in context
	std::transform(context.begin(),context.end(),idfc,[&idfs](int c) ->float {return idfs[c];});

	float idfsum=std::accumulate(idfc,&idfc[contextsize],0)+std::accumulate(&idfc[contextsize+1],&idfc[2*contextsize+1],0);
	float invidfsum=1/idfsum;

	std::vector<float> tot(vecdim);

	for(unsigned int i=0; i<contextsize; i++) {
		float idfterm=idfc[i]*invidfsum;
		std::transform(tot.begin(), tot.end(),&origvects[context[i]*vecdim], tot.begin(),  [idfterm](float f1, float f2) -> float { return f1+f2*idfterm; });
	}
	for(unsigned int i=contextsize+1; i<2*contextsize+1; i++) {
		float idfterm=idfc[i]*invidfsum;
		std::transform(tot.begin(), tot.end(),&origvects[context[i]*vecdim], tot.begin(),  [idfterm](float f1, float f2) -> float { return f1+f2*idfterm; });
	}

	//now tot will contain the context representation of the middle vector
	int midid=context[contextsize]; 
	fwrite(&tot[0],sizeof(float),vecdim,outfiles[midid]);
}

int lookup_word(const boost::unordered_map<std::string, int>& vocabmap, const std::string& word, bool indexed) {
	if(indexed) {
		return std::stoi(word);
	} else {
		boost::unordered_map<std::string,int>::const_iterator index=vocabmap.find(word);
		if(index==vocabmap.end()) {
			return 0;  //Unknown words are mapped to 0, so the first word in your vocab better be unknown
		}
		return index->second;}
}

int extract_contexts(std::ifstream& vocabstream, std::ifstream& tfidfstream, std::ifstream& vectorstream, std::string indir, std::string outdir, int vecdim,unsigned int contextsize,std::string eodmarker, bool indexed) {
	boost::unordered_map<std::string, int> vocabmap;
	std::vector<std::string> vocab;
	std::vector<float> idfs;
	std::vector<float> origvects(5*vecdim);
	
	std::string word;
	unsigned int index=0;
	while(getline(vocabstream,word)) {
		vocab.push_back(word);
		vocabmap[word]=index++;

		float tf;
		tfidfstream >>tf;
		idfs.push_back(tf);

		if((index+1)*vecdim>origvects.size()) {
			origvects.resize(origvects.size()*2,vecdim);
		}
		
		for(int i=0; i<vecdim; i++) {
			vectorstream>>origvects[index*vecdim+i];
		}
	}

	vocabstream.close();
	tfidfstream.close();
	vectorstream.close();


	int startdoci=lookup_word(vocabmap,"<s>",false);
	int enddoci=lookup_word(vocabmap,"<\\s>",false);


	//set limit of open files high enough to open a file for every word in the dictionary.
	rlimit lim;
	lim.rlim_cur=vocab.size()+1024; //1024 extra files just to be safe;
	lim.rlim_max=vocab.size()+1024;
	setrlimit(RLIMIT_NOFILE , &lim);
	
	std::vector<FILE*> outfiles(vocab.size());
	for(unsigned int i=0; i<vocab.size(); i++) {
		std::ostringstream s;
		s<<outdir<<"/"<< i << ".vectors";
		outfiles[i]=fopen(s.str().c_str(),"w");
		if(outfiles[i]==NULL) {
			std::cerr<<"Error opening file "<<s.str() << std::endl;
			return 9;
		}
	}

	for (boost::filesystem::directory_iterator itr(indir); itr!=boost::filesystem::directory_iterator(); ++itr) {
		std::string path=itr->path().string();
		if(!boost::algorithm::ends_with(path,".txt")) {
			continue;
		}

		std::ifstream corpusreader(path.c_str());
		if(!corpusreader.good()) {
			return 7;
		}

		std::cout << "Reading corpus file " << path << std::endl;

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
				compute_and_output_context(context, idfs, origvects,outfiles,vecdim,contextsize);
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
				compute_and_output_context(context, idfs, origvects,outfiles,vecdim,contextsize);
				context.pop_front();
				context.push_back(enddoci);
			}
		} while(!corpusreader.eof());
	}
	std::cout << "Closing files" <<std::endl;
	/* Not necessary, since exiting properly will clean up anyway (And is much faster)
	for(unsigned int i=0; i<vocab.size(); i++) {
		if(i%1000==0) std::cout<< "Closing file " << i <<std::endl;
		fclose(outfiles[i]);
	}
	*/  
	return 0;
}


int main(int argc, char** argv) {

	std::string vocabf;
	std::string idff;
	std::string vecf;
	std::string corpusd;
	std::string outd;
	int dim;
	unsigned int contextsize;
	std::string eod;
	po::options_description desc("Command Line Options");
	desc.add_options()
    ("help,h", "produce help message")
    ("vocab,v", po::value<std::string>(&vocabf)->required(), "vocab file")
	("idf,i", po::value<std::string>(&idff)->required(), "idf file")
	("vec,w", po::value<std::string>(&vecf)->required(), "word vectors file")
	("corpus,c", po::value<std::string>(&corpusd)->required(), "corpus directory")
	("outdir,o", po::value<std::string>(&outd)->required(), "output directory")
	("dim,d", po::value<int>(&dim)->default_value(50),"word vector dimension")
	("contextsize,s", po::value<unsigned int>(&contextsize)->default_value(5),"size of context (# of words before and after)")
	("eodmarker,e",po::value<std::string>(&eod)->default_value("eeeoddd"),"end of document marker")
	("preindexed","indicates the corpus is pre-indexed with the vocab file");
		

	
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
	
	std::ifstream vocab(vocabf);
	if(!vocab.good()) {
		std::cerr << "Vocab file no good" <<std::endl;
		return 2;
	}

	std::ifstream frequencies(idff);
	if(!frequencies.good()) {
		std::cerr << "Frequencies file no good" <<std::endl;
		return 3;
	}

	std::ifstream vectors(vecf);
	if(!vectors.good()) {
		std::cerr << "Vectors file no good" <<std::endl;
		return 4;
	}

	std::string indir(corpusd);
	if(!boost::filesystem::is_directory(indir)) {
		std::cerr << "Input directory does not exist" <<std::endl;
		return 5;
	}

	std::string outdir(outd);
	if(!boost::filesystem::is_directory(outdir)) {
		std::cerr << "Input directory does not exist" <<std::endl;
		return 6;
	}

	return extract_contexts(vocab, frequencies, vectors, indir,outdir,dim,contextsize,eod,vm.count("preindexed")>0);
}


