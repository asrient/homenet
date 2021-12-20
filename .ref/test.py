import socketserver
import time

class EchoRequestHandler(socketserver.BaseRequestHandler):

    def handle(self):
        # Echo the back to the client
        data = self.request.recv(1024)
        time.sleep(5)
        self.request.send(data)
        return

server=None
count = 0


def client(ind,ip,port):
    # Connect to the server
    global count
    global server
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((ip, port))

    # Send the data
    message = 'Hello, world'
    print (f'[{ind}] Sending : {message}')
    len_sent = s.send(bytes(message,'UTF-8'))

    # Receive a response
    response = s.recv(len_sent)
    print (f'[{ind}] Received : {response}')

    # Clean up
    s.close()
    count+=1
    if count==3:
        server.socket.close()

if __name__ == '__main__':
    import socket
    import threading

    address = ('localhost', 0) # let the kernel give us a port
    server = socketserver.TCPServer(address, EchoRequestHandler)
    ip, port = server.server_address # find out what port we were given

    t = threading.Thread(target=server.serve_forever)
    t.setDaemon(True) # don't hang on exit
    t.start()

    cs=[]

    for i in range(3):
        c = threading.Thread(target=client,args=(i,ip,port))
        cs.append(c)

    