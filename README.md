# Project 1: Simple Mail Client

- Name: Martin Gonzalez
- Email: martingonzalez@u.boisestate.edu
- Class: CS425-001

## Known Bugs or Issues

No known bugs or issues. 

## Experience

There was a lot of new stuff for me in creating the mail client 
so it took me more time than I expected, but I was able to get it
in the end. I began with creating the mail client and being able
to send a message to the test server, but I didn't do it the wasy I 
needed to for Task 2. Pure protocol helpers, not calling recv and send 
directly, and the socket trasnport. That took me some time because I 
had to do a hefty restructure and move a lot of code around to match 
the requirements. The testing wasn't bad, but I relied on AI to produce
majority of the test cases and getting every possible case covered. 

## Design
The SMTP client is split into three layers so that protocol logic can be
tested without a network connection. Layer 1 was the pure protocol helpers 
that take strings/integers, and return strings or status codes. Layer 2 was 
the session over a swappable transport, implementing the SMTP conversation 
through a smtp struct. Layer 3 was the real socket transport, the wrappers 
over the functions.

The split was necessary to make testing possible without a real live server, 
instead using a fake server for the unit tests that would otherwise need
one. 