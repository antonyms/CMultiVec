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

#include <boost/unordered_map.hpp>

#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>

#include "mlpack/cosinesqrkernel.hpp"
#include <armadillo>

#include "common.hpp"

#include <regex>

namespace po=boost::program_options;
namespace fs=boost::filesystem;

int deindex_corpus(fs::ifstream& vocabstream, fs::path& icorpus, fs::path& ocorpus, std::string eodmarker) {
  std::vector<std::string> vocab;
  std::string word;
  while(getline(vocabstream,word)) {
    vocab.push_back(word);
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
 
      while(getline(corpusreader,word)) {
	if(word == eodmarker) {
	  corpuswriter << word<<"\n";
	  continue;
	}

	int ind=read_index(word,vocab.size());
	corpuswriter << vocab[ind]<<"\n";
      }
    }
  } catch(std::invalid_argument& e) {
    std::cerr<<"Error, non-numerical line found in indexed corpus.\n  Please make sure your corpus is in the right format.\n";
    return 9;
  } catch(std::out_of_range& e) {
    std::cerr << "Error, found out of bounds index in indexed file.\n";
    return 10;
  }  
  
  return 0;
}

static std::regex numregex("[-+]?\\d*\\.?\\d+", std::regex::ECMAScript | std::regex::optimize);
static std::regex digitregex("\\d", std::regex::ECMAScript | std::regex::optimize);

int lookup_word(const boost::unordered_map<std::string, int>& vocabmap, const std::string& word, int oovind, boost::optional<const std::string&> digit_rep) {
  boost::unordered_map<std::string,int>::const_iterator index = vocabmap.find(word);
  if(index != vocabmap.end()) {
    return index->second;
  }
    
  if(digit_rep.is_initialized()) {
    if(std::regex_match(word,numregex)) {
      std::string digified = std::regex_replace(word, digitregex, *digit_rep);
      index=vocabmap.find(digified);
      if(index !=vocabmap.end()){
	return index->second;
      }
    }
  }
  return oovind;
}


int index_corpus(fs::ifstream& vocabstream, fs::path& icorpus, fs::path& ocorpus, const std::string& unktoken, std::string eodmarker, boost::optional<const std::string&> digit_rep) {
  std::vector<std::string> vocab;
  boost::unordered_map<std::string, int> vocabmap;
  
  std::string word;
  int index=0;
  while(getline(vocabstream,word)) {
    vocab.push_back(word);
    vocabmap[word]=index++;
  }

  int oov;
  try {
    oov = vocabmap.at(unktoken);
  } catch(std::out_of_range& e) {
    std::cerr<<"Error: Unknown token marker not in the vocabulary";
    return 6;
  }

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
    

    while(getline(corpusreader,word)) {
      if(word == eodmarker) {
	corpuswriter << eodmarker <<"\n";
	continue;
      }
      int ind = lookup_word(vocabmap, word, oov, digit_rep); 
      corpuswriter << ind<<"\n";
    }
    
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

  std::string unktoken, eod;
  std::string digit_rep;
  po::options_description desc("CRelabelCorpus Options");
  desc.add_options()
    ("help,h", "produce help message")
    ("index,x","run in indexing mode")
    ("deindex,u","run in deindexing mode")
    ("vocab,v", po::value<std::string>(&vocabf)->value_name("<filename>")->required(), "original vocab file")
    ("icorpus,i", po::value<std::string>(&icorpusf)->value_name("<directory>")->required(), "input corpus")
    ("ocorpus,o", po::value<std::string>(&ocorpusf)->value_name("<directory>")->required(), "output relabeled corpus")
    ("oovtoken", po::value<std::string>(&unktoken)->value_name("<string>")->default_value("uuunkkk"), "OOV token")
    ("eodmarker", po::value<std::string>(&eod)->value_name("<string>")->default_value("eeeoddd"), "end of document marker")
    ("digify", po::value<std::string>(&digit_rep)->value_name("<string>")->default_value("DG"), "digify OOV numbers by replacing all digits with the given string") 
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

  if(vm.count("index") + vm.count("deindex") != 1) {
    std::cerr << "Error: You must choose exactly one of --index or --deindex\n";
  }
	
  fs::ifstream vocab(vocabf);
  if(!vocab.good()) {
    std::cerr << "Vocab file no good" <<std::endl;
    return 2;
  }
		
  fs::path icorpus(icorpusf);
  if(!fs::is_directory(icorpus)) {
    std::cerr << "Input corpus directory does not exist" <<std::endl;
    return 3;
  }
  fs::path ocorpus(ocorpusf);
  if(!fs::is_directory(ocorpus)) {
    std::cerr << "Output corpus directory does not exist" <<std::endl;
    return 4;
  }
  boost::optional<const std::string&> digit_rep_arg;
  if(!digit_rep.empty()) {
    digit_rep_arg=digit_rep;
  }

  if(vm.count("index")) {
    return index_corpus(vocab, icorpus, ocorpus, unktoken, eod, digit_rep_arg);
  } else {
    return deindex_corpus(vocab, icorpus, ocorpus, eod);
  }

}
