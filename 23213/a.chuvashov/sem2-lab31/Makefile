main.out: main.cpp parser.out proxy.out cache.out
	g++ -Wall cache.out parser.out proxy.out main.cpp -o main.out

parser.out: HTTP_Parser.cpp
	g++ -Wall -c HTTP_Parser.cpp -o parser.out

proxy.out : Proxy.cpp
	g++ -Wall -c Proxy.cpp -o proxy.out

cache.out : Cache.cpp
	g++ -Wall -c Cache.cpp -o cache.out

clean: 
	rm *.out