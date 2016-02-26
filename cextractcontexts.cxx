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
#include <queue>

#include <sys/resource.h>

#include <boost/filesystem.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/unordered_map.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/program_options.hpp>

#include "common.hpp"

namespace po=boost::program_options;

struct FileCacheEntry;

struct FileCacheEntry {
  std::list<FileCacheEntry*>::iterator iterator;
  FILE* file=NULL;
  bool initialized=false;
};

class CachingFileArray {
public:
  CachingFileArray(std::function<std::string (size_t)> f_namer, size_t numFiles, size_t cachesize): cachesize(cachesize), file_namer(f_namer), queuesize(0), entries(numFiles)  {

  }

  FILE* getFile(size_t id) {
   
    FileCacheEntry& e=entries[id];
    if(e.file!=NULL) { //cache hit
      //Move the current file to the front of the queue
      if(e.iterator!=queue.begin()) {
	queue.splice(queue.begin(), queue, e.iterator);
      }
      return e.file;
    } else { //cache miss
      if(queuesize >= cachesize) {
	closeOldestFile();
      }
      return openFile(id);
    }
  }

  void closeAll() {
    while(closeOldestFile());
  }
  
protected:
  FILE* openFile(size_t id) {
    FileCacheEntry& e=entries[id];
    std::string fname=file_namer(id);

    e.file = fopen(fname.c_str(), e.initialized?"ab":"wb");
    
    if(e.file==NULL) {
      return NULL;
    }
    queue.push_front(&e);
    queuesize++;
    e.iterator=queue.begin();

    e.initialized=true;
    
    return e.file;
  }
  
  bool closeOldestFile() {
    if(!queue.empty()) {
      FileCacheEntry* e = queue.back();
      queue.pop_back();
      queuesize--;
      
      fclose(e->file);
      e->file=NULL;
      return true;
    } else {
      return false;
    }
}
    

    size_t cachesize;
std::function<std::string (size_t)> file_namer;
std::list<FileCacheEntry*> queue;
size_t queuesize;
std::vector<FileCacheEntry> entries;
};

int compute_and_output_context(const boost::circular_buffer<int>& context, const std::vector<float>& idfs, const arma::fmat& origvects, CachingFileArray& outfiles,unsigned int vecdim, unsigned int contextsize, int prune) {
	int midid=context[contextsize]; 
	if(midid>=prune) {
		return 0;
	}
	arma::fvec out(vecdim,arma::fill::zeros);
	compute_context(context, idfs,origvects,out,vecdim,contextsize);

	FILE* fout=outfiles.getFile(midid);
	
	if(fout==NULL) {
	  std::cerr<<"Error opening file #"<< midid << std::endl;
	  return 9;
	}				    

	//now tot will contain the context representation of the middle vector
	size_t n = fwrite(out.memptr(),sizeof(float),vecdim, fout);
	if(n != vecdim) {
	  std::cerr<< "Error writing to file #"<<midid<<std::endl;
	  return 10;
	}
	
	return 0;
}

