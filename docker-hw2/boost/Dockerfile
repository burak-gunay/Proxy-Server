FROM ubuntu:16.04
RUN apt-get update
RUN apt-get install g++ -y
RUN apt-get install libboost-all-dev -y
RUN mkdir -p /var/log/erss/
RUN mkdir /code
WORKDIR /code
ADD . /code/
RUN g++ main.cpp -pthread -lboost_system -o main --std=c++11
