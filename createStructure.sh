#!/bin/bash

mkdir -p HuffnPuffHighRise/core
mkdir -p HuffnPuffHighRise/gameRule
mkdir -p HuffnPuffHighRise/efficient_core
mkdir -p HuffnPuffHighRise/validation
mkdir -p HuffnPuffHighRise/outputs

mv core.cpp HuffnPuffHighRise/core/
mv slot.exe HuffnPuffHighRise/core/

touch HuffnPuffHighRise/SUMMARY.md
touch HuffnPuffHighRise/AGENTS.md

chmod a-w HuffnPuffHighRise/core/core.cpp

echo "Project structure initialized."