int extract_contexts(std::ifstream& vocabstream, std::ifstream& tfidfstream, std::ifstream& vectorstream, std::string indir, std::string outdir, int vecdim, unsigned int contextsize, std::string eodmarker, std::string ssmarker, std::string esmarker, unsigned int prune, unsigned int fcachesize) {
  boost::unordered_map<std::string, int> vocabmap;
  std::vector<std::string> vocab;
  std::vector<float> idfs;
  arma::fmat origvects(vecdim,5);
	
  std::string word;
  unsigned int index=0;
  while(getline(vocabstream,word)) {
    vocab.push_back(word);
    vocabmap[word]=index;

    float tf;
    tfidfstream >>tf;
    idfs.push_back(tf);

    if(index>=origvects.n_cols) {
      origvects.resize(origvects.n_rows,origvects.n_cols*2);
    }
		
    for(int i=0; i<vecdim; i++) {
      vectorstream>>origvects(i,index);
    }
    index++;
  }

  vocabstream.close();
  tfidfstream.close();
  vectorstream.close();

  int startdoci, enddoci;

  try {
    startdoci = vocabmap.at(ssmarker);
  }catch(std::out_of_range& e) {
    std::cerr<<"Error: Start of sentence fill marker is not in the vocabulary.\n";
    return 5;
  }
  try {
    enddoci = vocabmap.at(esmarker);
  }catch(std::out_of_range& e) {
    std::cerr<<"Error: End of sentence fill marker is not in the vocabulary.\n";
    return 6;
  }
	
  unsigned int vsize=vocab.size();

  if(prune && prune <vsize) {
    vsize=prune;
  }
  if(fcachesize==0) {
    fcachesize=vsize;
  }
  //set limit of open files high enough to open a file for every word in the dictionary.
  rlimit lim;
  lim.rlim_cur=fcachesize+1024; //1024 extra files just to be safe;
  lim.rlim_max=fcachesize+1024;
  setrlimit(RLIMIT_NOFILE , &lim);

  CachingFileArray outfiles(
			    [&outdir](size_t i) {
			      std::ostringstream s;
			      s<<outdir<<"/"<< i << ".vectors";
			      return s.str();
			    },
			    vsize, fcachesize);

  try {
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
      boost::circular_buffer<int> context(2*contextsize+1);

      do {
	context.clear();
	for(unsigned int i=0; i<contextsize; i++) {
	  context.push_back(startdoci);
	}
	std::string word;
	for(unsigned int i=0; i<contextsize; i++) {
	  if(getline(corpusreader,word)) {
	    if(word == eodmarker) goto EOD;
	    int wind=read_index(word, vocab.size());				  
	    context.push_back(wind);
	  }
	}

	while(getline(corpusreader,word)) {
	  if(word == eodmarker) goto EOD;
	  int newind=read_index(word,vocab.size());
	  context.push_back(newind);
	  int retcode=compute_and_output_context(context, idfs, origvects,outfiles,vecdim,contextsize,vsize);
	  if(retcode) return retcode;
			  
	  context.pop_front();
	}
      EOD:
	unsigned int k=0;
	while(context.size()<2*contextsize+1) {
	  context.push_back(enddoci);
	  k++;
	}
	for(; k<contextsize; k++) {
	  int retcode = compute_and_output_context(context, idfs, origvects,outfiles,vecdim,contextsize,vsize);
	  if(retcode) return retcode;
		    
	  context.pop_front();
	  context.push_back(enddoci);
	}
      } while(!corpusreader.eof());
    }
  } catch(std::invalid_argument& e) {
    std::cerr<<"Error, non-numerical line found in indexed corpus.\n  Please make sure your corpus is in the right format.\n";
    return 9;
  } catch(std::out_of_range& e) {
    std::cerr << "Error, found out of bounds index in indexed file.\n";
    return 10;
  }  

  std::cout << "Closing files" <<std::endl;
  /*
  // Not necessary, since exiting properly will clean up anyway (And is much faster)
  outfiles.closeAll();
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
  std::string ssmarker, esmarker, eod;

  unsigned int prune=0;
  unsigned int fcachesize=0;
  po::options_description desc("CExtractContexts Options");
  desc.add_options()
    ("help,h", "produce help message")
    ("vocab,v", po::value<std::string>(&vocabf)->value_name("<filename>")->required(), "vocab file")
    ("idf,i", po::value<std::string>(&idff)->value_name("<filename>")->required(), "idf file")
    ("vec,w", po::value<std::string>(&vecf)->value_name("<filename>")->required(), "word vectors file")
    ("corpus,c", po::value<std::string>(&corpusd)->value_name("<directory>")->required(), "corpus directory")
    ("outdir,o", po::value<std::string>(&outd)->value_name("<directory>")->required(), "directory to output contexts")
    ("dim,d", po::value<int>(&dim)->value_name("<number>")->default_value(50),"word vector dimension")
    ("contextsize,s", po::value<unsigned int>(&contextsize)->value_name("<number>")->default_value(5),"size of context (# of words before and after)")
    ("eodmarker,e",po::value<std::string>(&eod)->value_name("<string>")->default_value("eeeoddd"),"end of document marker")
    ("ssmarker", po::value<std::string>(&ssmarker)->value_name("<string>")->default_value("<s>"),"start of sentence fill marker")
    ("esmarker", po::value<std::string>(&esmarker)->value_name("<string>")->default_value("</s>"), "end sentence fill marker")
     ("prune,p",po::value<unsigned int>(&prune)->value_name("<number>"),"only output contexts for the first N words in the vocab")
    ("fcachesize,f", po::value<unsigned int>(&fcachesize)->value_name("<number>"), "maximum number of files to open at once");
		
	
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


    if(!boost::filesystem::is_directory(corpusd)) {
      std::cerr << "Input directory does not exist" <<std::endl;
      return 5;
    }

    if(!boost::filesystem::is_directory(outd)) {
      std::cerr << "Input directory does not exist" <<std::endl;
      return 6;
    }

    return extract_contexts(vocab, frequencies, vectors, corpusd, outd, dim, contextsize, eod, ssmarker, esmarker, prune, fcachesize);
}


