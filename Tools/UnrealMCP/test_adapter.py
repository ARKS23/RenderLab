"""Offline stdlib tests for the project scope adapter and upstream error handling."""

import asyncio
import json
from unittest import IsolatedAsyncioTestCase, TestCase, main
from unittest.mock import AsyncMock, patch

import start_server as adapter


def identity(project=None, version="5.8.0-55116800+++UE5+Release-5.8"):
    return {
        "success": True,
        "result": json.dumps({
            "project_dir": str(adapter.ROOT if project is None else project),
            "engine_version": version,
        }),
    }


class ScopeTests(IsolatedAsyncioTestCase):
    async def test_correct_project(self):
        with patch.object(adapter, "_python", AsyncMock(return_value=identity())):
            await adapter.check_project()

    async def test_wrong_project(self):
        fake = AsyncMock(return_value=identity(project="C:/OtherProject"))
        with patch.object(adapter, "_python", fake):
            with self.assertRaisesRegex(adapter.core.UnrealExecutionError, "Wrong Editor project"):
                await adapter.check_project()

    async def test_wrong_engine(self):
        fake = AsyncMock(return_value=identity(version="5.7.4"))
        with patch.object(adapter, "_python", fake):
            with self.assertRaisesRegex(adapter.core.UnrealExecutionError, "Expected UE 5.8"):
                await adapter.check_project()

    async def test_malformed_response(self):
        fake = AsyncMock(return_value={"success": True, "result": "bad JSON"})
        with patch.object(adapter, "_python", fake):
            with self.assertRaises(adapter.core.UnrealExecutionError):
                await adapter.check_project()

    async def test_failure_does_not_forward_action(self):
        fake = AsyncMock(return_value=identity(project="C:/OtherProject"))
        action = AsyncMock()
        with patch.object(adapter, "_python", fake), patch.object(adapter, "_send", action):
            with self.assertRaises(adapter.core.UnrealExecutionError):
                await adapter.scoped_action("actor_actions", "ue_set_location", {})
        action.assert_not_called()

    async def test_connection_refusal_is_reported(self):
        fake = AsyncMock(side_effect=adapter.core.UnrealExecutionError("Connection refused"))
        with patch.object(adapter, "_python", fake):
            with self.assertRaisesRegex(adapter.core.UnrealExecutionError, "Connection refused"):
                await adapter.check_project()

    async def test_forwarding_after_identity(self):
        action = AsyncMock(return_value={"success": True, "count": 138})
        with patch.object(adapter, "_python", AsyncMock(return_value=identity())):
            with patch.object(adapter, "_send", action):
                result = await adapter.scoped_action("level_actions", "ue_list_level_actors", {})
        self.assertEqual(result["count"], 138)
        action.assert_awaited_once_with("level_actions", "ue_list_level_actors", {})

    async def test_discovery_has_no_editor_dependency(self):
        fake = AsyncMock(side_effect=AssertionError("Discovery must not contact Editor"))
        with patch.object(adapter, "_python", fake):
            await adapter.configure()
            tools = await adapter.dispatcher.dispatcher_mcp.get_tools()
        self.assertEqual(set(tools), adapter.DOMAINS)
        fake.assert_not_called()


class ResultTests(TestCase):
    def test_inner_failure_is_preserved(self):
        result = adapter.core._unwrap_result({
            "success": True, "result": '{"success": false, "message": "Not found"}',
        })
        self.assertFalse(result["success"])

    def test_inner_success_is_unwrapped(self):
        result = adapter.core._unwrap_result({
            "success": True, "result": '{"success": true, "count": 3}',
        })
        self.assertEqual(result["count"], 3)


if __name__ == "__main__":
    main(verbosity=2)
