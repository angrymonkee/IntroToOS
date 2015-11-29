Project #4
Daniel Jones
Introduction to Operating Systems

Credit to:
SUN Remote Procedure Call Mechanism
http://web.cs.wpi.edu/~rek/DCS/D04/SunRPC.html

Oracle Developers Guide
https://docs.oracle.com/cd/E19683-01/816-1435/index.html

Protocol Compiling and Lower Level RPC Programming
http://www.cs.cf.ac.uk/Dave/C/node34.html


RPC Server:

	For the server side of this RPC call I decided to create a 
	structure to carry the data being transferred between the  client 
	and server. This structure is serialized and marshalled by the RPC 
	mechanism using TCP binding as specified in the requirements. I did
	notice timeouts when I increased the size of the files being 
	transferred. I used the "clnt_control" function to increase the 
	default timeout period to 60 seconds. A future enhancement could be
	to make the timeout configurable via input parameters. Since we were
	not submitting minifyjpeg_main.cs I did not include this in my 
	solution. Being that I had the intention of completing the 
	multithreaded version of the server, my solution was generated using
	the "M" parameter.

RPC Client:

	For the client side of the RPC call I created an instance of the 
	structure to be used when sending data to the server as well as the 
	structure to be used when for the results from the server. The 
	structure contained an "opaque" data type that carried the byte 
	array as well as the size of the array being transferred.

Summary:

	This solution was tested using files as large as 400MB with as many 
	as 20 client threads. All results showed proper output as expected 
	with the file 50% smaller than the original. Valgrind results helped 
	to resolve an initialization error I was getting when executing in 
	Udacity.

Known Issues:
	Only known issues are memory related. Valgrind identified 40 bytes 
	in the client application that were not being unallocated when the 
	execution completed. Efforts to identify where these were coming 
	from seemed to be in the generated code from rpcgen.

