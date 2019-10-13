# TSAM_P3
T-409-TSAM
Fall 2019

For this project, students were asked to write a simple store and forward botnet message server,
with accompanying Command and Control (C&C) client, where the goal is to link server and client into a class wide botnet.

This project was created, compiled and run on MacOS Mojave.

We got assistance from Group 21, Benedikt Rúnar and Hannes Kristjánsson.

Compiler used: g++

Instructions to run

Server:
1. Open skel.ru.is, log in and find the folder containing server.cpp
2. Compile the server code: g++ -std=c++11 server.cpp -o tsam<groupname>
3. Run the server code: ./tsam<grouopname> <port#>

Client:
1. Open a terminal window and find the folder containing client.cpp
2. Compile the client code: g++ -std=c++11 client.cpp -lpthread -o client
3. Run the client code: ./client <ip address> <port#>

