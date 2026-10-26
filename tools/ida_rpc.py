#!/usr/bin/env python3
"""Minimal JSON-RPC client for the IDA MCP plugin (localhost:13337).

The ida-pro-mcp MCP server proxies to this same endpoint. This helper is for
bulk read-only snapshots (function/string/type inventories) that would be too
large to stream through the chat.

Usage:
    python tools/ida_rpc.py <method> [json-params-array]

Examples:
    python tools/ida_rpc.py get_metadata
    python tools/ida_rpc.py list_functions '[0,0]'
    python tools/ida_rpc.py list_strings '[0,0]'
    python tools/ida_rpc.py get_function_by_address '["0x1000ba00"]'
"""

from __future__ import annotations

import json
import sys
import urllib.error
import urllib.request

URL = "http://127.0.0.1:13337/mcp"


def rpc(method: str, params: list | dict | None = None):
    payload = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": method,
        "params": [] if params is None else params,
    }
    req = urllib.request.Request(
        URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            obj = json.loads(resp.read().decode("utf-8"))
    except urllib.error.URLError as exc:
        raise SystemExit(f"IDA RPC unreachable at {URL}: {exc}") from exc
    if "error" in obj:
        err = obj["error"]
        raise SystemExit(
            f"IDA RPC error {err.get('code')}: {err.get('message')}"
        )
    return obj.get("result")


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    method = sys.argv[1]
    params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else []
    result = rpc(method, params)
    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
