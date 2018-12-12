---
title: Introduction
---

# Introduction

Tree-sitter is a parser generator tool and an incremental parsing library. It can build a concrete syntax tree for a source file and efficiently update the syntax tree as the source file is edited. Tree-sitter aims to be:

* **General** enough to parse any programming language
* **Fast** enough to parse on every keystroke in a text editor
* **Robust** enough to provide useful results even in the presence of syntax errors
* **Dependency-free** so that the runtime library (which is written in pure C) can be embedded in any application

### Language Bindings

There are currently bindings that allow Tree-sitter to be used from the following languages:

* [JavaScript](https://github.com/tree-sitter/node-tree-sitter)
* [Rust](https://github.com/tree-sitter/rust-tree-sitter)
* [Haskell](https://github.com/tree-sitter/haskell-tree-sitter)
* [Ruby](https://github.com/tree-sitter/ruby-tree-sitter)

### Available Parsers

Parsers for these languages are fairly complete:

* [Bash](https://github.com/tree-sitter/tree-sitter-bash)
* [C](https://github.com/tree-sitter/tree-sitter-c)
* [C++](https://github.com/tree-sitter/tree-sitter-cpp)
* [CSS](https://github.com/tree-sitter/tree-sitter-css)
* [ERB / EJS](https://github.com/tree-sitter/tree-sitter-embedded-template)
* [Go](https://github.com/tree-sitter/tree-sitter-go)
* [HTML](https://github.com/tree-sitter/tree-sitter-html)
* [JavaScript](https://github.com/tree-sitter/tree-sitter-javascript)
* [OCaml](https://github.com/tree-sitter/tree-sitter-ocaml)
* [PHP](https://github.com/tree-sitter/tree-sitter-php)
* [Python](https://github.com/tree-sitter/tree-sitter-python)
* [Ruby](https://github.com/tree-sitter/tree-sitter-ruby)
* [Rust](https://github.com/tree-sitter/tree-sitter-rust)
* [TypeScript](https://github.com/tree-sitter/tree-sitter-typescript)

Parsers for these languages are in development:

* [Agda](https://github.com/tree-sitter/tree-sitter-agda)
* [C-sharp](https://github.com/tree-sitter/tree-sitter-c-sharp)
* [Haskell](https://github.com/tree-sitter/tree-sitter-haskell)
* [Java](https://github.com/tree-sitter/tree-sitter-java)
* [Julia](https://github.com/tree-sitter/tree-sitter-julia)
* [Scala](https://github.com/tree-sitter/tree-sitter-scala)

### Talks on Tree-sitter

* [FOSDEM 2018](https://www.youtube.com/watch?v=0CGzC_iss-8)
* [GitHub Universe 2017](https://www.youtube.com/watch?v=a1rC79DHpmY)

### Underlying Research

The design of Tree-sitter was greatly influenced by the following research papers:

- [Practical Algorithms for Incremental Software Development Environments](https://www2.eecs.berkeley.edu/Pubs/TechRpts/1997/CSD-97-946.pdf)
- [Context Aware Scanning for Parsing Extensible Languages](http://www.umsec.umn.edu/publications/Context-Aware-Scanning-Parsing-Extensible)
- [Efficient and Flexible Incremental Parsing](http://ftp.cs.berkeley.edu/sggs/toplas-parsing.ps)
- [Incremental Analysis of Real Programming Languages](https://pdfs.semanticscholar.org/ca69/018c29cc415820ed207d7e1d391e2da1656f.pdf)
- [Error Detection and Recovery in LR Parsers](http://what-when-how.com/compiler-writing/bottom-up-parsing-compiler-writing-part-13)
- [Error Recovery for LR Parsers](http://www.dtic.mil/dtic/tr/fulltext/u2/a043470.pdf)
