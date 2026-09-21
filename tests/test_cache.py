from tests.protocol_client import send_command


def test_anything_response_is_cached(
    c_server,
    mock_http_server
):
    cache_file = (
        c_server["workdir"] / "anything.txt"
    )

    assert not cache_file.exists()

    assert (
        mock_http_server.get_request_count(
            "/anything"
        )
        == 0
    )

    first_response = send_command(
        c_server["host"],
        c_server["port"],
        "anything"
    )

    assert "200 OK" in first_response
    assert "mock anything response" in first_response

    assert cache_file.exists()

    assert (
        mock_http_server.get_request_count(
            "/anything"
        )
        == 1
    )

    second_response = send_command(
        c_server["host"],
        c_server["port"],
        "anything"
    )

    assert second_response == first_response

    assert (
        mock_http_server.get_request_count(
            "/anything"
        )
        == 1
    )

def test_json_response_is_cached(
    c_server,
    mock_http_server
):
    cache_file = (
        c_server["workdir"] / "json.txt"
    )

    assert not cache_file.exists()

    first_response = send_command(
        c_server["host"],
        c_server["port"],
        "json"
    )

    assert "200 OK" in first_response
    assert '{"message":"mock json response"}' in first_response

    assert cache_file.exists()

    assert (
        mock_http_server.get_request_count(
            "/json"
        )
        == 1
    )

    second_response = send_command(
        c_server["host"],
        c_server["port"],
        "json"
    )

    assert second_response == first_response

    assert (
        mock_http_server.get_request_count(
            "/json"
        )
        == 1
    )