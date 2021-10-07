all:
	mkdir -p bin/include
	mkdir -p bin/lib
	cp *.h bin/include/
	g++ -c -I src yhttpparser.cpp -o bin/yhttpparser.o
	g++ -c -I src yhttpserver.cpp -o bin/yhttpserver.o
	ar -rcs libsimpleserver.a bin/*.o
	mv libsimpleserver.a bin/lib/
	rm -rf bin/*.o
test: test.cpp
	g++ -I bin/include test.cpp bin/lib/libsimpleserver.a -o bin/http_server_test
	