from tests.protocol_client import send_command


def test_anything_request(c_server):
    response = send_command(
        c_server["host"],
        c_server["port"],
        "anything"
    )

    assert "200 OK" in response
    assert "mock anything response" in response


def test_json_request(c_server):
    response = send_command(
        c_server["host"],
        c_server["port"],
        "json"
    )

    assert "200 OK" in response
    assert '{"message":"mock json response"}' in response