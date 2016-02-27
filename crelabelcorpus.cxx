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

#include "mlpack/cosinesqrkernel.hpp"
#include <armadillo>

#include "common.hpp"

#ifdef ENABLE_HALITE
#include "Classifier.h"

namespace hl=Halite;
#endif

namespace po=boost::program_options;
namespace fs=boost::filesystem;



class SphericalKMeansClassifier {
public:
  SphericalKMeansClassifier(size_t vecdim): numcenters(0), centers(vecdim,5) {
  }

  void addCenters(std::string& word, std::string newword, std::ifstream& newvocabstream, std::ifstream& centerstream) {

    crossreference.push_back(numcenters);
    do {
      if(numcenters>=centers.n_cols) {
	centers.resize(centers.n_rows,centers.n_cols*2);
      }
      for(unsigned int i=0; i<centers.n_rows; i++) {
	centerstream >>centers(i,numcenters);
      }
      getline(newvocabstream, newword);
      numcenters++;
    } while(boost::algorithm::ends_with(newword,word));
    
  }
  
  int convertWord(const boost::circular_buffer<int>& context, const std::vector<float>& idfs, const arma::fmat&  origvects, unsigned int contextsize) {
    int index=context[contextsize];
    arma::fvec c(centers.n_rows, arma::fill::zeros);
    compute_context(context, idfs, origvects, c, centers.n_rows, contextsize);
    unsigned int starti=crossreference[index];
    unsigned int endi=crossreference[index+1];

    int nclust=endi-starti;
    std::vector<float> sim(nclust);

    for(int i=0; i<nclust; i++) {
      sim[i]=CosineSqrKernel().Evaluate(centers.unsafe_col(starti+i),c);
    }
	
    return distance(sim.begin(), max_element(sim.begin(), sim.end()));
  }


  std::vector<unsigned int> crossreference;
  size_t numcenters;
  arma::fmat centers;
};
#ifdef ENABLE_HALITE
class HaliteClassifier {
public:
  HaliteClassifier(size_t vecdim): vecdim(vecdim) {
  }
  void addClusters(size_t index, size_t& nextclusteridx, std::ifstream& haliteclustersfile) {
    classifiers.emplace_back();
    hl::Classifier<float>& cl=classifiers.back();
    cl.hardClustering=1;
    
    while(nextclusteridx == index) {
      cl.betaClusters.emplace_back(0, vecdim);
      hl::BetaCluster<float>& newcluster=cl.betaClusters.back();

      //Get correlation cluster;
      haliteclustersfile >> newcluster.correlationCluster;

      //Get relevance vectors
      for(size_t i=0; i<vecdim; i++) {
	haliteclustersfile >> newcluster.relevantDimension[i];
      }

      //Get relevance vectors
      for(size_t i=0; i<vecdim; i++) {
	haliteclustersfile >> newcluster.min[i];
      }

      //Get relevance vectors
      for(size_t i=0; i<vecdim; i++) {
	haliteclustersfile >> newcluster.max[i];
      }

      haliteclustersfile>>nextclusteridx;
    }
  }
  int convertWord(const boost::circular_buffer<int>& context, const std::vector<float>& idfs, const arma::fmat& origvects,  unsigned int contextsize) {
    int index=context[contextsize];
  
    arma::fvec c(vecdim, arma::fill::zeros);
    compute_context(context, idfs, origvects, c, vecdim, contextsize);

    hl::Classifier<float>& classifier = classifiers[index];
    size_t cluster = 0;
    classifier.assignToClusters(c.memptr(), &cluster);
    return cluster;
  }
  
  size_t vecdim;
  std::vector<hl::Classifier<float>> classifiers;
};
#endif

