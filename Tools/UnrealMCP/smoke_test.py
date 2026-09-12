"""Read-only MCP protocol and Editor smoke test; writes only Saved test evidence."""

import argparse
import asyncio
from datetime import datetime, timezone
import json
from pathlib import Path
import sys

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

ROOT = Path(__file__).resolve().parents[2]
REPORT = ROOT / "Saved" / "UnrealMCP" / "smoke-test.json"


def unpack(result):
    if result.isError:
        raise AssertionError(str(result))
    for content in result.content:
        if content.type == "text":
            data = json.loads(content.text)
            if data.get("success") is False:
                raise AssertionError(json.dumps(data, ensure_ascii=False))
            # Raw execute_python has an additional transport wrapper.
            if isinstance(data.get("result"), str):
                return json.loads(data["result"])
            return data
    raise AssertionError("No JSON text result")


async def run(args):
    report = {"utc": datetime.now(timezone.utc).isoformat(), "results": {}}
    parameters = StdioServerParameters(
        command=sys.executable,
        args=[str(Path(__file__).with_name("start_server.py"))],
        cwd=str(ROOT),
    )
    try:
        async with stdio_client(parameters) as (read, write):
            async with ClientSession(read, write) as client:
                init = await client.initialize()
                report["initialize"] = init.model_dump(mode="json")
                tools = await client.list_tools()
                report["tools"] = [tool.name for tool in tools.tools]
                assert set(report["tools"]) == {
                    "actor", "asset", "blueprint", "editor", "level",
                    "material", "static_mesh", "texture", "util",
                }
                print("PASS MCP initialize/tools/list: 9 domains", flush=True)

                async def call(name, domain, action, params=None):
                    result = await client.call_tool(
                        domain, {"action": action, "params": params or {}}
                    )
                    data = unpack(result)
                    report["results"][name] = data
                    print(f"PASS {name}", flush=True)
                    return data

                if not args.offline:
                    project = await call("project", "util", "get_project_info")
                    assert project["project_name"] == "RenderLab"
                    assert project["engine_version"].startswith("5.8.")
                    await call("level", "level", "get_current_level_path")
                    actors = await call("actors", "level", "list_level_actors")
                    assert actors["count"] > 0
                    light = next(
                        (actor for actor in actors["actors"] if actor["class"] == "DirectionalLight"),
                        actors["actors"][0],
                    )
                    components = await call(
                        "components", "actor", "list_actor_components",
                        {"actor_label": light["label"]},
                    )
                    assert components["count"] > 0
                    await call("asset_catalog", "asset", "list_actions")
                    await call("blueprint_catalog", "blueprint", "list_actions")
                    bp = args.blueprint
                    await call(
                        "blueprint_asset", "asset", "get_asset_info", {"asset_path": bp}
                    )
                    variables = await call(
                        "variables", "blueprint", "list_blueprint_variables", {"asset_path": bp}
                    )
                    assert variables["count"] > 0
                    functions = await call(
                        "functions", "blueprint", "list_callable_functions",
                        {"asset_path": bp, "filter": "Sky"},
                    )
                    assert functions["count"] > 0
                    graph = await call(
                        "graph", "blueprint", "get_blueprint_graph_info",
                        {"asset_path": bp, "graph_name": args.graph},
                    )
                    assert graph["node_count"] > 0
                    pins = {
                        (node["node_name"], pin["pin_name"])
                        for node in graph["nodes"] for pin in node["pins"]
                    }
                    links = [
                        (node["node_name"], pin["pin_name"], link["node_name"], link["pin_name"])
                        for node in graph["nodes"] for pin in node["pins"]
                        for link in pin.get("linked_to", [])
                    ]
                    assert links, "Need a graph with a nonempty connection to validate links"
                    missing = [link for link in links if (link[2], link[3]) not in pins]
                    assert not missing, f"Links to omitted pins: {missing}"
                    assert all(
                        (link[2], link[3], link[0], link[1]) in links for link in links
                    ), "Non-reciprocal graph connection"
                    report["graph_summary"] = {
                        "nodes": graph["node_count"], "visible_pins": len(pins),
                        "directed_links": len(links), "missing_targets": missing,
                        "reciprocal_connections": len(links) // 2,
                    }
                    await call(
                        "blueprint_components", "blueprint", "list_blueprint_components",
                        {"asset_path": bp},
                    )
                    await call("editor_selection", "editor", "get_selected_assets")
                    await call(
                        "material_info", "material", "get_material_info",
                        {"material_path": "/Engine/EngineSky/M_Sky_Panning_Clouds2"},
                    )
                report["success"] = True
    except BaseException as exc:
        report["success"] = False
        report["error"] = repr(exc)
        raise
    finally:
        REPORT.parent.mkdir(parents=True, exist_ok=True)
        output = REPORT.with_name("smoke-offline.json") if args.offline else REPORT
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"Evidence: {output}", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--offline", action="store_true", help="Only initialize and list tools")
    parser.add_argument("--blueprint", default="/Engine/EngineSky/BP_Sky_Sphere")
    parser.add_argument("--graph", default="UserConstructionScript")
    asyncio.run(run(parser.parse_args()))
