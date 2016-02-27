/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Jeremy Salwen nor the name of any other
 *    contributor may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Jeremy Salwen AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL Jeremy Salwen OR ANY OTHER
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef COMMON_H
#define COMMON_H
#include "boost/circular_buffer.hpp"
#include "boost/unordered_map.hpp"
#include "boost/optional.hpp"
#include <boost/program_options.hpp>
#include <regex>

#include <armadillo>


enum ClusterAlgos {
  SphericalKMeans,
  HaliteAlgo
};


void add_eod_option(boost::program_options::options_description& desc, std::string* eodmarker);
void add_context_options(boost::program_options::options_description& desc, std::string* ssmarker, std::string* esmarker);
void add_indexing_options(boost::program_options::options_description& desc, std::string* oovtoken, std::string* digit_rep);



int read_index(const std::string& index, int vocabsize);


int lookup_word(const boost::unordered_map<std::string, int>& vocabmap, const std::string& word, bool preindexed, int oovind, boost::optional<const std::string&> digit_rep);

void compute_context(const boost::circular_buffer<int>& context, const std::vector<float>& idfs, const arma::fmat&  origvects, arma::fvec& outvec, unsigned int vecdim, unsigned int contextsize);

#endif
