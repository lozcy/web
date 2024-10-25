FLAGS = -g -std=c++11 -Wall
INC = -I ../lock -I ../http_conn -I ../threadpool -I ../DB -I ../timer
SQLLINK = -lmysqlclient
REDISLINK = -lhiredis
THREADLINK = -lpthread
LINK = $(SQLLINK) $(THREADLINK) $(REDISLINK)
all:
	g++ $(FLAGS) $(INC) -o test test.cpp $(LINK)