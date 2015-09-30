Project #1
Daniel Jones
Introduction to Operating Systems

Credit to:
Beej's Guide to Network Programming
http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html


/* ======== Echoclent and Echoserve (Warmup #1) ======== */

	I spent most of my here time getting acclaimated to some of the basics
of C programming. I probably spent more time than I should have but,
felt it necessary to help build a foundation before moving forward.

echoclient.c:
	Client simply connects to a socket and begins transmitting data 
once a handshake with the server has been established. Once it receives a 
response back from the server it will echo out the response

echoserver.c:
	Once server receives a request it responds with the content in
the request. One important aspect of the server that I found to be of critical
importance was the ability to handle multiple connections at a time. I knew
the rest of the assigments would require this functionality so I built this
into the server from the start. If you look in the "main" method of the
echoserver, you will see that I loop indefinitely and fork a new process to
to handle multiple requests. I now know this is not ideal and I should have used
threads instead, but this was my first attempt at acheiving this behavior.


/* ======= FileTransfer (Warmup #2) ======= */

transferclient.c:
	Here I reused the methods I created in the echoclient to 
connect to the server and establish the handshake. I used the "fwrite" function
to support handling binary data transfers

transferserver.c:
	Similar to the client is used the "fread" function to read
to support transmitting binary data when reading from the file system.

TESTING:
Both MD5 and SHA1 sums were run on all transmitted files and originals on the
server. These check sums were compared and verified to be identical in all
tested cases.

Files used were both text and images files, ranging from 5KB to 100MB


/* ========== GetFile Client and GetFile Server ========= */

	I tried to leverage enum constructs for both the scheme and method
parts of the client request header and server response header. These were used
to allow common touch points for future implementations in the event I wanted 
to add a "POST" method or any number of new schemes to the system.

gfscheme_t: enum of known schemes
gfmethod_t: enum of known methods

	Additionally, I wanted to encapsulate the response sent back from the 
server so I created "response_message_t". This lives on the request object so 
I can easily see which response is associated with a given request. In the event
this were to be multithreaded, then having the response on the request 
would allow the data for each thread to be self contained.

TESTING:
Both MD5 and SHA1 sums were run on all transmitted files and originals on the
server. These check sums were compared and verified to be identical in all 
tested scenarios.

Tests were run with all files included in 
"server_root/courses/ud923/filecorpus" folder in the compressed file downloaded
from udacity.


/* ========= Multithreaded GetFile Client Server ========= */

My approach for this closely followed the design outlined in the link under
the "Helpful Hints" section of the project specification.
	https://docs.google.com/drawings/d/1a2LPUBv9a3GvrrGzoDu2EY4779-tJzMJ7Sz2ArcxWFU/edit

CLIENT:
	Followed boss-worker pattern for sending requests to the server based 
on user input. The "ServerSettings" structure was created to pass both server 
and thread specific settings to each worker thread. This gave each thread the
information required to carry out it's own specific unit of work. A file lock
was used to manage access to the file system when writing received files. This
was only necessary in the event that the same file was sent back by two 
different requests which would have cause a race condition and could have 
corrupted the file otherwise.

SERVER:
	The server also followed a boss-worker pattern for receiving requests
and transmitting responses. First, I initialize a worker queue (steque.h) in
handler.c which will be used by the workers to handle each request. New requests
are picked up by the boss thread and dropped into the worker queue. Each spawned
worker thread is set to continually processed until "cleaned up" by the server
"main"

TESTING:

Both MD5 and SHA1 sums were run on all transmitted files and originals on the
server. These check sums were compared and verified to be identical in all
tested scenarios.

Tests were run with all files included in
"server_root/courses/ud923/filecorpus" folder in the compressed file downloaded
from udacity.
