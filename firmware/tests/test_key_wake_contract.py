#!/usr/bin/env python3
"""Integration contracts for Buddy's dimmed-screen key wake path."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def function_body(source: str, name: str) -> str:
    match = re.search(rf"\b{re.escape(name)}\s*\([^;]*?\)\s*\{{", source)
    if not match:
        raise AssertionError(f"function not found: {name}")
    start = match.end() - 1
    depth = 0
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start + 1:index]
    raise AssertionError(f"function is unterminated: {name}")


class KeyWakeGuardWiringTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.main = read("main/main.c")

    def test_callback_only_queues_actions(self) -> None:
        body = function_body(self.main, "on_key")
        self.assertIn("xQueueSend(keys,&e,0)", body)
        self.assertNotIn("buddy_ui_", body)

    def test_first_sleep_key_only_wakes_screen(self) -> None:
        self.assertIn("bool was_asleep=display_mode!=B_DISPLAY_ACTIVE || buddy_visible(&model,now)==B_SLEEP;buddy_touch(&model,now);activity_at=now;redraw=true;",self.main)
        self.assertIn("buddy_screen_key(&screen_locked,was_asleep,k.event==BSP_BTN_DOUBLE && k.key==BSP_BTN_OK)",self.main)
        self.assertIn("next=screen_locked?B_DISPLAY_OFF:",self.main)
        self.assertNotIn("esp_deep_sleep_start",self.main)


if __name__ == "__main__":
    unittest.main()
