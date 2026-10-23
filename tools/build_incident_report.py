import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
import os
import platform
import subprocess

def run_git_command(arguments):
    try:
        result = subprocess.run(
            ["git", *arguments],
            check=True,
            capture_output=True,
            text=True
        )

        return result.stdout.strip()

    except (
        subprocess.CalledProcessError,
        FileNotFoundError
    ):
        return None

def get_git_context():
    commit_sha = run_git_command(
        ["rev-parse", "HEAD"]
    )

    branch = run_git_command(
        ["branch", "--show-current"]
    )

    status = run_git_command(
        ["status", "--porcelain"]
    )

    return {
        "commit_sha": commit_sha,
        "branch": branch,
        "working_tree_dirty": bool(status),
    }

def get_environment_context():
    return {
        "os": platform.system(),
        "os_release": platform.release(),
        "machine": platform.machine(),
        "python_version": platform.python_version(),
        "ci": bool(
            os.getenv("CI")
        ),
    }

def get_ci_context():
    if not os.getenv("GITHUB_ACTIONS"):
        return None

    return {
        "provider": "github-actions",
        "repository": os.getenv(
            "GITHUB_REPOSITORY"
        ),
        "workflow": os.getenv(
            "GITHUB_WORKFLOW"
        ),
        "run_id": os.getenv(
            "GITHUB_RUN_ID"
        ),
        "run_number": os.getenv(
            "GITHUB_RUN_NUMBER"
        ),
        "job": os.getenv(
            "GITHUB_JOB"
        ),
        "ref": os.getenv(
            "GITHUB_REF"
        ),
        "sha": os.getenv(
            "GITHUB_SHA"
        ),
    }

def parse_float(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def parse_junit_report(xml_path):
    tree = ET.parse(xml_path)
    root = tree.getroot()

    test_results = []

    for testcase in root.iter("testcase"):
        classname = testcase.get("classname", "")
        name = testcase.get("name", "")
        duration = parse_float(
            testcase.get("time")
        )

        test_id = (
            f"{classname}::{name}"
            if classname
            else name
        )

        result = {
            "id": test_id,
            "suite": classname,
            "name": name,
            "status": "passed",
            "duration_seconds": duration,
        }

        failure = testcase.find("failure")
        error = testcase.find("error")
        skipped = testcase.find("skipped")

        if failure is not None:
            result["status"] = "failed"
            result["failure"] = {
                "kind": "assertion_failure",
                "message": failure.get(
                    "message",
                    ""
                ),
                "details": (
                    failure.text or ""
                ).strip(),
            }

        elif error is not None:
            result["status"] = "error"
            result["failure"] = {
                "kind": "test_error",
                "message": error.get(
                    "message",
                    ""
                ),
                "details": (
                    error.text or ""
                ).strip(),
            }

        elif skipped is not None:
            result["status"] = "skipped"

        test_results.append(result)

    summary = {
        "total": len(test_results),
        "passed": sum(
            test["status"] == "passed"
            for test in test_results
        ),
        "failed": sum(
            test["status"] == "failed"
            for test in test_results
        ),
        "errors": sum(
            test["status"] == "error"
            for test in test_results
        ),
        "skipped": sum(
            test["status"] == "skipped"
            for test in test_results
        ),
    }

    suites = list(root.iter("testsuite"))

    timestamp = None
    hostname = None
    reported_duration = 0.0

    if suites:
        timestamp = suites[0].get("timestamp")
        hostname = suites[0].get("hostname")

        reported_duration = sum(
            parse_float(suite.get("time"))
            for suite in suites
        )

    return {
        "schema_version": 2,

        "source": {
            "type": "pytest-junit",
            "file": str(xml_path),
        },

        "repository": get_git_context(),

        "environment": get_environment_context(),

        "ci_context": get_ci_context(),

        "run": {
            "timestamp": timestamp,
            "hostname": hostname,
            "duration_seconds": reported_duration,
            "summary": summary,
        },

        "tests": test_results,
    }


def main():
    if len(sys.argv) != 3:
        print(
            "Usage: "
            "python build_incident_report.py "
            "<junit.xml> <incident.json>"
        )
        return 1

    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])

    if not input_path.exists():
        print(
            f"JUnit report not found: "
            f"{input_path}"
        )
        return 1

    incident_report = parse_junit_report(
        input_path
    )

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True
    )

    with output_path.open(
        "w",
        encoding="utf-8"
    ) as file:
        json.dump(
            incident_report,
            file,
            indent=2
        )

    summary = (
        incident_report["run"]["summary"]
    )

    print(
        "Incident report created: "
        f"{output_path}"
    )

    print(
        f"Tests: {summary['total']} | "
        f"Passed: {summary['passed']} | "
        f"Failed: {summary['failed']} | "
        f"Errors: {summary['errors']} | "
        f"Skipped: {summary['skipped']}"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())