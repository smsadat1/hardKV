import socket
import pytest


HOST = "127.0.0.1"
PORT = 9736


@pytest.fixture(scope="session")
def client():
    with socket.create_connection((HOST, PORT)) as sock:
        yield sock

def request(client, message):
    client.sendall(f"{message}\n".encode("utf-8"))
    return client.recv(4096).decode("utf-8").rstrip('\n')
