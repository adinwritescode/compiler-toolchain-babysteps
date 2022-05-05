driver:
	clang++ -g -O3 Qadin.cpp -o Qadin `llvm-config --cxxflags --ldflags --system-libs --libs core`
clean:
	rm -f Qadin