#!/usr/bin/env python

import socket
import struct
import threading
import time
import signal

try:
    from enum import Enum
except ImportError:
    Enum = object

STX = 0x99

class PprzParserState(Enum):
    WaitSTX = 1
    GotSTX = 2
    GotLength = 3
    GotPayload = 4
    GotCRC1 = 5

class PprzTransport(object):
    def __init__(self):
        self.state = PprzParserState.WaitSTX
        self.length = 0
        self.buf = []
        self.ck_a = 0
        self.ck_b = 0
        self.idx = 0

    def parse_byte(self, c):
        b = struct.unpack("<B", c)[0]
        if self.state == PprzParserState.WaitSTX:
            if b == STX:
                self.state = PprzParserState.GotSTX
        elif self.state == PprzParserState.GotSTX:
            self.length = b - 4
            if self.length <= 0:
                self.state = PprzParserState.WaitSTX
                return False
            self.buf = bytearray(b)
            self.ck_a = b % 256
            self.ck_a = b % 256
            self.ck_b = b % 256
            self.buf[0] = STX
            self.buf[1] = b
            self.idx = 2
            self.state = PprzParserState.GotLength
        elif self.state == PprzParserState.GotLength:
            self.buf[self.idx] = b
            self.ck_a = (self.ck_a + b) % 256
            self.ck_b = (self.ck_b + self.ck_a) % 256
            self.idx += 1
            if self.idx == self.length+2:
                self.state = PprzParserState.GotPayload
        elif self.state == PprzParserState.GotPayload:
            if self.ck_a == b:
                self.buf[self.idx] = b
                self.idx += 1
                self.state = PprzParserState.GotCRC1
            else:
                self.state = PprzParserState.WaitSTX
        elif self.state == PprzParserState.GotCRC1:
            self.state = PprzParserState.WaitSTX
            if self.ck_b == b:
                self.buf[self.idx] = b
                return True
        else:
            self.state = PprzParserState.WaitSTX
        return False


class UdpInterface(threading.Thread):
  def __init__(self, read, writes):
    threading.Thread.__init__(self)
    self.shutdown_flag = threading.Event()
    self.trans = PprzTransport()
    self.read = read
    self.writes = writes
    self.running = True

  def stop(self):
    self.running = False
    self.server.close()

  def run(self):
    try:
      while self.running  and not self.shutdown_flag.is_set():
        try:
          (msg, address) = self.read.recvfrom(2048)
          length = len(msg)
          for c in msg:
            if not isinstance(c, bytes): c = struct.pack("B",c)
            if self.trans.parse_byte(c):
              #print("msg_id:",self.trans.buf[5])
              for x in self.writes:
                sock, port = x
                sock.sendto(self.trans.buf, ('127.0.0.1', port))
        except socket.timeout:
          pass
    except StopIteration:
      pass


if __name__ == '__main__':

  def pair(arg):return [int(x) for x in arg.split(':')]

  def sock(p):
    try:
      fd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
      fd.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
      fd.settimeout(2.0)
      fd.bind(('0.0.0.0',p))
      return(fd)
    except OSError:
      print("Error: unable to open socket on ports")
      exit(0)

  import argparse
  parser = argparse.ArgumentParser(usage="--mux InPort1:OutPort1 InPort2:OutPort2")
  parser.add_argument('--mux', type=pair, nargs='+')
  args = parser.parse_args()

  cons = [(sock(4243),4242)]
  for s in args.mux:
    if s[0] == 4242 or s[0] == 4243 or s[1] == 4242 or s[1] == 4243: 
      print("Error: 4242 and 4243 are not allowed")  
      exit(0)
    else: cons += [(sock(s[0]),s[1])]

  streams = []
  streams.append(UdpInterface(cons[0][0],cons[1:]))
  for i in range(1,len(cons)):
    streams.append(UdpInterface(cons[i][0],cons[0:1]))

  for thread in streams: thread.start()
  try:
    while True:
      time.sleep(2)
  except KeyboardInterrupt:
    for thread in streams:
      thread.shutdown_flag.set()
      thread.join()
