"""Start the pinned local MCP server, scoped to this RenderLab checkout."""

import asyncio
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent / "server" / "src"))

from unreal_mcp import core, dispatcher  # noqa: E402

DOMAINS = {
    "actor", "asset", "blueprint", "editor", "level",
    "material", "static_mesh", "texture", "util",
}
_send = core.send_to_unreal
_python = core.send_python_exec
_compile = core.send_livecoding_compile
_lock = asyncio.Lock()


def canonical(path):
    return os.path.normcase(os.path.realpath(path)).rstrip("\\/")


async def check_project():
    # Check each call: the editor may have restarted on another project.
    response = await asyncio.to_thread(
        asyncio.run,
        _python(
            "import unreal,json; "
            "print(json.dumps({'project_dir':unreal.Paths.convert_relative_path_to_full("
            "unreal.Paths.project_dir()),"
            "'engine_version':unreal.SystemLibrary.get_engine_version()}))"
        ),
    )
    try:
        info = json.loads(response["result"])
        if not response.get("success"):
            raise ValueError("Editor Python execution failed")
        if canonical(info["project_dir"]) != canonical(ROOT):
            raise ValueError(f"Wrong Editor project: {info['project_dir']}")
        if not info["engine_version"].startswith("5.8."):
            raise ValueError(f"Expected UE 5.8, got {info['engine_version']}")
    except (KeyError, TypeError, ValueError) as exc:
        raise core.UnrealExecutionError(f"RenderLab identity check failed: {exc}") from exc


async def scoped_action(module, action, params):
    async with _lock:
        await check_project()
        return await asyncio.to_thread(asyncio.run, _send(module, action, params))


async def scoped_python(code):
    async with _lock:
        await check_project()
        return await asyncio.to_thread(asyncio.run, _python(code))


async def scoped_compile():
    async with _lock:
        await check_project()
        return await asyncio.to_thread(asyncio.run, _compile())


async def configure():
    dispatcher.send_to_unreal = scoped_action
    dispatcher.send_python_exec = scoped_python
    dispatcher.send_livecoding_compile = scoped_compile
    for name in list(await dispatcher.dispatcher_mcp.get_tools()):
        if name not in DOMAINS:
            dispatcher.dispatcher_mcp.remove_tool(name)


async def main():
    await configure()
    await dispatcher.dispatcher_mcp.run_async(transport="stdio")


if __name__ == "__main__":
    asyncio.run(main())
