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
int expand_vocab(fs::ifstream& vocabin, fs::ofstream& vocabout,  fs::ifstream& vecstream, fs::ofstream& ocenterstream, const std::string& clusterdir) {

	fs::path clusterpath(clusterdir);

	std::string word;
	unsigned int index=0;
	while(getline(vocabin,word)) {
		std::string origvec;
		getline(vecstream,origvec);
		std::stringstream ss;
		ss << index << ".txt";
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
				std::string origvec;
				vocabout <<  std::setfill ('0') << std::setw (2) << defn <<word <<'\n';
				ocenterstream << origvec << '\n';
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
	
	po::options_description desc("CExpandVocab Options");
	desc.add_options()
    ("help,h", "produce help message")
    ("ivocab,v", po::value<std::string>(&vocabf)->value_name("<filename>")->required(), "original vocab file")
	("ovocab", po::value<std::string>(&ovocabf)->value_name("<filename>")->required(), "output vocab file")
	("vec,w", po::value<std::string>(&vecf)->value_name("<filename>")->required(), "original word vectors")
	("centers", po::value<std::string>(&ocenterf)->value_name("<filename>")->required(), "output cluster centers")
	("clusters,c", po::value<std::string>(&clusterdir)->value_name("<directory>")->required(), "clusters directory")
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
	
	fs::ifstream ivocab(vocabf);
	if(!ivocab.good()) {
		std::cerr << "Input vocab file no good" <<std::endl;
		return 2;
	}
	fs::ofstream ovocab(ovocabf);
	if(!ovocab.good()) {
		std::cerr << "Output vocab file no good" <<std::endl;
		return 3;
	}
	fs::ifstream vectors(vecf);
	if(!vectors.good()) {
		std::cerr << "Input vectors file no good" <<std::endl;
		return 3;
	}
	fs::ofstream ocenter(ocenterf);
	if(!ocenter.good()) {
		std::cerr << "Output context mapping file no good" <<std::endl;
		return 4;
	}
	if(!fs::is_directory(clusterdir)) {
		std::cerr << "Cluster directory does not exist" <<std::endl;
		return 5;
	}


	return expand_vocab(ivocab,ovocab, vectors, ocenter, clusterdir);
}