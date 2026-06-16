import socket, time
time.sleep(2)
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', 55555))
keys = ['e', 'd', 'i', 't', 'spc', 'l', 'i', 's', 'p', 'dot', 't', 'x', 't', 'ret']
for k in keys:
    s.sendall(f'sendkey {k}\n'.encode())
    time.sleep(0.1)
s.sendall(b'quit\n')
s.close()
