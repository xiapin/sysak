from socket import socket, AF_INET, SOCK_STREAM


def cli():
    s = socket(AF_INET, SOCK_STREAM)
    s.connect(('172.16.0.129', 4321))
    s.send(b'Hello')


if __name__ == '__main__':
    cli()
