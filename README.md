# Project2-Aiza-Stefan
This project implements a simplified version of TCP with Window Size Scaling. The resulting reliable data transfer protocol between the sender and reciever may also be tested using `mininet`.

Usage
---
To build the RDT sender and reciever:
```
$ cd rdt2.0/src && make
```
Once compiled, the reciever can listen on a port as follows:
```
$ ./rdt2.0/obj/rdt_receiver <port_number> <file_name>
```
The sender can send a file to the reciever as follows:
```
$ ./rdt2.0/obj/rdt_sender <reciever_ip> <reciever_port> <file_name>
```

The project is sub-divided into two tasks.

Task 1: Simplified TCP sender/receiver
---
- [ ] **Pipeline Sender** Extending the sender to send 10 packets.

- [ ] **Handling ACKs** Properly sending and handling acknowledgments.

- [ ] **Retransmission** Retransmissions of lost packets.

- [ ] **Error Handling** Properly receiving the exact file on the receiver with no errors.


Task 2: Congestion Control
---

- [ ] **Slow start** Implementing TCP slow start.

- [ ] **Congestion Avoidance** 

- [ ] **Fast Retransmit** 
