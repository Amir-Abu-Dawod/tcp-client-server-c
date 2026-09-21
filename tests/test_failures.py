from tests.protocol_client import send_command


def test_http_failure_is_not_cached(
    c_server,
    mock_http_server
):
    mock_http_server.set_response_status(
        "/anything",
        502
    )

    cache_file = (
        c_server["workdir"] / "anything.txt"
    )

    response = send_command(
        c_server["host"],
        c_server["port"],
        "anything"
    )

    assert response == (
        "ERROR: Failed to retrieve anything resource."
    )

    assert not cache_file.exists()

    assert (
        mock_http_server.get_request_count(
            "/anything"
        )
        == 1
    )

def test_unknown_command_returns_error(
    c_server
):
    response = send_command(
        c_server["host"],
        c_server["port"],
        "NOT_A_COMMAND"
    )

    assert response == "ERROR: Unknown command."

def test_server_survives_unknown_command(
    c_server
):
    first_response = send_command(
        c_server["host"],
        c_server["port"],
        "NOT_A_COMMAND"
    )

    second_response = send_command(
        c_server["host"],
        c_server["port"],
        "RTT"
    )

    assert first_response == (
        "ERROR: Unknown command."
    )

    assert second_response == "RTT_ACK"