#!/usr/bin/env python3
import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from typing import Any


@dataclass
class LspMessage:
    payload: dict[str, Any]
    received_at: float


class LspClient:
    def __init__(self, name: str, command: list[str], cwd: pathlib.Path):
        self.name = name
        self.command = command
        self.cwd = cwd
        self.next_id = 1
        self.messages: list[LspMessage] = []
        self.stderr_lines: list[str] = []
        self.start_time = time.perf_counter()
        self.proc = subprocess.Popen(
            command,
            cwd=cwd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=False,
            bufsize=0,
        )
        self.reader = threading.Thread(target=self._read_stdout, daemon=True)
        self.err_reader = threading.Thread(target=self._read_stderr, daemon=True)
        self.reader.start()
        self.err_reader.start()

    def _read_stdout(self) -> None:
        assert self.proc.stdout is not None
        stream = self.proc.stdout
        while True:
            headers: dict[str, str] = {}
            while True:
                line = stream.readline()
                if not line:
                    return
                if line in (b"\r\n", b"\n"):
                    break
                key, _, value = line.decode("ascii", errors="replace").partition(":")
                headers[key.lower()] = value.strip()

            length = int(headers.get("content-length", "0"))
            body = stream.read(length)
            if not body:
                return
            try:
                payload = json.loads(body.decode("utf-8"))
            except json.JSONDecodeError:
                continue
            self.messages.append(LspMessage(payload=payload, received_at=time.perf_counter()))

    def _read_stderr(self) -> None:
        assert self.proc.stderr is not None
        for raw in self.proc.stderr:
            self.stderr_lines.append(raw.decode("utf-8", errors="replace").rstrip())

    def send(self, payload: dict[str, Any]) -> None:
        body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        header = f"Content-Length: {len(body)}\r\n\r\n".encode("ascii")
        assert self.proc.stdin is not None
        self.proc.stdin.write(header + body)
        self.proc.stdin.flush()

    def request(self, method: str, params: dict[str, Any]) -> int:
        request_id = self.next_id
        self.next_id += 1
        self.send({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params})
        return request_id

    def notify(self, method: str, params: dict[str, Any]) -> None:
        self.send({"jsonrpc": "2.0", "method": method, "params": params})

    def wait_response(self, request_id: int, timeout: float) -> LspMessage:
        deadline = time.perf_counter() + timeout
        seen = 0
        while time.perf_counter() < deadline:
            while seen < len(self.messages):
                msg = self.messages[seen]
                seen += 1
                if msg.payload.get("id") == request_id:
                    return msg
            if self.proc.poll() is not None:
                raise RuntimeError(f"{self.name} exited with code {self.proc.returncode}")
            time.sleep(0.005)
        raise TimeoutError(f"timed out waiting for {self.name} response id={request_id}")

    def stop(self) -> None:
        try:
            shutdown_id = self.request("shutdown", {})
            self.wait_response(shutdown_id, 2.0)
            self.notify("exit", {})
        except Exception:
            pass
        try:
            self.proc.terminate()
            self.proc.wait(timeout=2.0)
        except Exception:
            self.proc.kill()


def file_uri(path: pathlib.Path) -> str:
    return path.resolve().as_uri()


def generate_project(root: pathlib.Path, modules: int) -> tuple[pathlib.Path, int, int]:
    if root.exists():
        shutil.rmtree(root)
    rtl = root / "rtl"
    rtl.mkdir(parents=True)

    target_name = f"bench_mod_{modules - 1:05d}"
    top_text = f"""module bench_top;
  {target_name} u_target();
endmodule
"""
    top = rtl / "top.sv"
    top.write_text(top_text)

    for idx in range(modules):
        name = f"bench_mod_{idx:05d}"
        prev = f"bench_mod_{idx - 1:05d}" if idx else ""
        body = [f"module {name};\n"]
        body.append(f"  localparam int ID = {idx};\n")
        if prev:
            body.append(f"  {prev} u_prev();\n")
        body.append("endmodule\n")
        (rtl / f"{name}.sv").write_text("".join(body))

    files = [top] + [rtl / f"bench_mod_{idx:05d}.sv" for idx in range(modules)]
    (root / "lazyverilog.f").write_text("\n".join(str(path) for path in files) + "\n")
    (root / "verible.filelist").write_text("\n".join(str(path) for path in files) + "\n")
    (root / "lazyverilog.toml").write_text('[design]\nvcode = "lazyverilog.f"\n')

    return top, 1, top_text.splitlines()[1].index(target_name) + 1


