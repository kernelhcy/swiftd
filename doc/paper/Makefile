all:毕业设计论文.pdf 任务书考核表等.pdf
	xelatex -output-directory=./tempfiles mergepdf.tex
	mv ./tempfiles/mergepdf.pdf 毕业设计论文黄丛宇.pdf
	acroread 毕业设计论文黄丛宇.pdf &

midppt:midcheckppt.tex
	xelatex -output-directory=./tempfiles midcheckppt.tex
	xelatex -output-directory=./tempfiles midcheckppt.tex
	mv ./tempfiles/midcheckppt.pdf 中期检查PPT.pdf
	acroread 中期检查PPT.pdf &

paper:mypaper.tex
	xelatex -output-directory=./tempfiles mypaper.tex
	xelatex -output-directory=./tempfiles mypaper.tex
	mv ./tempfiles/mypaper.pdf 毕业设计论文.pdf
#	acroread 毕业设计论文.pdf &
	
ppt:paperppt.tex
	xelatex -output-directory=./tempfiles paperppt.tex
	xelatex -output-directory=./tempfiles paperppt.tex
	mv ./tempfiles/paperppt.pdf paperppt.pdf
	acroread paperppt.pdf &

c10k:c10kproblemzh.tex
	xelatex -output-directory=./tempfiles c10kproblemzh.tex
	xelatex -output-directory=./tempfiles c10kproblemzh.tex
	mv ./tempfiles/c10kproblemzh.pdf c10kproblemzh.pdf
	acroread c10kproblemzh.pdf &

clean:
	rm ./tempfiles/*
