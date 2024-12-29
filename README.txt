Author: Grayson Wade

How to Build:
"Make" builds both chat server and chat client
"Make _fileName_" will make only the specified file
"Make clean" removes both executables.

How to Run:
chat-sever reguires the Port number to be passed as the first argument
ex: ./chat-server 4000

chat-client requires two arguments, IP agress and Port number
ex: ./chat-client localhost 4000

About the program:
These programs were a submission for Middlburry college CSCI 315 systems programing assignment 7. chat-server.c is a multi-threaded chat server which can handle an arbitrary number of connections. All messages sent through the server are displayed with a time stamp, and the nickname of the author client. Clients can set their name with "/nick _name_". The server will anounce whenever a client renames themselves. Clients can disconnect with "exit". Terminating the sever will cause all clients to terrminate. 