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

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

namespace po=boost::program_options;
int expand_vocab(std::ifstream& vocabin, const std::string& clusterdir, std::ofstream& vocabout) {
	return 0;
}
int main(int argc, char** argv) {
	
	std::string vocabf;
	std::string clusterdir;
	std::string outvocab;
	
	po::options_description desc("CExpandVocab Options");
	desc.add_options()
    ("help,h", "produce help message")
    ("vocab,v", po::value<std::string>(&vocabf)->value_name("<filename>")->required(), "vocab file")
	("clusters,c", po::value<std::string>(&clusterdir)->value_name("<directory>")->required(), "clusters directory")
	("outvocab,o", po::value<std::string>(&outvocab)->value_name("<filename>")->required(), "expanded vocab output file")
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
	
	std::ifstream vocab(vocabf);
	if(!vocab.good()) {
		std::cerr << "Vocab file no good" <<std::endl;
		return 2;
	}

	if(!boost::filesystem::is_directory(clusterdir)) {
		std::cerr << "Cluster directory does not exist" <<std::endl;
		return 3;
	}
	
	std::ofstream ovocab(outvocab);
	if(!ovocab.good()) {
		std::cerr << "Output vocab file no good" <<std::endl;
		return 4;
	}
	return expand_vocab(vocab,clusterdir,ovocab);
}