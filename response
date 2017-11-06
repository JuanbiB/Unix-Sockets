So this is the state machine logic:

TODO:
1) Make sure all of this works for binary files (images, pdfs, etc.) right now it only
   sends text files.
2) We need to implement the end of file logic. Right now receiver just quits and closes
   the file when it gets a "FIN" from the sender. There's more to it...
3) Implement step 3 of the receiver. (Mani) (use my timed_out() method in sender.cc)
4) Implement step 4 of the sender. (Juanbi)

I just implemented the receiver's ACK being lost and the sender resending payload. 

So the sender can follow this logic:
 repeat the following sequence until reaching the end of the file:

1. read the next block of data from the file and send it with the current sequence number
 2. wait for an incoming message
 3. if a timeout occurs before a message is received, resend the last packet and continue with step 2
 4. if a message is received, verify that it is an ACK with the expected sequence number. If it is, andvance
 the sequence number and continue with step 1. If not, resend the last packet and continue with step 2


The receiver's logic is simpler. It receives a packet and decides what to do with it, based on the content of the
 packet.
 1. If a data packet is received,
 if the packet has the expected sequence number, write the packet contents to the output file and
 send an ACK to the sender.
 if the sequence number is not the expected one, resend the ACK indicating the expected
 sequence number.
 2. If an ACK is received, ignore it.
 3. If the receiver times out without receiving anything, it resends the ACK indicating the expected sequence
 number.
