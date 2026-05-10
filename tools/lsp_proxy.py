#!/usr/bin/env python3
"""Transparent LSP message logging proxy.

Usage:
    lsp_proxy.py <logfile> <cmd> [args...]

Sits between Neovim and an LSP server, logging every JSON-RPC message
(both directions) to logfile while forwarding transparently.

Uses raw OS file descriptors (os.read/os.write) to avoid Python's
BufferedReader lock contention at interpreter shutdown with daemon threads.
"""
import sys
import os
import subprocess
import threading
import json
import time


STDIN_FD  = 0
STDOUT_FD = 1


def _read_exact(fd, n):
    """Read exactly n bytes from fd. Returns b'' on EOF."""
    buf = b""
    while len(buf) < n:
        chunk = os.read(fd, n - len(buf))
        if not chunk:
            return b""
        buf += chunk
    return buf


def read_message(fd):
    """Read one LSP framed message from raw fd.
    Returns (headers_dict, body_bytes) or (None, None) on EOF.
    Reads headers byte-by-byte (small) then body in bulk.
    """
    raw_header = b""
    while not raw_header.endswith(b"\r\n\r\n"):
        ch = os.read(fd, 1)
        if not ch:
            return None, None
        raw_header += ch

    headers = {}
    for line in raw_header.decode("utf-8", errors="replace").split("\r\n"):
        line = line.strip()
        if ":" in line:
            k, v = line.split(":", 1)
            headers[k.strip()] = v.strip()

    length = int(headers.get("Content-Length", 0))
    body = _read_exact(fd, length) if length > 0 else b""
    if length > 0 and len(body) < length:
        return None, None  # EOF mid-body
    return headers, body


def write_message(fd, body: bytes):
    """Write one LSP framed message to raw fd."""
    frame = f"Content-Length: {len(body)}\r\n\r\n".encode("utf-8") + body
    os.write(fd, frame)


def log_message(log, direction: str, body: bytes):
    try:
        data = json.loads(body)
        pretty = json.dumps(data, indent=2)
        if isinstance(data, dict):
            method = data.get("method") or f"response id={data.get('id', '?')}"
        else:
            method = "?"
    except Exception:
        pretty = body.decode("utf-8", errors="replace")
        method = "PARSE_ERROR"

    ts = time.strftime("%H:%M:%S")
    log.write(f"\n{'='*60}\n[{ts}] {direction}  {method}\n{pretty}\n")
    log.flush()


def forward_loop(src_fd, dst_fd, direction, log, done_event):
    """Read from src_fd, log, write to dst_fd. Signal done_event on EOF/error."""
    try:
        while True:
            headers, body = read_message(src_fd)
            if headers is None:
                log.write(f"\n[PROXY] EOF on {direction}\n")
                log.flush()
                return
            log_message(log, direction, body)
            write_message(dst_fd, body)
    except OSError as e:
        log.write(f"\n[PROXY ERROR] {direction}: {e}\n")
        log.flush()
    finally:
        done_event.set()


def main():
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    log_path = sys.argv[1]
    cmd = sys.argv[2:]

    log = open(log_path, "w", buffering=1)
    log.write(f"LSP Proxy started at {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
    log.write(f"Command: {' '.join(cmd)}\n")
    log.flush()

    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=open(log_path + ".stderr", "w"),
        close_fds=True,
    )
    server_stdin_fd  = proc.stdin.fileno()
    server_stdout_fd = proc.stdout.fileno()

    done_cs = threading.Event()
    done_sc = threading.Event()

    t_in = threading.Thread(
        target=forward_loop,
        args=(STDIN_FD, server_stdin_fd, "C→S", log, done_cs),
        daemon=True,
    )
    t_out = threading.Thread(
        target=forward_loop,
        args=(server_stdout_fd, STDOUT_FD, "S→C", log, done_sc),
        daemon=True,
    )
    t_in.start()
    t_out.start()

    proc.wait()
    log.write(f"\n[PROXY] Server exited with code {proc.returncode}\n")

    # Give threads 2s to flush; then exit cleanly without touching stdin buffer
    done_cs.wait(timeout=2.0)
    done_sc.wait(timeout=2.0)
    log.write("[PROXY] done.\n")
    log.close()
    os._exit(0)  # Hard exit — avoids BufferedReader lock at Python shutdown


if __name__ == "__main__":
    main()
