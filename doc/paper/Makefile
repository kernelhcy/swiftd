midppt:midcheckppt.tex
	xelatex -output-directory=./tempfiles midcheckppt.tex
	xelatex -output-directory=./tempfiles midcheckppt.tex
	mv ./tempfiles/midcheckppt.pdf 中期检查PPT.pdf
	acroread 中期检查PPT.pdf &

paper:mypaper.tex
	xelatex -output-directory=./tempfiles mypaper.tex
	xelatex -output-directory=./tempfiles mypaper.tex
	mv ./tempfiles/mypaper.pdf 毕业设计论文.pdf
	acroread 毕业设计论文.pdf &
	
test:test.tex
	xelatex -output-directory=./tempfiles test.tex
	xelatex -output-directory=./tempfiles test.tex
	mv ./tempfiles/test.pdf test.pdf
	acroread test.pdf &

clean:
	rm ./tempfiles/*
