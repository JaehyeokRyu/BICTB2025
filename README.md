# BIC_TB

Package for Offline TB 2025 Analysis

## Dependencies

All the dependencies can be sourced from CVMFS automatically by running compile (or environment set) script.

* ROOT
* python3
* YAML

## How to compile the package (required only once)
- Scripts for UBUNTU is prepared.
- source buildNinstall.sh

## How to compile the analysis scripts
**Caution) Works only after compiling the package or setting up the environment**

- cd analysis
- ./compile_<your OS> <analysis code in cpp>

- e.g.) ./compile_centos.sh TBanalysis_ex
