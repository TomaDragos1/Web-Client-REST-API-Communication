build: server.cpp subscriber.cpp
	g++ -g -Wall server.cpp -o server -lm
	g++ -g -Wall subscriber.cpp -o subscriber -lm
clean:
	g++ -g -Wall server.cpp -o server
	g++ -g -Wall subscriber.cpp -o subscriber
	rm server subscriber