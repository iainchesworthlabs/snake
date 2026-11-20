# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests of tools/game-center/sync_app_store_connect.py against a stand-in for App Store Connect.

Nothing here reaches Apple and no key is used. The stand-in keeps leaderboards, achievements, their versions, their
localizations and their images in memory, answers the requests the script makes in the shape of version 2 of the Game
Center part of the App Store Connect API, and writes down every request, so a test can say what was asked.
"""

from __future__ import annotations

import importlib.util
import json
import re
import sys
import tempfile
import unittest
import urllib.parse
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/game-center/sync_app_store_connect.py"

spec = importlib.util.spec_from_file_location("sync_app_store_connect", SCRIPT)
assert spec is not None and spec.loader is not None
sync = importlib.util.module_from_spec(spec)
sys.modules["sync_app_store_connect"] = sync
spec.loader.exec_module(sync)

BUNDLE = "com.example.game"
UPLOADS = "https://uploads.example.test/"

KINDS = {
    "gameCenterLeaderboards": ("leaderboard", "gameCenterLeaderboardVersions", "gameCenterLeaderboardLocalizations"),
    "gameCenterAchievements": ("achievement", "gameCenterAchievementVersions", "gameCenterAchievementLocalizations"),
}
IMAGE_OF = {
    "gameCenterLeaderboardLocalizations": "gameCenterLeaderboardImages",
    "gameCenterAchievementLocalizations": "gameCenterAchievementImages",
}


class FakeAppStoreConnect:
    """App Store Connect, as far as the script uses it."""

    def __init__(self, game_center: bool = True) -> None:
        self.requests: list[tuple[str, str]] = []
        self.bodies: list[tuple[str, str, dict[str, Any]]] = []
        self.tokens_seen_by_uploads = 0
        self.uploaded: dict[str, bytes] = {}
        self.detail: str | None = "detail-1" if game_center else None
        self.resources: dict[str, dict[str, Any]] = {}  # identifier -> {type, attributes, versions}
        self.versions: dict[str, dict[str, Any]] = {}  # identifier -> {type, attributes, owner, words}
        self.words: dict[str, dict[str, Any]] = {}  # identifier -> {type, attributes, version, image}
        self.images: dict[str, dict[str, Any]] = {}  # identifier -> {type, attributes, words}
        self.next = 0

    # What a test sets up or looks at ----------------------------------------------------------------------------------

    def new_id(self, prefix: str) -> str:
        self.next += 1
        return f"{prefix}-{self.next}"

    def writes(self) -> list[tuple[str, str]]:
        return [(method, path) for method, path in self.requests if method != "GET"]

    def resource(self, identifier: str) -> dict[str, Any]:
        return next(r for r in self.resources.values() if r["attributes"]["vendorIdentifier"] == identifier)

    def latest(self, identifier: str) -> dict[str, Any]:
        versions = [self.versions[v] for v in self.resource(identifier)["versions"]]
        return max(versions, key=lambda version: version["attributes"]["version"])

    def set_state(self, state: str) -> None:
        for version in self.versions.values():
            version["attributes"]["state"] = state

    # The requests -----------------------------------------------------------------------------------------------------

    def send(self, method: str, url: str, headers: dict[str, str], body: bytes | None) -> tuple[int, bytes]:
        if url.startswith(UPLOADS):
            if "Authorization" in headers:
                self.tokens_seen_by_uploads += 1
            name = url[len(UPLOADS) :]
            self.uploaded[name] = self.uploaded.get(name, b"") + (body or b"")
            return 200, b""
        assert url.startswith(sync.API + "/"), url
        assert headers.get("Authorization") == "Bearer token", "a request to the API must carry the token"
        parts = urllib.parse.urlsplit(url)
        path, query = parts.path, urllib.parse.parse_qs(parts.query)
        self.requests.append((method, path))
        document = json.loads(body.decode("utf-8")) if body else {}
        if body:
            self.bodies.append((method, path, document))
        status, answer = self.route(method, path, query, document)
        return status, json.dumps(answer).encode("utf-8") if answer is not None else b""

    def route(self, method: str, path: str, query: dict[str, list[str]], document: dict[str, Any]) -> tuple[int, Any]:
        data = document.get("data", {})
        if method == "GET" and path == "/v1/apps":
            wanted = query["filter[bundleId]"][0]
            apps = [{"type": "apps", "id": "app-1", "attributes": {"bundleId": BUNDLE}}] if wanted == BUNDLE else []
            return 200, {"data": apps}
        if method == "GET" and path == "/v1/apps/app-1/gameCenterDetail":
            return 200, {"data": {"type": "gameCenterDetails", "id": self.detail} if self.detail else None}
        if method == "POST" and path == "/v1/gameCenterDetails":
            assert data["relationships"]["app"]["data"] == {"type": "apps", "id": "app-1"}
            self.detail = "detail-1"
            return 201, {"data": {"type": "gameCenterDetails", "id": self.detail}}

        listed = re.fullmatch(
            r"/v1/gameCenterDetails/detail-1/(gameCenterLeaderboardsV2|gameCenterAchievementsV2)", path
        )
        if method == "GET" and listed:
            kind = "gameCenterLeaderboards" if "Leaderboards" in listed.group(1) else "gameCenterAchievements"
            return 200, {"data": [self.shown(i, r) for i, r in self.resources.items() if r["type"] == kind]}

        for kind, (owner, version_type, words_type) in KINDS.items():
            base = "/v2/" + kind
            if method == "POST" and path == base:
                return self.create(kind, owner, version_type, document)
            match = re.fullmatch(base + r"/([^/]+)", path)
            if method == "PATCH" and match:
                self.resources[match.group(1)]["attributes"].update(data["attributes"])
                return 200, {"data": self.shown(match.group(1), self.resources[match.group(1)])}
            match = re.fullmatch(base + r"/([^/]+)/versions", path)
            if method == "GET" and match:
                ids = self.resources[match.group(1)]["versions"]
                return 200, {"data": [self.shown(i, self.versions[i]) for i in ids]}

            versions = "/v2/" + version_type
            if method == "POST" and path == versions:
                resource_id = data["relationships"][owner]["data"]["id"]
                assert data["relationships"][owner]["data"]["type"] == kind
                return 201, {"data": self.shown(*self.add_version(resource_id, version_type))}
            match = re.fullmatch(versions + r"/([^/]+)/localizations", path)
            if method == "GET" and match:
                found = [(i, w) for i, w in self.words.items() if w["version"] == match.group(1)]
                answer: dict[str, Any] = {"data": [], "included": []}
                for identifier, words in found:
                    shown = self.shown(identifier, words)
                    if words["image"]:
                        shown["relationships"] = {
                            "image": {"data": {"type": IMAGE_OF[words_type], "id": words["image"]}}
                        }
                        answer["included"].append(self.shown(words["image"], self.images[words["image"]]))
                    answer["data"].append(shown)
                assert query.get("include") == ["image"]
                return 200, answer

            localizations = "/v2/" + words_type
            if method == "POST" and path == localizations:
                version_id = data["relationships"]["version"]["data"]["id"]
                assert self.versions[version_id]["type"] == version_type
                if self.versions[version_id]["attributes"]["state"] not in sync.EDITABLE:
                    return 409, self.refusal("STATE_ERROR", "The version cannot be changed in its state.")
                identifier = self.new_id("words")
                self.words[identifier] = {
                    "type": words_type,
                    "attributes": dict(data["attributes"]),
                    "version": version_id,
                    "image": None,
                }
                return 201, {"data": self.shown(identifier, self.words[identifier])}
            match = re.fullmatch(localizations + r"/([^/]+)", path)
            if method == "PATCH" and match:
                words = self.words[match.group(1)]
                if self.versions[words["version"]]["attributes"]["state"] not in sync.EDITABLE:
                    return 409, self.refusal("STATE_ERROR", "The version cannot be changed in its state.")
                words["attributes"].update(data["attributes"])
                return 200, {"data": self.shown(match.group(1), words)}

            images = "/v2/" + IMAGE_OF[words_type]
            if method == "POST" and path == images:
                words_id = data["relationships"]["localization"]["data"]["id"]
                assert self.words[words_id]["image"] is None, "a localization has one image: the old one goes first"
                identifier = self.new_id("image")
                size = data["attributes"]["fileSize"]
                half = size // 2
                self.images[identifier] = {
                    "type": IMAGE_OF[words_type],
                    "words": words_id,
                    "attributes": {
                        "fileName": data["attributes"]["fileName"],
                        "fileSize": size,
                        "assetDeliveryState": {"state": "AWAITING_UPLOAD"},
                        # Two parts, to show that the script sends each part where and as it is told.
                        "uploadOperations": [
                            {
                                "method": "PUT",
                                "url": UPLOADS + identifier,
                                "offset": 0,
                                "length": half,
                                "requestHeaders": [{"name": "Content-Type", "value": "image/png"}],
                            },
                            {
                                "method": "PUT",
                                "url": UPLOADS + identifier,
                                "offset": half,
                                "length": size - half,
                                "requestHeaders": [{"name": "Content-Type", "value": "image/png"}],
                            },
                        ],
                    },
                }
                self.words[words_id]["image"] = identifier
                return 201, {"data": self.shown(identifier, self.images[identifier])}
            match = re.fullmatch(images + r"/([^/]+)", path)
            if method == "PATCH" and match:
                assert data["attributes"] == {"uploaded": True}
                image = self.images[match.group(1)]
                complete = len(self.uploaded.get(match.group(1), b"")) == image["attributes"]["fileSize"]
                image["attributes"]["assetDeliveryState"] = {"state": "COMPLETE" if complete else "FAILED"}
                return 200, {"data": self.shown(match.group(1), image)}
            if method == "DELETE" and match:
                image = self.images.pop(match.group(1))
                self.words[image["words"]]["image"] = None
                return 204, None
        return 404, self.refusal("NOT_FOUND", f"{method} {path} is not a thing the stand-in knows")

    def create(self, kind: str, owner: str, version_type: str, document: dict[str, Any]) -> tuple[int, Any]:
        data = document["data"]
        attributes = data["attributes"]
        if any(r["attributes"]["vendorIdentifier"] == attributes["vendorIdentifier"] for r in self.resources.values()):
            return 409, self.refusal("ENTITY_ERROR.ATTRIBUTE.INVALID.DUPLICATE", "The identifier is in use.")
        # Apple's request to make a component makes its first version with it.
        assert data["relationships"]["gameCenterDetail"]["data"] == {"type": "gameCenterDetails", "id": "detail-1"}
        first = data["relationships"]["versions"]["data"]
        assert len(first) == 1 and first[0]["type"] == version_type and re.fullmatch(r"\$\{[\w-]+\}", first[0]["id"])
        assert document["included"] == [{"type": version_type, "id": first[0]["id"]}]
        identifier = self.new_id(owner)
        self.resources[identifier] = {"type": kind, "attributes": {**attributes, "archived": False}, "versions": []}
        self.add_version(identifier, version_type)
        return 201, {"data": self.shown(identifier, self.resources[identifier])}

    def add_version(self, resource_id: str, version_type: str) -> tuple[str, dict[str, Any]]:
        identifier = self.new_id("version")
        number = len(self.resources[resource_id]["versions"]) + 1
        self.versions[identifier] = {
            "type": version_type,
            "owner": resource_id,
            "attributes": {"version": number, "state": "PREPARE_FOR_SUBMISSION"},
        }
        self.resources[resource_id]["versions"].append(identifier)
        return identifier, self.versions[identifier]

    @staticmethod
    def shown(identifier: str, stored: dict[str, Any]) -> dict[str, Any]:
        return {"type": stored["type"], "id": identifier, "attributes": json.loads(json.dumps(stored["attributes"]))}

    @staticmethod
    def refusal(code: str, detail: str) -> dict[str, Any]:
        return {"errors": [{"status": "409", "code": code, "title": "Refused", "detail": detail}]}


def components_in(folder: Path, change: Any = None) -> Path:
    """A components.json of one leaderboard and two achievements, with their images, in a folder of its own."""
    document = {
        "leaderboards": [
            {
                "id": "com.example.game.leaderboard.easy",
                "referenceName": "Easy",
                "scoreFormat": "INTEGER",
                "sort": "DESC",
                "submission": "BEST_SCORE",
                "en": {"name": "Easy"},
            },
        ],
        "achievements": [
            {
                "id": "com.example.game.achievement.wrap",
                "referenceName": "Through the wall",
                "points": 10,
                "hidden": False,
                "achievableMoreThanOnce": False,
                "en": {
                    "name": "Through the wall",
                    "preEarnedDescription": "Go through the wall.",
                    "earnedDescription": "You went through the wall.",
                },
            },
            {
                "id": "com.example.game.achievement.bonus",
                "referenceName": "Bonus item",
                "points": 20,
                "hidden": True,
                "achievableMoreThanOnce": False,
                "en": {
                    "name": "Bonus item",
                    "preEarnedDescription": "Eat a bonus item.",
                    "earnedDescription": "You ate a bonus item.",
                },
            },
        ],
    }
    if change is not None:
        change(document)
    (folder / "images").mkdir(exist_ok=True)
    for slug, pixels in (("wrap", b"wrap-picture-1"), ("bonus", b"bonus-picture-1")):
        file = folder / "images" / f"{slug}.png"
        if not file.exists():
            file.write_bytes(pixels)
    path = folder / "components.json"
    path.write_text(json.dumps(document), encoding="utf-8")
    return path


class SyncTest(unittest.TestCase):
    def setUp(self) -> None:
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.path = Path(self.folder.name)
        self.store = FakeAppStoreConnect()

    def run_sync(self, apply: bool, change: Any = None) -> Any:
        components = sync.read_components(components_in(self.path, change))
        api = sync.Api(lambda: "token", self.store.send, sleep=lambda seconds: None)
        self.store.requests.clear()
        return sync.Sync(api, sync.Report(apply=apply)).run(components, BUNDLE)

    def test_a_dry_run_says_what_is_missing_and_writes_nothing(self) -> None:
        report = self.run_sync(apply=False)
        self.assertEqual(self.store.writes(), [])
        self.assertEqual(self.store.resources, {})
        text = "\n".join(report.lines)
        for identifier in ("leaderboard.easy", "achievement.wrap", "achievement.bonus"):
            self.assertIn(f"would change: create {identifier.split('.')[0]} com.example.game.{identifier}", text)
        self.assertGreater(report.changes, 0)

    def test_apply_makes_each_component_with_its_version_words_and_image(self) -> None:
        report = self.run_sync(apply=True)
        self.assertGreater(report.changes, 0)
        self.assertEqual(len(self.store.resources), 3)
        wrap = self.store.resource("com.example.game.achievement.wrap")
        self.assertEqual(wrap["attributes"]["points"], 10)
        self.assertTrue(wrap["attributes"]["showBeforeEarned"])
        # "hidden" in the file is the other way round from Apple's "shown before it is earned".
        self.assertFalse(self.store.resource("com.example.game.achievement.bonus")["attributes"]["showBeforeEarned"])
        easy = self.store.resource("com.example.game.leaderboard.easy")["attributes"]
        self.assertEqual(
            (easy["defaultFormatter"], easy["scoreSortType"], easy["submissionType"]), ("INTEGER", "DESC", "BEST_SCORE")
        )

        version = self.store.latest("com.example.game.achievement.wrap")
        words = [w for w in self.store.words.values() if w["version"] in wrap["versions"]]
        self.assertEqual(version["attributes"]["version"], 1)
        self.assertEqual([w["attributes"]["locale"] for w in words], ["en-US"])
        self.assertEqual(words[0]["attributes"]["beforeEarnedDescription"], "Go through the wall.")
        image = self.store.images[words[0]["image"]]
        self.assertRegex(image["attributes"]["fileName"], r"^wrap-[0-9a-f]{12}\.png$")
        self.assertEqual(image["attributes"]["assetDeliveryState"]["state"], "COMPLETE")
        self.assertEqual(self.store.uploaded[words[0]["image"]], b"wrap-picture-1")
        # The leaderboard has no image in the file, so none is made for it.
        self.assertEqual({i["type"] for i in self.store.images.values()}, {"gameCenterAchievementImages"})

    def test_the_token_goes_to_the_api_and_not_to_the_upload(self) -> None:
        self.run_sync(apply=True)
        self.assertGreater(len(self.store.uploaded), 0)
        self.assertEqual(self.store.tokens_seen_by_uploads, 0)

    def test_a_second_run_finds_nothing_to_change(self) -> None:
        self.run_sync(apply=True)
        again = self.run_sync(apply=True)
        self.assertEqual(again.changes, 0, again.lines)
        self.assertEqual(self.store.writes(), [])
        self.assertEqual(self.run_sync(apply=False).changes, 0)

    def test_nothing_is_ever_deleted_but_an_image_that_is_replaced(self) -> None:
        self.run_sync(apply=True)
        self.store.resources["extra"] = {
            "type": "gameCenterAchievements",
            "attributes": {
                "vendorIdentifier": "com.example.game.achievement.old",
                "referenceName": "Old",
                "archived": False,
            },
            "versions": [],
        }

        def without_bonus(document: dict[str, Any]) -> None:
            document["achievements"] = document["achievements"][:1]

        report = self.run_sync(apply=True, change=without_bonus)
        self.assertEqual(self.store.writes(), [])
        self.assertEqual(len(self.store.resources), 4)
        left = "\n".join(report.left)
        self.assertIn("com.example.game.achievement.old is in App Store Connect and not in components.json", left)
        self.assertIn("com.example.game.achievement.bonus is in App Store Connect and not in components.json", left)

    def test_points_and_words_that_changed_are_changed_in_a_version_not_sent_yet(self) -> None:
        self.run_sync(apply=True)

        def changed(document: dict[str, Any]) -> None:
            document["achievements"][0]["points"] = 30
            document["achievements"][0]["en"]["earnedDescription"] = "Out one side and in the other."

        report = self.run_sync(apply=True, change=changed)
        self.assertEqual(report.changes, 2, report.lines)
        self.assertEqual([method for method, _ in self.store.writes()], ["PATCH", "PATCH"])
        wrap = self.store.resource("com.example.game.achievement.wrap")
        self.assertEqual(wrap["attributes"]["points"], 30)
        self.assertEqual(len(wrap["versions"]), 1)
        words = next(w for w in self.store.words.values() if w["version"] == wrap["versions"][0])
        self.assertEqual(words["attributes"]["afterEarnedDescription"], "Out one side and in the other.")
        self.assertEqual(self.run_sync(apply=True, change=changed).changes, 0)

    def test_a_new_picture_replaces_the_image(self) -> None:
        self.run_sync(apply=True)
        (self.path / "images" / "wrap.png").write_bytes(b"wrap-picture-2, redrawn")
        report = self.run_sync(apply=True)
        self.assertEqual(report.changes, 1, report.lines)
        self.assertEqual([method for method, _ in self.store.writes()], ["DELETE", "POST", "PATCH"])
        wrap = self.store.resource("com.example.game.achievement.wrap")
        words = next(w for w in self.store.words.values() if w["version"] == wrap["versions"][0])
        self.assertEqual(self.store.uploaded[words["image"]], b"wrap-picture-2, redrawn")
        self.assertEqual(self.run_sync(apply=True).changes, 0)

    def test_a_live_version_is_not_changed_and_a_difference_starts_the_next(self) -> None:
        self.run_sync(apply=True)
        self.store.set_state("LIVE")
        self.assertEqual(self.run_sync(apply=True).changes, 0)

        def changed(document: dict[str, Any]) -> None:
            document["achievements"][0]["en"]["name"] = "Through the wall!"

        dry = self.run_sync(apply=False, change=changed)
        self.assertEqual(self.store.writes(), [])
        self.assertIn("start a new version (version 1 is LIVE and cannot be changed)", "\n".join(dry.lines))

        self.run_sync(apply=True, change=changed)
        wrap = self.store.resource("com.example.game.achievement.wrap")
        self.assertEqual(len(wrap["versions"]), 2)
        first, second = (self.store.versions[v] for v in wrap["versions"])
        self.assertEqual(
            (first["attributes"]["state"], second["attributes"]["state"]), ("LIVE", "PREPARE_FOR_SUBMISSION")
        )
        old = next(w for w in self.store.words.values() if w["version"] == wrap["versions"][0])
        new = next(w for w in self.store.words.values() if w["version"] == wrap["versions"][1])
        self.assertEqual(old["attributes"]["name"], "Through the wall")
        self.assertEqual(new["attributes"]["name"], "Through the wall!")
        self.assertIsNotNone(new["image"])
        self.assertEqual(self.run_sync(apply=True, change=changed).changes, 0)

    def test_a_version_that_is_with_apple_is_left_alone(self) -> None:
        self.run_sync(apply=True)
        self.store.set_state("IN_REVIEW")

        def changed(document: dict[str, Any]) -> None:
            document["leaderboards"][0]["en"]["name"] = "Easy mode"

        report = self.run_sync(apply=True, change=changed)
        self.assertEqual(self.store.writes(), [])
        self.assertEqual(report.changes, 0)
        self.assertIn("its version 1 is with Apple (IN_REVIEW)", "\n".join(report.left))

    def test_game_center_is_turned_on_for_an_app_that_has_it_off(self) -> None:
        self.store = FakeAppStoreConnect(game_center=False)
        dry = self.run_sync(apply=False)
        self.assertEqual(self.store.writes(), [])
        self.assertIn("would change: turn Game Center on", dry.lines[0])
        self.run_sync(apply=True)
        self.assertEqual(self.store.detail, "detail-1")
        self.assertEqual(len(self.store.resources), 3)

    def test_an_app_the_key_cannot_see_stops_the_run(self) -> None:
        components = sync.read_components(components_in(self.path))
        api = sync.Api(lambda: "token", self.store.send, sleep=lambda seconds: None)
        with self.assertRaises(sync.ComponentsError):
            sync.Sync(api, sync.Report(apply=True)).run(components, "com.example.other")
        self.assertEqual(self.store.writes(), [])

    def test_a_refusal_by_apple_is_shown_with_its_reason(self) -> None:
        self.run_sync(apply=True)
        # The version is live, and the stand-in is told nothing of the new version: a change in place is refused.
        self.store.set_state("LIVE")
        wrap = self.store.resource("com.example.game.achievement.wrap")
        words_id = next(i for i, w in self.store.words.items() if w["version"] == wrap["versions"][0])
        api = sync.Api(lambda: "token", self.store.send, sleep=lambda seconds: None)
        with self.assertRaises(sync.ApiError) as raised:
            api.call(
                "PATCH", f"/v2/gameCenterAchievementLocalizations/{words_id}", {"data": {"attributes": {"name": "x"}}}
            )
        self.assertIn("HTTP 409", str(raised.exception))
        self.assertIn("The version cannot be changed in its state.", str(raised.exception))


class ComponentsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.path = Path(self.folder.name)

    def faults(self, change: Any) -> str:
        with self.assertRaises(sync.ComponentsError) as raised:
            sync.read_components(components_in(self.path, change))
        return str(raised.exception)

    def test_the_file_of_the_game_is_in_order(self) -> None:
        components = sync.read_components()
        self.assertEqual(sum(1 for c in components if c.kind is sync.LEADERBOARD), 4)
        self.assertEqual(sum(1 for c in components if c.kind is sync.ACHIEVEMENT), 12)
        self.assertTrue(all(c.image is not None for c in components if c.kind is sync.ACHIEVEMENT))
        self.assertTrue(all("en-US" in c.words for c in components))

    def test_every_fault_is_named_at_once(self) -> None:
        def broken(document: dict[str, Any]) -> None:
            document["leaderboards"][0]["sort"] = "UP"
            document["achievements"][0]["points"] = 101
            del document["achievements"][1]["en"]
            document["achievements"].append({**document["achievements"][0], "points": 5})

        text = self.faults(broken)
        self.assertIn("UP is not one of ASC, DESC", text)
        self.assertIn('"points" must be a whole number from 1 to 100', text)
        self.assertIn('it has no "en"', text)
        self.assertIn("is there twice", text)

    def test_an_achievement_without_its_image_is_a_fault(self) -> None:
        def another(document: dict[str, Any]) -> None:
            document["achievements"][0]["id"] = "com.example.game.achievement.nopicture"

        self.assertIn("its image", self.faults(another))

    def test_more_than_a_thousand_points_is_a_fault(self) -> None:
        def many(document: dict[str, Any]) -> None:
            one = document["achievements"][0]
            document["achievements"] = [
                {**one, "id": f"com.example.game.achievement.wrap{n}", "points": 100} for n in range(11)
            ]
            for n in range(11):
                (self.path / "images").mkdir(exist_ok=True)
                (self.path / "images" / f"wrap{n}.png").write_bytes(b"p")

        self.assertIn("come to 1100 points, and Apple allows 1000", self.faults(many))


class TokenTest(unittest.TestCase):
    def test_a_der_signature_becomes_the_64_bytes_of_a_token(self) -> None:
        r = int.from_bytes(bytes(range(1, 33)), "big")
        s = 0x80 << 248  # the top bit set, so DER writes a zero before it
        der_r = b"\x02\x20" + r.to_bytes(32, "big")
        der_s = b"\x02\x21\x00" + s.to_bytes(32, "big")
        der = b"\x30" + bytes([len(der_r) + len(der_s)]) + der_r + der_s
        self.assertEqual(sync.der_to_raw(der), r.to_bytes(32, "big") + s.to_bytes(32, "big"))
        with self.assertRaises(ValueError):
            sync.der_to_raw(b"not a signature")

    def test_the_token_names_the_key_and_is_made_again_before_it_runs_out(self) -> None:
        clock = [1_000_000.0]
        signed: list[bytes] = []

        def sign(private_key: str, message: bytes) -> bytes:
            self.assertEqual(private_key, "the key")
            signed.append(message)
            return b"\x01" * 64

        token = sync.Token("KEY123", "issuer-1", "the key", sign=sign, now=lambda: clock[0])
        first = token()
        header, claims, signature = (
            json.loads(_decode(part)) if index < 2 else part for index, part in enumerate(first.split("."))
        )
        self.assertEqual(header, {"alg": "ES256", "kid": "KEY123", "typ": "JWT"})
        self.assertEqual(claims["iss"], "issuer-1")
        self.assertEqual(claims["aud"], "appstoreconnect-v1")
        self.assertLessEqual(claims["exp"] - claims["iat"], 1200)
        self.assertEqual(signed[0].decode(), first.rsplit(".", 1)[0])
        self.assertNotIn("the key", first)
        self.assertTrue(signature)
        clock[0] += 60
        self.assertEqual(token(), first)
        clock[0] += 600
        self.assertNotEqual(token(), first)


class ApiTest(unittest.TestCase):
    def test_a_token_is_never_sent_outside_the_api(self) -> None:
        api = sync.Api(lambda: "token", lambda *request: (200, b"{}"))
        with self.assertRaises(ValueError):
            api.call("GET", "https://example.test/v1/apps")

    def test_too_many_requests_is_asked_again_and_a_failed_write_is_not(self) -> None:
        answers = [(429, b""), (200, b'{"data": []}')]
        asked: list[str] = []

        def send(method: str, url: str, headers: dict[str, str], body: bytes | None) -> tuple[int, bytes]:
            asked.append(method)
            return answers.pop(0)

        api = sync.Api(lambda: "token", send, sleep=lambda seconds: None)
        self.assertEqual(api.call("POST", "/v2/gameCenterAchievements", {"data": {}}), {"data": []})
        self.assertEqual(asked, ["POST", "POST"])

        answers[:] = [(500, b""), (200, b"{}")]
        asked.clear()
        with self.assertRaises(sync.ApiError):
            api.call("POST", "/v2/gameCenterAchievements", {"data": {}})
        self.assertEqual(asked, ["POST"])

    def test_every_page_of_a_list_is_read(self) -> None:
        pages = {
            sync.API + "/v1/things?limit=200": {
                "data": [{"id": "1"}],
                "links": {"next": sync.API + "/v1/things?cursor=2"},
            },
            sync.API + "/v1/things?cursor=2": {"data": [{"id": "2"}], "links": {}},
        }
        api = sync.Api(lambda: "token", lambda method, url, headers, body: (200, json.dumps(pages[url]).encode()))
        data, _ = api.every("/v1/things")
        self.assertEqual([resource["id"] for resource in data], ["1", "2"])


def _decode(part: str) -> bytes:
    import base64

    return base64.urlsafe_b64decode(part + "=" * (-len(part) % 4))


class ProgramTest(unittest.TestCase):
    def test_validate_checks_the_file_of_the_game_and_needs_no_key(self) -> None:
        self.assertEqual(sync.main(["--validate"]), 0)

    def test_without_the_key_the_program_says_which_secrets_are_not_set(self) -> None:
        import contextlib
        import io
        import os
        from unittest import mock

        names = ("APPSTORE_API_KEY_ID", "APPSTORE_API_ISSUER_ID", "APPSTORE_API_PRIVATE_KEY")
        errors = io.StringIO()
        with mock.patch.dict(os.environ, {name: "" for name in names}), contextlib.redirect_stderr(errors):
            self.assertEqual(sync.main([]), 1)
        for name in names:
            self.assertIn(name, errors.getvalue())


if __name__ == "__main__":
    unittest.main()
