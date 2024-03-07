CXXFLAGS += -O3 -march=native

.PHONY: test
test: test.cpp ugrid.hpp
	$(CXX) test.cpp -o test -Wall $(CXXFLAGS) && ./test
