import pytest
import time

from conftest import request


def test_stat(client):
    # cache should start fresh
    assert request(client, "STAT KEY") == "0"
    assert request(client, "STAT MEM") == "0"
    assert request(client, "STAT LOL") == "UNKNOWN"

def test_unknown_command(client):
    assert request(client, "LOL") == "UNKNOWN"

def test_set_get_and_overwrite(client):
    assert request(client, "SET Apple 123") == "OK"
    assert request(client, "GET Apple") == "123"
    assert request(client, "SET Apple 456") == "OK"
    assert request(client, "GET Apple") == "456"


def test_get_missing_key(client):
    assert request(client, "GET missing") == "(nil)"

def test_exist_and_delete(client):
    assert request(client, "SET Apple 123") == "OK"
    assert request(client, "EXIST Apple") == "Y"
    assert request(client, "DELETE Apple") == "OK"
    assert request(client, "EXIST Apple") == "N"


@pytest.mark.parametrize(
    "command",
    [
        "SET empty",
        "SET emptyagain",
        "SET whitespace    ",
    ],
)
def test_set_without_value_fails(client, command):
    assert request(client, command) == "FAILED"

def test_long_value(client):
    assert request(client, "SET e 1234567890") == "OK"
    assert request(client, "GET e") == "1234567890"
    assert request(client, "DELETE e") == "OK"

def test_long_key(client):
    key = (
        "verylongkeythatIaddedheretotestifitworksbutmaybethisisnot"
        "longenoughbtwfuckjewsandalsofucklgbtqbastardsandwhitebrits"
    )
    assert request(client, f"SET {key} 1") == "OK"
    assert request(client, f"GET {key}") == "1"
    assert request(client, f"DELETE {key}") == "OK"


def test_memory_accounting(client):
    # memory should be reset since all previous test keys deleted subsequently
    assert request(client, "STAT KEY") == "0"
    assert request(client, "STAT MEM") == "0"

    # test with new entries (memory count keeps count of both key & value)
    assert request(client, "SET memes/cat.jpg ju437hde") == "OK"
    assert request(client, "SET docs/important.txt mfd74edh") == "OK"
    assert request(client, "STAT KEY") == "2"
    assert request(client, "STAT MEM") == "47" 

    assert request(client, "DELETE memes/cat.jpg") == "OK"
    assert request(client, "STAT KEY") == "1"
    assert request(client, "STAT MEM") == "26"
    assert request(client, "DELETE docs/important.txt") == "OK"
    assert request(client, "STAT KEY") == "0"
    assert request(client, "STAT MEM") == "0"

    assert request(client, "SET memes/dog.jpg u489erg4") == "OK"
    assert request(client, "SET memes/shit.jpg 984y4gre") == "OK"
    assert request(client, "STAT KEY") == "2"
    assert request(client, "STAT MEM") == "43"
    assert request(client, "EVICT memes/dog.jpg") == "OK"
    assert request(client, "EVICT memes/shit.jpg") == "OK"
    assert request(client, "STAT KEY") == "0"
    assert request(client, "STAT MEM") == "0"

