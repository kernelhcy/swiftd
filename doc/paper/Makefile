midcheck:midcheckppt.tex
	xelatex -output-directory=./tempfiles midcheckppt.tex
	mv ./tempfiles/midcheckppt.pdf 中期检查PPT.pdf
	acroread 中期检查PPT.pdf &

test:test.tex
	xelatex -output-directory=./tempfiles test.tex
	mv ./tempfiles/test.pdf test.pdf
	acroread test.pdf &

clean:
	rm ./tempfiles/*
