# Overview
CMultiVec is a (currenly incomplete) set of tools for training vector 
representations of words, with multiple prototypes per word.  It is 
based on Huang Socher 2012[1].  It is designed to be as fast as 
possible.

It includes

CExtractContexts: A tool for extracting context representations from a 
large corpus (complete) 
CClusterContexts: A tool for clustering large 
numbers of context representations (incomplete)
XXXX: A tool for 
re-labelling words in context with their appropriate representation 
(incomplete)

The goal is to make multi-protype representations more accessible.

### Requirements
* C++11
* Boost (filesystem, program options)

### Optional

* MLPack[2]

##CExtractContexts
CExtractContexts will open a file for every word in the vocabulary. For 
a vocabulary with 100,000 words, this means you will likely need to 
modify your permissions in /etc/security/limits.conf to allow you to 
open more files simultaneously. Adding a line like

    bob     hard    nofile     200000

and logging back in should do the trick.

# Data formats

## Vocabulary File
Text file, one word per line.

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
Binary files named N.bin which contain the contexts of the Nth word in 
the vocabulary. Contains a list of tfidf-weighted context vectors.  Each 
vector is D IEEE-754 floats. The vectors are just concatenated and there 
is no padding.


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