int relabel_corpus(ClusterAlgos format, fs::ifstream& vocabstream,fs::ifstream& newvocabstream,fs::ifstream& idfstream, fs::ifstream& vecstream, fs::ifstream& centerstream, fs::path& icorpus, fs::path& ocorpus, unsigned int vecdim, unsigned int contextsize, std::string eodmarker, std::string ssmarker, std::string esmarker) {

  boost::unordered_map<std::string, int> vocabmap;
  std::vector<std::string> vocab;
  std::vector<float> idfs;
  arma::fmat origvects(vecdim, 5);

  std::unique_ptr<SphericalKMeansClassifier> kmeans;
	
#ifdef ENABLE_HALITE
  std::unique_ptr<HaliteClassifier> halite;
#else
  std::cerr<<"Error: Attempted to use Halite clustering format when it was disabled at compile time\n";
  exit(1);	  
#endif
	
  if(format == SphericalKMeans) {
    kmeans = std::unique_ptr<SphericalKMeansClassifier>(new SphericalKMeansClassifier(vecdim));
  } else if(format == HaliteAlgo) {
#ifdef ENABLE_HALITE
    halite = std::unique_ptr<HaliteClassifier>(new HaliteClassifier(vecdim));
#endif
	  
  }

  unsigned int index=0;
  std::string word;
  std::string newword;
  getline(newvocabstream,newword);
  size_t nextclusteridx;
  centerstream >> nextclusteridx;
  while(getline(vocabstream,word)) {
    vocab.push_back(word);
		
    float idf;
    idfstream >> idf;
    idfs.push_back(idf);
		
    if(index>=origvects.n_cols) { 
      origvects.resize(vecdim, origvects.n_cols*2);
    }
    for(unsigned int i=0; i<vecdim; i++) {
      vecstream >> origvects(i,index);
    }
    if(format == SphericalKMeans) {
      kmeans->addCenters(word, newword, newvocabstream, centerstream);
    } else if(format == HaliteAlgo) {
#ifdef ENABLE_HALITE
      halite->addClusters(index, nextclusteridx, centerstream);
#endif
    }
    index++;
  }

  int startdoci, enddoci;

  try {
    startdoci = vocabmap.at(ssmarker);
  } catch(std::out_of_range& e) {
    std::cerr<<"Error: Start of sentence fill marker is not in the vocabulary.\n";
    return 5;
  }
  try {
    enddoci = vocabmap.at(esmarker);
  } catch(std::out_of_range& e) {
    std::cerr<<"Error: End of sentence fill marker is not in the vocabulary.\n";
    return 6;
  }
  try {
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
	for(unsigned int i=0; i<contextsize; i++) {
	  if(getline(corpusreader,word)) {
	    if(word == eodmarker) goto EOD;
	    int wind=read_index(word,vocab.size());

	    context.push_back(wind);
	  }
	}

	while(getline(corpusreader,word)) {
	  if(word == eodmarker) goto EOD;
	  int nextind=read_index(word, vocab.size());

	  context.push_back(nextind);

	  int wid=context[contextsize];
	  int meaning=0;

	  if(format == SphericalKMeans) {
	    meaning = kmeans->convertWord(context,  idfs, origvects, contextsize);
	  } else if(format == HaliteAlgo) {
#ifdef ENABLE_HALITE
	    meaning = halite->convertWord(context, idfs, origvects, contextsize);
#endif
	  }
				
	  corpuswriter <<  std::setfill ('0') << std::setw (2) << meaning << vocab[wid]<<'\n';
	  context.pop_front();
	}
      EOD:
	unsigned int k=0;
	while(context.size()<2*contextsize+1) {
	  context.push_back(enddoci);
	  k++;
	}
	for(; k<contextsize; k++) {
	  int wid=context[contextsize];
	  int meaning=0;

	  if(format == SphericalKMeans) {
	    meaning = kmeans->convertWord(context,  idfs, origvects, contextsize);
	  } else if(format == HaliteAlgo) {
#ifdef ENABLE_HALITE
	    meaning = halite->convertWord(context, idfs, origvects, contextsize);
#endif
	  }

	  corpuswriter <<  std::setfill ('0') << std::setw (2) << meaning << vocab[wid]<<'\n';
	  context.pop_front();
	  context.push_back(enddoci);
	}
      } while(!corpusreader.eof());
    }
  } catch(std::invalid_argument& e) {
    std::cerr<<"Error, non-numerical line found in indexed corpus.\n  Please make sure your corpus is in the right format.\n";
    return 9;
  } catch(std::out_of_range& e) {
    std::cerr << "Error, found out of bound index in indexed file.\n";
    return 10;
  }  
  return 0;
}

int main(int argc, char** argv) {
  std::string vocabf;
  std::string expandedvocabf;
  std::string idff;
  std::string vecf;
  std::string centersf;
  std::string icorpusf;
  std::string ocorpusf;

  unsigned int vecdim;
  unsigned int contextsize;
  std::string eod, ssmarker, esmarker;
  std::string digit_rep;
  po::options_description desc("CRelabelCorpus Options");
  desc.add_options()
    ("help,h", "produce help message")
    ("kmeans,k", "use spherical k-means clustering (default)")
    ("halite,l", " use Halite clustering")
    ("oldvocab,v", po::value<std::string>(&vocabf)->value_name("<filename>")->required(), "original vocab file")
    ("newvocab,e", po::value<std::string>(&expandedvocabf)->value_name("<filename>")->required(), "new vocabulary file")
    ("idf,f", po::value<std::string>(&idff)->value_name("<filename>")->required(), "original idf file")
    ("oldvec,w", po::value<std::string>(&vecf)->value_name("<filename>")->required(), "original word vectors")
    ("centers,c", po::value<std::string>(&centersf)->value_name("<filename>")->required(), "cluster centers file")
    ("icorpus,i", po::value<std::string>(&icorpusf)->value_name("<directory>")->required(), "input corpus")
    ("ocorpus,o", po::value<std::string>(&ocorpusf)->value_name("<directory>")->required(), "output relabeled corpus")
    ("dim,d", po::value<unsigned int>(&vecdim)->value_name("<number>")->default_value(50), "dimension of word vectors")
    ("contextsize,s", po::value<unsigned int>(&contextsize)->value_name("<number>")->default_value(5),"size of context (# of words before and after)")
    ;

	
  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }
  
  if(vm.count("kmeans") && vm.count("halite")) {
    std::cerr << "Error: Only one cluster format (kmeans, halite) can be selected\n";
    return 2;
  }

  ClusterAlgos format=SphericalKMeans;
  if(vm.count("halite")) {
   format=HaliteAlgo;
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
  fs::ifstream centers(centersf);
  if(!vectors.good()) {
    std::cerr << "Cluster centers file no good" <<std::endl;
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
  boost::optional<const std::string&> digit_rep_arg;
  if(!digit_rep.empty()) {
    digit_rep_arg=digit_rep;
  }
  return relabel_corpus(format, oldvocab,newvocab,idf,vectors,centers,icorpus,ocorpus,vecdim,contextsize, eod, ssmarker, esmarker);
}
