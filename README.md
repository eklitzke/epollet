# Edge-triggered Epoll

This repo demonstrates how to use `epoll` with the `EPOLLET` flag set (which
puts file descriptors into edge-triggered mode). This is a followup to my blog
post
[Blocking I/O, Nonblocking I/O, And Epoll](https://eklitzke.org/blocking-io-nonblocking-io-and-epoll),
intended to demonstrate some real code.
