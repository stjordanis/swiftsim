#!/bin/bash
echo "Generating PDF..."

cd ./figures
python3 flux_correction_method_plot.py
cd ..

pdflatex -jobname=GEARRT GEARRT.tex
bibtex GEARRT.tex
pdflatex -jobname=GEARRT GEARRT.tex
pdflatex -jobname=GEARRT GEARRT.tex
