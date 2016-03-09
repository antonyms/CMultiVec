#include "common.hpp"
namespace po=boost::program_options;

void add_eod_option(boost::program_options::options_description& desc, std::string* eodmarker) {
  desc.add_options()
    ("eodmarker",po::value<std::string>(eodmarker)->value_name("<string>")->default_value("eeeoddd"),"end of document marker");
}
void add_context_options(po::options_description& desc, std::string* ssmarker, std::string* esmarker) {
  desc.add_options()
    ("ssmarker", po::value<std::string>(ssmarker)->value_name("<string>")->default_value("<s>"),"start of sentence fill token")
    ("esmarker", po::value<std::string>(esmarker)->value_name("<string>")->default_value("</s>"), "end sentence fill token")
     ;
}
void add_indexing_options(po::options_description& desc, std::string* oovtoken, std::string* digit_rep) {
  desc.add_options()
    ("oovtoken", po::value<std::string>(oovtoken)->value_name("<string>")->default_value("UUUNKKK"), "OOV token")
    ("digify", po::value<std::string>(digit_rep)->value_name("<string>")->implicit_value("DG"), "digify OOV numbers by replacing all digits with the given string")
    ;
}

static std::regex numregex("[-+]?\\d*\\.?\\d+", std::regex::ECMAScript | std::regex::optimize);
static std::regex digitregex("\\d", std::regex::ECMAScript | std::regex::optimize);


int read_index(const std::string& index, int vocabsize) {
  int result=std::stoi(index);
  if(result<0 || result>=vocabsize) {
    throw std::out_of_range("Out of vocab range");
  }
  return result;
}

int lookup_word(const boost::unordered_map<std::string, int>& vocabmap, const std::string& word, bool preindexed, int oovind, boost::optional<const std::string&> digit_rep) {
  if(preindexed) {
    return read_index(word, vocabmap.size());
  } else {
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
}

void compute_context(const boost::circular_buffer<int>& context, const std::vector<float>& idfs, const arma::fmat&  origvects, arma::fvec& outvec, unsigned int vecdim, unsigned int contextsize) {

	float idfc[2*contextsize+1]; //idf context window

	//Look up the idfs of the words in context
	std::transform(context.begin(),context.end(),idfc,[&idfs](int c) ->float {return idfs[c];});

	float idfsum=std::accumulate(idfc,&idfc[contextsize],0.0f)+std::accumulate(&idfc[contextsize+1],&idfc[2*contextsize+1],0.0f);
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
