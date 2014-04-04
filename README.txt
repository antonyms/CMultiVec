CMultiVec is a (currenly incomplete) set of tools for training vector representations 
of words, with multiple prototypes per word.  It is based on Huang Socher 2012.  It 
is designed to be as fast as possible.

It includes

1. A tool for extracting context representations from a large corpus (complete)
2. A tool for clustering large numbers of context representations (incomplete)
3. A tool for re-labelling words in context with their appropriate representation (incomplete)

The goal is to make multi-protype representations more accessible.

Currently the only requirement is Boost and C++11.  However, it will open up a file for 
every word in the vocabulary.  For a vocabulary with 100,000 words, this means you will 
likely need to modify your permissions in /etc/security/limits.conf to allow you to 
open more files simultaneously. Addding a line like

    bob     hard    nofile     200000

and logging back in should do the trick.


Citations:

@inproceedings{HuangEtAl2012,
author = {Eric H. Huang and Richard Socher and Christopher D. Manning and Andrew Y. Ng},
title = {Improving Word Representations via Global Context and Multiple Word Prototypes},
booktitle = {Annual Meeting of the Association for Computational Linguistics (ACL)},
year = 2012
}

