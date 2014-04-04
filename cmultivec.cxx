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

#include <sys/resource.h>

#include <boost/filesystem.hpp>
#include <boost/unordered_map.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/array.hpp>
#include <boost/algorithm/string/predicate.hpp>

#define VECDIM 50
#define CSIZE 5
#define EODWORD "eeeoddd"
typedef boost::array<float,VECDIM> wvec;

void compute_and_output_context(const boost::circular_buffer<int>& context, const std::vector<float>& idfs, const std::vector<wvec>& origvects, std::vector<FILE*>& outfiles) {
	float idfc[2*CSIZE+1]; //idf context window

	//Look up the idfs of the words in context
	std::transform(context.begin(),context.end(),idfc,[&idfs](int c) ->float {return idfs[c];});

	float idfsum=std::accumulate(idfc,&idfc[CSIZE],0)+std::accumulate(&idfc[CSIZE+1],&idfc[2*CSIZE+1],0);
	float invidfsum=1/idfsum;

	wvec tot;

	for(int i=0; i<CSIZE; i++) {
		wvec cont=origvects[context[i]];
		float idfterm=idfc[i]*invidfsum;
		std::transform(tot.begin(), tot.end(), cont.begin(), tot.begin(),  [idfterm](float f1, float f2) -> float { return f1+f2*idfterm; });
	}
	for(int i=CSIZE+1; i<2*CSIZE+1; i++) {
		wvec cont=origvects[context[i]];
		float idfterm=idfc[i]*invidfsum;
		std::transform(tot.begin(), tot.end(), cont.begin(), tot.begin(),  [idfterm](float f1, float f2) -> float { return f1+f2*idfterm; });
	}

	//now tot will contain the context representation of the middle vector
	int midid=context[CSIZE]; 
	fwrite(tot.begin(),sizeof(float),VECDIM,outfiles[midid]);
}

int lookup_word(const boost::unordered_map<std::string, int>& vocabmap, const std::string& word) {
	boost::unordered_map<std::string,int>::const_iterator index=vocabmap.find(word);
	if(index==vocabmap.end()) {
		return 0;  //Unknown words are mapped to 0, so the first word in your vocab better be unknown
	}

	return index->second;
}

int extract_contexts(std::ifstream& vocabstream, std::ifstream& tfidfstream, std::ifstream& vectorstream, std::string indir, std::string outdir) {
	boost::unordered_map<std::string, int> vocabmap;
	std::vector<std::string> vocab;
	std::vector<float> idfs;
	std::vector<wvec> origvects;

	std::string word;
	int index=0;
	while(getline(vocabstream,word)) {
		vocab.push_back(word);
		vocabmap[word]=index++;

		float tf;
		tfidfstream >>tf;
		idfs.push_back(tf);

		wvec v;
		for(int i=0; i<VECDIM; i++) {
			vectorstream>>v[i];
		}
		origvects.push_back(v);
	}

	vocabstream.close();
	tfidfstream.close();
	vectorstream.close();


	int startdoci=lookup_word(vocabmap,"<s>");
	int enddoci=lookup_word(vocabmap,"<\\s>");


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
		boost::circular_buffer<int > context(2*CSIZE+1);

		do {
			context.clear();
			for(int i=0; i<CSIZE; i++) {
				context.push_back(startdoci);
			}
			std::string word;
			for(int i=0; i<CSIZE+1; i++) {
				if(getline(corpusreader,word)) {
					if(word==EODWORD) goto EOD;
					int wind=lookup_word(vocabmap,word);
					context.push_back(wind);
				}
			}

			while(getline(corpusreader,word)) {
				if(word==EODWORD) goto EOD;
				compute_and_output_context(context, idfs, origvects,outfiles);
				context.pop_front();
				int newind=lookup_word(vocabmap,word);
				context.push_back(newind);
			}
			EOD:
			for(int i=0; i<CSIZE; i++) {
				if(context.size()==2*CSIZE+1) {
					compute_and_output_context(context, idfs, origvects,outfiles);
					context.pop_front();
				}
				context.push_back(enddoci);
			}
		} while(!corpusreader.eof());
	}
	std::cout << "Closing files" <<std::endl;
	for(unsigned int i=0; i<vocab.size(); i++) {
		if(i%1000==0) std::cout<< "Closing file " << i <<std::endl;
		fclose(outfiles[i]);
	}
	return 0;
}

void print_usage()
{
	std::cout<< "Usage: CMultiVec vocab.txt frequencies.tfidf original.vectors inputdif outputdir" <<std::endl;
}


int main(int argc, char** argv) {
	if(argc!=6) {
		print_usage();
		return 1;
	}

	std::ifstream vocab(argv[1]);
	if(!vocab.good()) {
		std::cerr << "Vocab file no good" <<std::endl;
		return 2;
	}

	std::ifstream frequencies(argv[2]);
	if(!frequencies.good()) {
		std::cerr << "Frequencies file no good" <<std::endl;
		return 3;
	}

	std::ifstream vectors(argv[3]);
	if(!vectors.good()) {
		std::cerr << "Vectors file no good" <<std::endl;
		return 4;
	}

	std::string indir(argv[4]);
	if(!boost::filesystem::is_directory(indir)) {
		std::cerr << "Input directory does not exist" <<std::endl;
		return 5;
	}

	std::string outdir(argv[5]);
	if(!boost::filesystem::is_directory(outdir)) {
		std::cerr << "Input directory does not exist" <<std::endl;
		return 6;
	}

	return extract_contexts(vocab, frequencies, vectors, indir,outdir);
}


