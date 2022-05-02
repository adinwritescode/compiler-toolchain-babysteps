parse:
	gcc -g predictive_parser.c -o parse -lm
post:
	gcc -g postfix.c -o post
driver:
	clang++ -g -O3 Qadin.cpp -o Qadin `llvm-config --cxxflags --ldflags --system-libs --libs core`
clean:
	rm -f parse, post, Qadin