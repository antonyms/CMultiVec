# Overview
CMultiVec is a (currenly incomplete) set of tools for training vector 
representations of words, with multiple prototypes per word.  It is 
based on Huang Socher 2012[1].  It is designed to be as fast as 
possible.

It includes

* CExtractContexts: A tool for extracting context representations from a large corpus 
* CClusterContexts: A tool for clustering large numbers of context representations
* CExpandVocab: A tool for generating vocabulary files and the data needed to relabel a corpus
* CRelabelCorpus: A tool for relabeling a corpus based on the context of the words.

The goal is to make multi-protype representations more accessible.

### Requirements
* C++11
* Boost (filesystem, program options, iostreams)
* MLPack[2]

#Tools
There are four tools, which work as a pipeline.  For a full listing of 
the command line options, just run any of the tools with the -h option.

The data formats section below will also help you understand the inputs 
and outputs of these tools

##CExtractContexts
CExtractContexts creates a Context Directory from a corpus.  By default 
it will open a file for every word in the vocabulary. For a vocabulary 
with 100,000 words, this means you will likely need to modify your 
permissions in /etc/security/limits.conf to allow you to open more files 
simultaneously. Adding a line like

    bob     hard    nofile     200000

and logging back in should do the trick.

If that is not possible, or you are having trouble with memory usage, 
you can limit the number of files CExtractContexts will open 
simultaneously using the --fcachesize option.  However, note that this 
can significantly slow things down.

You can partition the corpus and run multiple copies of CExtractContexts 
at the same time outputting to separate Context Directories.  To merge 
the context directories, simply concatenate the respective .vector files 
together.


##CClusterContexts
CClusterContexts looks at each word in the vocabulary and forms clusters 
of the extracted contexts.  These clusters are the different "senses" of 
the word.

CClusterContexts currently implements spherical K-Means clustering using 
the MLPack library, as well as the Halite Clustering algorithm using 
HaliteClustering which is included as a git submodule.

##CExpandVocab
CExpandVocab uses the clustering generated by CClusterContexts to expand 
the vocabulary into the new expanded vocabulary file, which contains one 
entry for every "sense" of a word.

##CRelabelCorpus
CRelabelCorpus uses the clustering generated by CCLusterContexts to 
relabel a corpus with the new expanded vocabulary file.

# Data formats

## Vocabulary File
Text file, one word per line.  CMultiVec has hardcoded in that <s> and 
</s> are inserted before and after every document, so make sure that 
they are included in your vocab file.

## idf File
Text file, one floating point value per line.  This will be the inverse 
document frequency weighting of the word in the corresponding line of 
the vocab file.  See https://en.wikipedia.org/wiki/Tf%E2%80%93idf for 
more information.

## Vectors File
Text File, D whitespace-separated floating point values per line, where 
D is the dimension of the word representation vectors.  Each line 
corresponds to the representation of the same line in the vocab.

## Corpus Directory
Directory containing an arbitrary number of .txt files.  All of them 
will be processed.

## Context Directory
Binary files named N.vectors which contain the contexts of the Nth word in 
the vocabulary. Contains a list of tfidf-weighted context vectors.  Each 
vector is D IEEE-754 floats. The vectors are just concatenated and there 
is no padding.

## Clusters Directory
Directory containing text files N.*.txt which contain the clusters 
generated from the contexts of the Nth word in the vocabulary.  

Depending on the clustering mode, they will be in different formats.

If using the kmeans clustering mode: N.centers.txt will have on each 
line a whitespace separated vector, representing the center of one of 
the clusters.

If using the halite clustering mode: N.hlclusters.txt will be a sequence 
of "Beta Clusters". Each Beta Cluster will list (whitespace and newline 
separated)

  Correlation Cluster number\n
  Vector of relevance\n
  Vector of lower bounds\n
  Vector of upper bounds\n

Where each correlation cluster may be composed of multiple beta clusters.

## Expanded Vocab file

The expanded vocab file is just the same as the normal vocab file, 
except the different sense-clusters of a word are different entries.  
The cluster number is just prepended as a two digit number to the entry.  
CMultivec tools depend on the expanded vocab to be in the same order as 
the original vocab, and the clusters to be in numerical order.  For 
example, it might look like

````
00dog
01dog
00cat
00jello
01jello
02jello
````

## Centers File
Text file, each line containing a whitespace separated vector.  Each 
vector is the center of the corresponding line in the expanded 
vocabulary file.  For example, if (0,1.2, 5) is on the line 
corresponding to 04bagel, then (0,1.2,5) is the center of the the 4th 
cluster of the contexts of "bagel".

# Citations
````
@inproceedings{HuangEtAl2012,
author = {Eric H. Huang and Richard Socher and Christopher D. Manning and Andrew Y. Ng},
title = {Improving Word Representations via Global Context and Multiple Word Prototypes},
booktitle = {Annual Meeting of the Association for Computational Linguistics (ACL)},
year = 2012
}

@article{mlpack2013,
  title     = {{MLPACK}: A Scalable {C++} Machine Learning Library},
  author    = {Curtin, Ryan R. and Cline, James R. and Slagle, Neil P. and
               March, William B. and Ram, P. and Mehta, Nishant A. and Gray,
               Alexander G.},
  journal   = {Journal of Machine Learning Research},
  volume    = {14},
  pages     = {801--805},
  year      = {2013}
}
````
