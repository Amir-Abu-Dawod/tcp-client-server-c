from tests.protocol_client import send_command


def test_rtt_request_returns_ack(c_server):
    response = send_command(
        c_server["host"],
        c_server["port"],
        "RTT"
    )

    assert response == "RTT_ACK"

def test_multiple_rtt_requests(c_server):
    first_response = send_command(
        c_server["host"],
        c_server["port"],
        "RTT"
    )

    second_response = send_command(
        c_server["host"],
        c_server["port"],
        "RTT"
    )

    assert first_response == "RTT_ACK"
    assert second_response == "RTT_ACK"