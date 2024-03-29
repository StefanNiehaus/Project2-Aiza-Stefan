# Project2-Aiza-Stefan
This project implements a simplified version of TCP with Window Size Scaling. The resulting reliable data transfer protocol between the sender and receiver may also be tested using `mininet`. 

Usage
---
To build the RDT sender and receiver:
```
$ cd rdt2.0/src && make
```
Once compiled, the receiver can listen on a port as follows:
```
$ ./rdt2.0/obj/rdt_receiver <port_number> <file_name>
```
The sender can send a file to the receiver as follows:
```
$ ./rdt2.0/obj/rdt_sender <receiver_ip> <receiver_port> <file_name>
```

Test using `mininet`
---
Transfer `cellsim` folder into root directory of repository after cloning. This folder contains the necessary configurations for seting up the simulated network. Also ensure that the supporting files for adjusting network traffic are in the root folder.

The project is sub-divided into two tasks.

Task 1: Simplified TCP sender/receiver
---

- [x] **Pipeline Sender** Extending the sender to send 10 packets.

- [x] **Handling ACKs** Properly sending and handling acknowledgments.

- [x] **Retransmission** Retransmissions of lost packets.

- [x] **Error Handling** Properly receiving the exact file on the receiver with no errors.


Task 2: Congestion Control
---

- [x] **Receiver Buffer**

- [x] **Slow start**

- [x] **Congestion Avoidance** 

- [x] **Fast Retransmit** 

- [x] **Correct throughput plots** 

- [x] **Correct CWND recording and plotting** 
