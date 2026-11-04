import pytest
import time

from conftest import request

def test_key_value_persistence(client):
    assert request(client, "SET Eggplant 909") == "OK"
    assert request(client, "GET Eggplant") == "909"
    assert request(client, "EVICT Eggplant") == "OK"

    # should survive cache removal (EXPIRE) because of disk
    assert request(client, "GET Eggplant") == "909" 
    assert request(client, "DELETE Eggplant") == "OK"
    assert request(client, "GET Eggplant") == "(nil)"


