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
#include <iomanip>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>
#include <boost/unordered_map.hpp>

#include <armadillo>

namespace po=boost::program_options;
namespace fs=boost::filesystem;

enum ClusterAlgos {
  SphericalKMeans,
  HaliteAlgo
};
int expand_vocab(ClusterAlgos format, fs::ifstream& vocabin, fs::ofstream& vocabout, fs::ofstream& ocenterstream, const std::string& clusterdir, int dim) {

  fs::path clusterpath(clusterdir);

  std::string word;
  unsigned int index=0;
  while(getline(vocabin,word)) {
    std::stringstream ss;
    if(format==SphericalKMeans) {
      ss << index << ".centers.txt";
      fs::path cfile=clusterpath / ss.str();
		
      int defn=0;
      if(fs::exists(cfile)) {
	fs::ifstream clusterfile(cfile);
	std::string clustervec;
	while(getline(clusterfile,clustervec)) {
	  vocabout << std::setfill ('0') << std::setw (2) << defn++ <<word <<'\n';
	  ocenterstream << clustervec << '\n';
	}
      } else {
	vocabout <<  std::setfill ('0') << std::setw (2) << 0 <<word <<'\n';
	for(int i=0; i<dim; i++) { //Fill with zeros if there are no clusters
	  ocenterstream << "0 ";
	}
	ocenterstream << '\n';
      }
    } else if(format==HaliteAlgo) {
      ss << index << ".hlclusters.txt";
      fs::path cfile=clusterpath / ss.str();
		
      int defn=0;
      if(fs::exists(cfile)) {
	fs::ifstream clusterfile(cfile);
	std::string line;
	while(getline(clusterfile,line)) {
	  vocabout << std::setfill ('0') << std::setw (2) << defn++ <<word <<'\n';

	  //Output the word number and cluster number
	  ocenterstream << index << '\n';
	  //Output the cluster number
	  ocenterstream << line << '\n';
	  //Output the relevance vector
	  getline(clusterfile,line);
	  ocenterstream << line << '\n';
	  //Output the min vector
	  getline(clusterfile,line);
	  ocenterstream << line << '\n';
	  //Output the max vector
	  getline(clusterfile,line);
	  ocenterstream << line << '\n';
	}
      } else {
	vocabout <<  std::setfill ('0') << std::setw (2) << 0 <<word <<'\n';
	for(int i=0; i<dim; i++) { //Fill with zeros if there are no clusters
	  ocenterstream << "0 ";
	}
	ocenterstream << '\n';
      }
 
    }
    index++;
  }
  return 0;
}

int main(int argc, char** argv) {
	
  std::string vocabf;
  std::string ovocabf;
  std::string vecf;
  std::string ocenterf;
  std::string clusterdir;
  unsigned int dim;
	
  po::options_description desc("CExpandVocab Options");
  desc.add_options()
    ("help,h", "produce help message")
    ("kmeans,k", "use spherical k-means clustering (default)")
    ("halite,l", " use Halite clustering")
    ("ivocab,v", po::value<std::string>(&vocabf)->value_name("<filename>")->required(), "original vocab file")
    ("ovocab", po::value<std::string>(&ovocabf)->value_name("<filename>")->required(), "output vocab file")
    ("centers", po::value<std::string>(&ocenterf)->value_name("<filename>")->required(), "output cluster centers")
    ("clusters,c", po::value<std::string>(&clusterdir)->value_name("<directory>")->required(), "clusters directory")
    ("dim,d", po::value<unsigned int>(&dim)->value_name("<number>")->default_value(50),"word vector dimension")
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

  if(vm.count("kmeans") && vm.count("halite")) {
    std::cerr << "Error: Only one cluster format (kmeans, halite) can be selected\n";
    return 2;
  }

  ClusterAlgos format=SphericalKMeans;
  if(vm.count("halite")) {
   format=HaliteAlgo;
  }
  fs::ifstream ivocab(vocabf);
  if(!ivocab.good()) {
    std::cerr << "Input vocab file no good" <<std::endl;
    return 3;
  }
  fs::ofstream ovocab(ovocabf);
  if(!ovocab.good()) {
    std::cerr << "Output vocab file no good" <<std::endl;
    return 4;
  }
  fs::ofstream ocenter(ocenterf);
  if(!ocenter.good()) {
    std::cerr << "Output context mapping file no good" <<std::endl;
    return 5;
  }
  if(!fs::is_directory(clusterdir)) {
    std::cerr << "Cluster directory does not exist" <<std::endl;
    return 6;
  }


  return expand_vocab(format, ivocab,ovocab, ocenter, clusterdir, dim);
}