def build_verible_ls(verible_root: pathlib.Path) -> pathlib.Path:
    binary = verible_root / "bazel-bin/verible/verilog/tools/ls/verible-verilog-ls"
    if binary.exists():
        return binary

    bench_bin = pathlib.Path("/tmp/lazyverilog-bench/bin")
    bench_bin.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = f"{bench_bin}:{env['PATH']}"
    if not shutil.which("bazel", path=env["PATH"]):
        bazelisk = shutil.which("bazelisk", path=env["PATH"])
        if bazelisk:
            (bench_bin / "bazel").symlink_to(bazelisk)
        elif shutil.which("go"):
            env["GOBIN"] = str(bench_bin)
            subprocess.run(
                ["go", "install", "github.com/bazelbuild/bazelisk@latest"],
                check=True,
                env=env,
            )
            bazelisk_path = bench_bin / "bazelisk"
            if not (bench_bin / "bazel").exists():
                (bench_bin / "bazel").symlink_to(bazelisk_path)

    subprocess.run(
        ["bazel", "build", "//verible/verilog/tools/ls:verible-verilog-ls"],
        cwd=verible_root,
        env=env,
        check=True,
    )
    return binary


def run_server(
    name: str,
    command: list[str],
    project: pathlib.Path,
    top: pathlib.Path,
    line: int,
    character: int,
    timeout: float,
) -> dict[str, Any]:
    text = top.read_text()
    client = LspClient(name, command, project)
    try:
        root_uri = file_uri(project)
        doc_uri = file_uri(top)

        initialize_started = time.perf_counter()
        init_id = client.request(
            "initialize",
            {
                "processId": os.getpid(),
                "rootUri": root_uri,
                "rootPath": str(project),
                "capabilities": {},
                "workspaceFolders": [{"uri": root_uri, "name": project.name}],
            },
        )
        init_msg = client.wait_response(init_id, timeout)
        initialized_at = init_msg.received_at
        client.notify("initialized", {})

        client.notify(
            "textDocument/didOpen",
            {
                "textDocument": {
                    "uri": doc_uri,
                    "languageId": "systemverilog",
                    "version": 1,
                    "text": text,
                }
            },
        )

        definition_started = time.perf_counter()
        def_id = client.request(
            "textDocument/definition",
            {
                "textDocument": {"uri": doc_uri},
                "position": {"line": line, "character": character},
            },
        )
        def_msg = client.wait_response(def_id, timeout)
        total = def_msg.received_at - client.start_time
        result = def_msg.payload.get("result")
        result_count = len(result) if isinstance(result, list) else int(result is not None)
        return {
            "server": name,
            "initialize_s": initialized_at - initialize_started,
            "definition_request_s": def_msg.received_at - definition_started,
            "start_to_definition_s": total,
            "result_count": result_count,
            "error": def_msg.payload.get("error"),
            "stderr_tail": client.stderr_lines[-5:],
        }
    finally:
        client.stop()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare first go-to-definition latency for lazyverilog-lsp and verible-ls."
    )
    parser.add_argument("--modules", type=int, default=4000)
    parser.add_argument("--project", type=pathlib.Path, default=pathlib.Path("/tmp/lazyverilog-lsp-bench"))
    parser.add_argument("--lazy-bin", type=pathlib.Path, default=pathlib.Path("build/lazyverilog-lsp"))
    parser.add_argument("--verible-root", type=pathlib.Path, default=pathlib.Path("~/dev/verible"))
    parser.add_argument("--verible-bin", type=pathlib.Path)
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--no-build-verible", action="store_true")
    args = parser.parse_args()

    top, line, character = generate_project(args.project, args.modules)
    lazy_bin = args.lazy_bin.resolve()
    verible_root = args.verible_root.expanduser().resolve()
    verible_bin = args.verible_bin.expanduser().resolve() if args.verible_bin else None

    if not lazy_bin.exists():
        subprocess.run(["cmake", "--build", "build", "--target", "lazyverilog-lsp"], check=True)
    if verible_bin is None:
        candidate = verible_root / "bazel-bin/verible/verilog/tools/ls/verible-verilog-ls"
        verible_bin = candidate if args.no_build_verible else build_verible_ls(verible_root)

    print(f"project: {args.project}")
    print(f"modules: {args.modules}")
    print(f"target:  {top}:{line + 1}:{character + 1}")
    print()

    rows = [
        run_server(
            "lazyverilog-lsp",
            [str(lazy_bin)],
            args.project,
            top,
            line,
            character,
            args.timeout,
        ),
        run_server(
            "verible-verilog-ls",
            [str(verible_bin)],
            args.project,
            top,
            line,
            character,
            args.timeout,
        ),
    ]

    print("server              init_s    gd_request_s  start_to_gd_s  results")
    for row in rows:
        print(
            f"{row['server']:<18} "
            f"{row['initialize_s']:>7.3f}   "
            f"{row['definition_request_s']:>11.3f}   "
            f"{row['start_to_definition_s']:>12.3f}   "
            f"{row['result_count']}"
        )
        if row["error"]:
            print(f"  error: {row['error']}")
        if row["stderr_tail"]:
            print("  stderr tail:")
            for line in row["stderr_tail"]:
                print(f"    {line}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
