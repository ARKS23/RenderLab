# Unreal MCP Source Provenance

- Repository: https://github.com/GenOrca/unreal-mcp
- Commit: `f7986db239516aa4299ddc6f54d713253bd82631`
- Retrieved: 2026-09-11
- Plugin/server version: 2.2.0
- License: Apache-2.0, retained in `Plugins/UnrealMCPython/LICENSE.txt` and `server/LICENSE`.
- Vendored plugin: `Plugins/UnrealMCPython` from the upstream `Plugins/UnrealMCPython`.
- Vendored server: `Tools/UnrealMCP/server` from the upstream `mcp-server`.
- No upstream example project, asset pack, client configuration, or engine binaries installed.

## Local Integration Changes

1. Plugin descriptor declares `EditorScriptingUtilities`, used by upstream Python actions.
2. `start_server.py` limits discovery to nine relevant domains, checks the project's absolute path and UE 5.8 before each editor operation, and serializes TCP calls off the MCP event loop.
3. Project `.venv` uses the upstream lock's FastMCP 2.7.1 / MCP 1.9.3 runtime set (36 packages). FastMCP 2.14.7 was tested but rejected: upstream imports the old top-level `Image` export. Trial-only packages were uninstalled using pip; `pip check` passes.
4. Project-only Codex configuration; no global MCP config changes.
5. `server/src/unreal_mcp/dispatcher.py` adds MCP initialization instructions about read-first usage and upstream limitations. No upstream C++ implementation changes were needed to build on UE 5.8.

## Known Upstream Limits

Graph reads search event/function graphs but not macro or collapsed subgraphs, and omit hidden pins. Variable reads cover Blueprint-owned member variables; callable function reflection is not complete function-source enumeration and its parameter list also includes compiler-generated local properties (missing CPF_Parm filtering). The loopback TCP bridge permits arbitrary Python execution without authentication and lacks robust request framing. Use only a trusted local machine, short serialized requests, and no network forwarding. The historical dependency lock is for local stdio compatibility, not an assurance that these versions are suitable for an exposed HTTP service.

Local compilation and test evidence are recorded in `Docs/UE_MCP_Setup.md`.
