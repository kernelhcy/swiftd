all:midcheckppt.tex
	xelatex -output-directory=./tempfiles midcheckppt.tex &&\
	mv ./tempfiles/midcheckppt.pdf 中期检查PPT.pdf &&\
	acroread 中期检查PPT.pdf &

clean:
	rm ./tempfiles/*
