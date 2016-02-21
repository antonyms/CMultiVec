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

#include <string>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/program_options.hpp>
#include <boost/iostreams/device/mapped_file.hpp>


#include <mlpack/core.hpp>
#include <mlpack/methods/kmeans/kmeans.hpp>

#include "mlpack/cosinesqrkernel.hpp" 

#ifdef ENABLE_HALITE
#include "halite/include/HaliteClustering.h"
#include "halite/include/PointSource.h"
#endif

namespace po=boost::program_options;
namespace km=mlpack::kmeans;
namespace hl=Halite;

enum ClusterAlgos {
  SphericalKMeans,
  HaliteAlgo
};

int cluster_contexts(ClusterAlgos algorithm, std::string& contextdir,const std::string& clusterdir, size_t numclust, int vecdim) {
  for (boost::filesystem::directory_iterator itr(contextdir); itr!=boost::filesystem::directory_iterator(); ++itr) {
    std::string path=itr->path().string();
    if(!boost::algorithm::ends_with(path,".vectors")) {
      continue;
    }
    if(boost::filesystem::file_size(itr->path())==0) {
      continue;
    }
    std::cout << path << '\n';
    boost::iostreams::mapped_file_source file(itr->path());
    size_t numpoints=file.size()/(vecdim*sizeof(float));
    std::cout << numpoints << " points" <<std::endl;
    const arma::fmat data((float*)file.data(), vecdim, numpoints, false,true);

    numclust=std::min(numpoints,numclust);

    arma::Col<size_t> assignments(numpoints);
		
    arma::fmat centroids(vecdim,numclust);

    if(algorithm == SphericalKMeans) {
      km::KMeans<CosineSqrKernel> k;

      k.Cluster(data, numclust, assignments,centroids);

      boost::filesystem::path outpath=clusterdir / itr->path().filename();
      outpath=outpath.replace_extension(".centers.txt");
      
      std::ofstream clusterfile(outpath.string());
      for(unsigned int i=0; i<numclust; i++) {
	for(int j=0; j<vecdim; j++) {
	  clusterfile << centroids(j,i) << " ";
	}
	clusterfile << '\n';
      }
      clusterfile.close();
    } else if(algorithm == HaliteAlgo) {
#ifndef ENABLE_HALITE
      std::cerr<<"Error: Attempted to use Halite clustering when it was disabled at compile time\n";
      exit(1);
#else
    hl::PackedArrayPointSource<float> pts(data.memptr(), vecdim, numpoints);
    hl::HaliteClustering<float> h(pts, hl::NormalizationMode::Independent, (2*vecdim), -1, 1e-10, 4, 1, 1, DB_HASH, 0);
    h.findCorrelationClusters();
    shared_ptr<hl::Classifier<float> > classifier=h.getClassifier();
    std::shared_ptr<hl::Normalization<float>> normalization=classifier->normalization;
    std::vector<float> denormMin(vecdim), denormMax(vecdim);
    
    boost::filesystem::path outpath=clusterdir / itr->path().filename();
    outpath=outpath.replace_extension(".halite.txt");
    std::ofstream clusterfile(outpath.string());
    for(const hl::BetaCluster<float>& b: classifier->betaClusters) {
      normalization->denormalize(b.min.begin(), denormMin.begin());
      normalization->denormalize(b.max.begin(), denormMax.begin());

      clusterfile << b.correlationCluster<<"\n";
      for(unsigned char c: b.relevantDimension) {
	clusterfile << (c?"1 ":"0 ");
      }
      clusterfile<<"\n";
      for(float f:denormMin) {
	clusterfile << f << " ";
      }
      clusterfile <<"\n";
      for(float f:denormMax) {
	clusterfile << f << " ";
      }
      clusterfile<<"\n";
    }
    clusterfile.close();
#endif
    }

  }
  return 0;
}


int main(int argc, char** argv) {
  std::string contextdir;
  std::string clusterdir;
  size_t numclust;
  unsigned int dim;
	
  po::options_description desc("CClusterContexts Options");
  desc.add_options()
    ("help,h", "produce help message")
    ("kmeans,k", "use spherical k-means clustering (default)")
#ifdef ENABLE_HALITE
    ("halite,l", "use Halite clustering")
#else
    ("halite,l", "use Halite clustering [DISABLED AT COMPILE TIME]")
#endif
    ("contexts,i", po::value<std::string>(&contextdir)->value_name("<directory>")->required(), "directory of contexts to cluster")
    ("clusters,o", po::value<std::string>(&clusterdir)->value_name("<directory>")->required(), "directory to output clusters")
    ("numclust,n", po::value<size_t>(&numclust)->value_name("<number>")->default_value(10),"number of clusters")
    ("dim,d", po::value<unsigned int>(&dim)->value_name("<number>")->default_value(50),"word vector dimension")
    ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }
  if(vm.count("kmeans") && vm.count("halite")) {
    std::cerr << "Error: Only one clustering algorithm can be selected\n";
    return 0;
  }

  ClusterAlgos algorithm=SphericalKMeans;
  if(vm.count("halite")) {
    algorithm=HaliteAlgo;
  }
       
  try {
    po::notify(vm);
  } catch(po::required_option& exception) {
    std::cerr << "Error: " << exception.what() << "\n";
    std::cout << desc << "\n";
    return 1;
  }
  if(!boost::filesystem::is_directory(contextdir)) {
    std::cerr << "Context directory does not exist" <<std::endl;
    return 2;
  }
  if(!boost::filesystem::is_directory(clusterdir)) {
    std::cerr << "Cluster directory does not exist" <<std::endl;
    return 3;
  }
  return cluster_contexts(algorithm, contextdir, clusterdir, numclust, dim);
}
