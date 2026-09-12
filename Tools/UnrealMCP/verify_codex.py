"""Verify tool discovery through the installed Codex app-server, without a model turn."""

from datetime import datetime, timezone
import json
from pathlib import Path
import queue
import shutil
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved" / "UnrealMCP"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    with (OUT / "codex-discovery.stderr.log").open("w", encoding="utf-8") as errors:
        process = subprocess.Popen(
            [shutil.which("codex"), "app-server", "--stdio"],
            cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=errors,
            text=True, encoding="utf-8", creationflags=subprocess.CREATE_NO_WINDOW,
        )
        messages = queue.Queue()

        def collect():
            for line in process.stdout:
                try:
                    messages.put(json.loads(line))
                except json.JSONDecodeError:
                    pass

        threading.Thread(target=collect, daemon=True).start()
        seq = 0

        def send(method, params=None, timeout=90):
            nonlocal seq
            seq += 1
            request = {"id": seq, "method": method, "params": params}
            process.stdin.write(json.dumps(request) + "\n")
            process.stdin.flush()
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                item = messages.get(timeout=max(0.1, deadline - time.monotonic()))
                if item.get("id") == seq:
                    if "error" in item:
                        raise RuntimeError(item["error"])
                    return item["result"]
            raise TimeoutError(method)

        try:
            initialized = send("initialize", {
                "clientInfo": {"name": "renderlab_mcp_verifier", "version": "1.0.0"},
                "capabilities": {"experimentalApi": True},
            })
            process.stdin.write('{"method":"initialized"}\n')
            process.stdin.flush()
            result = send("mcpServerStatus/list", {"detail": "toolsAndAuthOnly", "limit": 100})
            report = {
                "utc": datetime.now(timezone.utc).isoformat(),
                "initialize": initialized, "status": result,
            }
            (OUT / "codex-discovery.json").write_text(
                json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
            )
            servers = result.get("data", [])
            target = next(item for item in servers if item["name"] == "unreal_renderlab")
            tools = target.get("tools", {})
            assert tools, f"Codex discovered no Unreal tools: {target}"
            print(json.dumps({
                "server": target["name"], "serverInfo": target["serverInfo"],
                "tools": sorted(tools), "evidence": str(OUT / "codex-discovery.json"),
            }, ensure_ascii=False, indent=2))
        finally:
            process.stdin.close()
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=10)


if __name__ == "__main__":
    main()
