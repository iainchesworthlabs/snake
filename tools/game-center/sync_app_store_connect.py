#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Makes Game Center in App Store Connect the same as apps/snake/packaging/ios/game-center/components.json.

    python3 tools/game-center/sync_app_store_connect.py            says what it would change, and changes nothing
    python3 tools/game-center/sync_app_store_connect.py --apply    makes the changes
    python3 tools/game-center/sync_app_store_connect.py --validate checks components.json and the images, off line

components.json is the record. For each leaderboard and achievement in it the script creates the one that is missing,
with its first version, its names and descriptions and its image, and brings one that differs back to the file. It
needs nothing but Python 3 and the `openssl` program, which signs the token.

What it never does:

- delete or archive anything, or change an identifier (Apple never lets an identifier change). A component that is in
  App Store Connect and not in the file is named and left alone;
- add a component to a version of the app, submit one for review or release one. Those stay with the owner;
- touch a version that is with Apple (waiting for review, in review, accepted, pending release). It says so and
  leaves it for the next run.

Apple keeps the words and the image of a component in numbered versions. A version that is not live yet is changed in
place. A version that is live cannot be changed, so a difference starts the next version, which the owner then sends
for review with a version of the app.

The key is an App Store Connect API key, read from the environment and never written to the output:

    APPSTORE_API_KEY_ID, APPSTORE_API_ISSUER_ID, APPSTORE_API_PRIVATE_KEY (the contents of the AuthKey_<id>.p8 file)

It follows version 2 of the Game Center part of the App Store Connect API (specification 4.6.1). Version 1 of that
part is deprecated and is not used.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
COMPONENTS = ROOT / "apps/snake/packaging/ios/game-center/components.json"
BUNDLE_ID = "com.iainchesworthlabs.snake"
API = "https://api.appstoreconnect.apple.com"

# The languages of components.json, as the game names them, and Apple's locale for each.
LOCALES = {"en": "en-US", "fr": "fr-FR", "es": "es-ES", "de": "de-DE", "ja": "ja", "zh_CN": "zh-Hans", "he": "he"}

SCORE_FORMATS = (
    "INTEGER", "DECIMAL_POINT_1_PLACE", "DECIMAL_POINT_2_PLACE", "DECIMAL_POINT_3_PLACE", "ELAPSED_TIME_CENTISECOND",
    "ELAPSED_TIME_MINUTE", "ELAPSED_TIME_SECOND", "MONEY_POUND_DECIMAL", "MONEY_POUND", "MONEY_DOLLAR_DECIMAL",
    "MONEY_DOLLAR", "MONEY_EURO_DECIMAL", "MONEY_EURO", "MONEY_FRANC_DECIMAL", "MONEY_FRANC", "MONEY_KRONER_DECIMAL",
    "MONEY_KRONER", "MONEY_YEN",
)  # fmt: skip
SORTS = ("ASC", "DESC")
SUBMISSIONS = ("BEST_SCORE", "MOST_RECENT_SCORE")

# Apple's limits (App Store Connect help, "Achievements").
MAX_IDENTIFIER = 100
MAX_POINTS_EACH = 100
MAX_POINTS_ALL = 1000
MAX_ACHIEVEMENTS = 100

# The states of a version (GameCenterVersionState). One that is not sent yet is changed in place; one that is with
# Apple is left alone; any other (live, or replaced) cannot be changed, and a difference starts the next version.
EDITABLE = frozenset({"PREPARE_FOR_SUBMISSION", "READY_FOR_REVIEW", "DEVELOPER_REJECTED", "REJECTED"})
WITH_APPLE = frozenset({"WAITING_FOR_REVIEW", "IN_REVIEW", "ACCEPTED", "PENDING_RELEASE"})


# What the file asks for -----------------------------------------------------------------------------------------------


@dataclass(frozen=True)
class Image:
    path: Path
    name: str  # the name it is uploaded under: the slug and the start of its SHA-256, so a new picture has a new name
    size: int


@dataclass(frozen=True)
class Kind:
    """The names the API has for one kind of component."""

    word: str  # "leaderboard" or "achievement"
    type: str  # the type of the resource
    listed: str  # the relationship of the Game Center detail that lists them
    path: str
    version_type: str
    version_path: str
    owner: str  # the relationship of a version that names its component
    words_type: str
    words_path: str
    image_type: str
    image_path: str


LEADERBOARD = Kind(
    word="leaderboard",
    type="gameCenterLeaderboards",
    listed="gameCenterLeaderboardsV2",
    path="/v2/gameCenterLeaderboards",
    version_type="gameCenterLeaderboardVersions",
    version_path="/v2/gameCenterLeaderboardVersions",
    owner="leaderboard",
    words_type="gameCenterLeaderboardLocalizations",
    words_path="/v2/gameCenterLeaderboardLocalizations",
    image_type="gameCenterLeaderboardImages",
    image_path="/v2/gameCenterLeaderboardImages",
)
ACHIEVEMENT = Kind(
    word="achievement",
    type="gameCenterAchievements",
    listed="gameCenterAchievementsV2",
    path="/v2/gameCenterAchievements",
    version_type="gameCenterAchievementVersions",
    version_path="/v2/gameCenterAchievementVersions",
    owner="achievement",
    words_type="gameCenterAchievementLocalizations",
    words_path="/v2/gameCenterAchievementLocalizations",
    image_type="gameCenterAchievementImages",
    image_path="/v2/gameCenterAchievementImages",
)


@dataclass(frozen=True)
class Component:
    kind: Kind
    identifier: str
    attributes: dict[str, Any]  # as the API names them, without the identifier
    words: dict[str, dict[str, str]]  # Apple's locale, then the attributes of its localization
    image: Image | None


class ComponentsError(Exception):
    """components.json, or an image it needs, is not as it must be."""


def slug_of(identifier: str) -> str:
    return identifier.rsplit(".", 1)[-1]


def read_components(path: Path = COMPONENTS, images: Path | None = None) -> list[Component]:
    """Reads components.json into what the API is asked for, leaderboards first. Every fault is named at once."""
    images = images if images is not None else path.parent / "images"
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise ComponentsError(f"{path}: {error}") from error
    faults: list[str] = []
    components: list[Component] = []
    seen: set[str] = set()

    def text(entry: dict[str, Any], key: str, where: str) -> str:
        value = entry.get(key)
        if not isinstance(value, str) or not value.strip():
            faults.append(f'{where}: "{key}" is missing or empty')
            return ""
        return value

    def identifier_of(entry: dict[str, Any], where: str) -> str:
        identifier = text(entry, "id", where)
        if len(identifier) > MAX_IDENTIFIER:
            faults.append(f"{where}: the identifier is longer than {MAX_IDENTIFIER} characters")
        if identifier in seen:
            faults.append(f"{where}: the identifier {identifier} is there twice")
        seen.add(identifier)
        return identifier

    def languages(entry: dict[str, Any], where: str) -> list[tuple[str, dict[str, Any]]]:
        found = [(LOCALES[key], entry[key]) for key in LOCALES if isinstance(entry.get(key), dict)]
        if not any(locale == "en-US" for locale, _ in found):
            faults.append(f'{where}: it has no "en", and English is the language every component must have')
        return found

    for index, entry in enumerate(document.get("leaderboards", [])):
        where = f"leaderboard {index + 1}"
        identifier = identifier_of(entry, where)
        where = f"leaderboard {identifier or index + 1}"
        attributes = {
            "referenceName": text(entry, "referenceName", where),
            "defaultFormatter": text(entry, "scoreFormat", where),
            "scoreSortType": text(entry, "sort", where),
            "submissionType": text(entry, "submission", where),
        }
        for key, allowed in (
            ("defaultFormatter", SCORE_FORMATS),
            ("scoreSortType", SORTS),
            ("submissionType", SUBMISSIONS),
        ):
            if attributes[key] and attributes[key] not in allowed:
                faults.append(f"{where}: {attributes[key]} is not one of {', '.join(allowed)}")
        words = {
            locale: {"name": text(language, "name", f"{where}, {locale}")}
            for locale, language in languages(entry, where)
        }
        components.append(Component(LEADERBOARD, identifier, attributes, words, None))

    points_in_all = 0
    achievements = document.get("achievements", [])
    if len(achievements) > MAX_ACHIEVEMENTS:
        faults.append(f"there are {len(achievements)} achievements, and Apple allows {MAX_ACHIEVEMENTS}")
    for index, entry in enumerate(achievements):
        where = f"achievement {index + 1}"
        identifier = identifier_of(entry, where)
        where = f"achievement {identifier or index + 1}"
        points = entry.get("points")
        if not isinstance(points, int) or isinstance(points, bool) or not 1 <= points <= MAX_POINTS_EACH:
            faults.append(f'{where}: "points" must be a whole number from 1 to {MAX_POINTS_EACH}')
            points = 0
        points_in_all += points
        for key in ("hidden", "achievableMoreThanOnce"):
            if not isinstance(entry.get(key), bool):
                faults.append(f'{where}: "{key}" must be true or false')
        attributes = {
            "referenceName": text(entry, "referenceName", where),
            "points": points,
            # Apple asks whether it is shown before it is earned, which is the other way round from "hidden".
            "showBeforeEarned": not entry.get("hidden", False),
            "repeatable": bool(entry.get("achievableMoreThanOnce", False)),
        }
        words = {
            locale: {
                "name": text(language, "name", f"{where}, {locale}"),
                "beforeEarnedDescription": text(language, "preEarnedDescription", f"{where}, {locale}"),
                "afterEarnedDescription": text(language, "earnedDescription", f"{where}, {locale}"),
            }
            for locale, language in languages(entry, where)
        }
        image = None
        file = images / f"{slug_of(identifier)}.png"
        if identifier:
            try:
                data = file.read_bytes()
            except OSError:
                faults.append(f"{where}: its image {file} is not there")
            else:
                image = Image(file, f"{slug_of(identifier)}-{hashlib.sha256(data).hexdigest()[:12]}.png", len(data))
        components.append(Component(ACHIEVEMENT, identifier, attributes, words, image))
    if points_in_all > MAX_POINTS_ALL:
        faults.append(f"the achievements come to {points_in_all} points, and Apple allows {MAX_POINTS_ALL}")

    if faults:
        raise ComponentsError(f"{path}:\n  " + "\n  ".join(faults))
    return components


# The API ------------------------------------------------------------------------------------------------------------


class ApiError(Exception):
    """A request that App Store Connect refused."""

    def __init__(self, method: str, where: str, status: int, body: bytes) -> None:
        self.status = status
        details: list[str] = []
        try:
            for error in json.loads(body.decode("utf-8")).get("errors", []):
                details.append(" ".join(str(error[key]) for key in ("code", "title", "detail") if error.get(key)))
        except (ValueError, AttributeError, UnicodeDecodeError):
            pass
        super().__init__(f"{method} {where}: HTTP {status}" + "".join(f"\n    {line}" for line in details))


Send = Callable[[str, str, dict[str, str], "bytes | None"], "tuple[int, bytes]"]


def send_over_http(method: str, url: str, headers: dict[str, str], body: bytes | None) -> tuple[int, bytes]:
    """One request. A refusal is returned as its status and body, not raised."""
    request = urllib.request.Request(url, data=body, method=method, headers=headers)  # noqa: S310 (https only, below)
    try:
        with urllib.request.urlopen(request, timeout=60) as response:  # noqa: S310
            return response.status, response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.read()


def der_to_raw(signature: bytes) -> bytes:
    """An ECDSA signature as openssl writes it (DER: a sequence of two integers) as the 64 bytes a token carries."""

    def integer(at: int) -> tuple[int, int]:
        if signature[at] != 0x02:
            raise ValueError("not a DER integer")
        length = signature[at + 1]
        if length & 0x80:
            raise ValueError("an integer too long for a P-256 signature")
        start = at + 2
        return int.from_bytes(signature[start : start + length], "big"), start + length

    if len(signature) < 8 or signature[0] != 0x30:
        raise ValueError("not a DER sequence")
    at = 3 if signature[1] & 0x80 else 2
    r, at = integer(at)
    s, at = integer(at)
    if at != len(signature):
        raise ValueError("bytes after the signature")
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")


def sign_with_openssl(private_key: str, message: bytes) -> bytes:
    """Signs with the key of a .p8 file (ES256). The key is on disk, readable by this user alone, only while openssl
    runs."""
    handle, name = tempfile.mkstemp(prefix="authkey-", suffix=".p8")
    try:
        with os.fdopen(handle, "w", encoding="ascii") as file:
            file.write(private_key.strip() + "\n")
        done = subprocess.run(  # noqa: S603
            ["openssl", "dgst", "-sha256", "-sign", name],  # noqa: S607
            input=message,
            capture_output=True,
            check=False,
        )
    finally:
        os.unlink(name)
    if done.returncode != 0:
        # openssl's own words name the fault and never the key.
        raise RuntimeError(
            "openssl could not sign with APPSTORE_API_PRIVATE_KEY: " + done.stderr.decode(errors="replace").strip()
        )
    return der_to_raw(done.stdout)


def b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


class Token:
    """The token of an App Store Connect API key, made again before it runs out (Apple allows one 20 minutes)."""

    LIFETIME = 600
    MARGIN = 120

    def __init__(
        self,
        key_id: str,
        issuer_id: str,
        private_key: str,
        sign: Callable[[str, bytes], bytes] = sign_with_openssl,
        now: Callable[[], float] = time.time,
    ) -> None:
        self._key_id = key_id
        self._issuer_id = issuer_id
        self._private_key = private_key
        self._sign = sign
        self._now = now
        self._token = ""
        self._expires = 0.0

    def __call__(self) -> str:
        now = self._now()
        if not self._token or now > self._expires - self.MARGIN:
            header = {"alg": "ES256", "kid": self._key_id, "typ": "JWT"}
            claims = {
                "iss": self._issuer_id,
                "iat": int(now),
                "exp": int(now) + self.LIFETIME,
                "aud": "appstoreconnect-v1",
            }
            signed = (
                b64url(json.dumps(header, separators=(",", ":")).encode())
                + "."
                + b64url(json.dumps(claims, separators=(",", ":")).encode())
            )
            self._token = signed + "." + b64url(self._sign(self._private_key, signed.encode("ascii")))
            self._expires = now + self.LIFETIME
        return self._token


class Api:
    """Requests to App Store Connect. The token goes to Apple's API host and nowhere else."""

    TRIES = 4

    def __init__(
        self, token: Callable[[], str], send: Send = send_over_http, sleep: Callable[[float], None] = time.sleep
    ) -> None:
        self._token = token
        self._send = send
        self._sleep = sleep

    def call(
        self, method: str, path: str, body: dict[str, Any] | None = None, query: dict[str, str] | None = None
    ) -> dict[str, Any]:
        url = path if path.startswith(API + "/") else API + path
        if not url.startswith(API + "/"):
            raise ValueError(f"{url} is not Apple's API")
        if query:
            url += "?" + urllib.parse.urlencode(query)
        data = json.dumps(body).encode("utf-8") if body is not None else None
        for attempt in range(1, self.TRIES + 1):
            headers = {"Authorization": "Bearer " + self._token(), "Accept": "application/json"}
            if data is not None:
                headers["Content-Type"] = "application/json"
            status, answer = self._send(method, url, headers, data)
            # Too many requests was not acted on, so anything may be asked again; a fault of the server is only asked
            # again for a read, since a write may have happened.
            again = status == 429 or (method == "GET" and status >= 500)
            if not again or attempt == self.TRIES:
                break
            self._sleep(2.0**attempt)
        where = urllib.parse.urlsplit(url).path
        if status == 204 or (200 <= status < 300 and not answer):
            return {}
        if not 200 <= status < 300:
            raise ApiError(method, where, status, answer)
        return json.loads(answer.decode("utf-8"))

    def every(
        self, path: str, query: dict[str, str] | None = None
    ) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
        """Every page of a list: the resources, and what came with them."""
        data: list[dict[str, Any]] = []
        included: list[dict[str, Any]] = []
        page = self.call("GET", path, query={"limit": "200", **(query or {})})
        while True:
            data += page.get("data") or []
            included += page.get("included") or []
            following = (page.get("links") or {}).get("next")
            if not following:
                return data, included
            page = self.call("GET", following)

    def upload(self, operations: list[dict[str, Any]], data: bytes) -> None:
        """Sends a file in the parts Apple asked for. These go to Apple's storage, without the token."""
        for operation in operations:
            headers = {header["name"]: header["value"] for header in operation.get("requestHeaders") or []}
            part = data[operation.get("offset", 0) : operation.get("offset", 0) + operation.get("length", len(data))]
            status, answer = self._send(operation["method"], operation["url"], headers, part)
            if not 200 <= status < 300:
                raise ApiError(operation["method"], "the upload of an image", status, answer)


# The comparison and the changes ---------------------------------------------------------------------------------------


def linked(kind_of_resource: str, identifier: str) -> dict[str, Any]:
    return {"data": {"type": kind_of_resource, "id": identifier}}


@dataclass
class Report:
    """What a run found: the lines to show, how many changes, and what was left for a person."""

    apply: bool
    lines: list[str] = field(default_factory=list)
    changes: int = 0
    left: list[str] = field(default_factory=list)

    def say(self, line: str) -> None:
        self.lines.append(line)

    def change(self, line: str) -> None:
        self.changes += 1
        self.lines.append(("changed: " if self.apply else "would change: ") + line)

    def leave(self, line: str) -> None:
        self.left.append(line)
        self.lines.append("left alone: " + line)


class Sync:
    def __init__(self, api: Api, report: Report) -> None:
        self._api = api
        self._report = report

    @property
    def apply(self) -> bool:
        return self._report.apply

    def run(self, components: list[Component], bundle_id: str = BUNDLE_ID) -> Report:
        api, report = self._api, self._report
        apps, _ = api.every("/v1/apps", {"filter[bundleId]": bundle_id})
        apps = [app for app in apps if app.get("attributes", {}).get("bundleId") == bundle_id]
        if not apps:
            raise ComponentsError(
                f"App Store Connect has no app with the bundle identifier {bundle_id} that this key can see"
            )
        app_id = apps[0]["id"]

        try:
            detail = api.call("GET", f"/v1/apps/{app_id}/gameCenterDetail").get("data")
        except ApiError as error:
            if error.status != 404:
                raise
            detail = None
        detail_id = detail["id"] if detail else None
        if detail_id is None:
            report.change(f"turn Game Center on for {bundle_id}")
            if self.apply:
                body = {"data": {"type": "gameCenterDetails", "relationships": {"app": linked("apps", app_id)}}}
                detail_id = api.call("POST", "/v1/gameCenterDetails", body)["data"]["id"]

        for kind in (LEADERBOARD, ACHIEVEMENT):
            wanted = [component for component in components if component.kind is kind]
            there: dict[str, dict[str, Any]] = {}
            if detail_id is not None:
                listed, _ = api.every(f"/v1/gameCenterDetails/{detail_id}/{kind.listed}")
                there = {resource["attributes"]["vendorIdentifier"]: resource for resource in listed}
            for component in wanted:
                self.component(component, there.get(component.identifier), detail_id)
            names = {component.identifier for component in wanted}
            for identifier, resource in there.items():
                if identifier not in names and not resource["attributes"].get("archived"):
                    report.leave(f"{kind.word} {identifier} is in App Store Connect and not in components.json")
        return report

    def component(self, component: Component, resource: dict[str, Any] | None, detail_id: str | None) -> None:
        api, report, kind = self._api, self._report, component.kind
        name = f"{kind.word} {component.identifier}"
        if resource is None:
            facts = ", ".join(f"{key} {value}" for key, value in component.attributes.items() if key != "referenceName")
            report.change(f"create {name} ({facts})")
            if not self.apply:
                self.words(component, name, None, {}, {})
                return
            body = {
                "data": {
                    "type": kind.type,
                    "attributes": {"vendorIdentifier": component.identifier, **component.attributes},
                    "relationships": {
                        "gameCenterDetail": linked("gameCenterDetails", str(detail_id)),
                        # Apple makes the first version with the component: a resource to make in the same request
                        # has an identifier of this form, and is described under "included".
                        "versions": {"data": [{"type": kind.version_type, "id": "${version-1}"}]},
                    },
                },
                "included": [{"type": kind.version_type, "id": "${version-1}"}],
            }
            resource = api.call("POST", kind.path, body)["data"]
        else:
            if resource["attributes"].get("archived"):
                report.leave(f"{name} is archived in App Store Connect")
                return
            differs = {
                key: value for key, value in component.attributes.items() if resource["attributes"].get(key) != value
            }
            if differs:
                was = ", ".join(
                    f"{key} {resource['attributes'].get(key)!r} to {value!r}" for key, value in differs.items()
                )
                report.change(f"{name}: {was}")
                if self.apply:
                    api.call(
                        "PATCH",
                        f"{kind.path}/{resource['id']}",
                        {"data": {"type": kind.type, "id": resource["id"], "attributes": differs}},
                    )

        versions, _ = api.every(f"{kind.path}/{resource['id']}/versions")
        latest = max(versions, key=lambda version: version["attributes"].get("version") or 0, default=None)
        if latest is None:
            self.new_version(component, name, resource["id"], None)
            return
        state = latest["attributes"].get("state") or ""
        number = latest["attributes"].get("version")
        words, images = self.read_words(kind, latest["id"])
        if not self.differences(component, words, images):
            return
        if state in WITH_APPLE:
            report.leave(
                f"{name} differs from components.json, and its version {number} is with Apple ({state}); "
                "run this again when Apple has decided"
            )
        elif state in EDITABLE:
            self.words(component, name, latest["id"], words, images)
        else:
            self.new_version(component, name, resource["id"], (number, state, words, images))

    def new_version(
        self,
        component: Component,
        name: str,
        resource_id: str,
        was: tuple[Any, str, dict[str, Any], dict[str, Any]] | None,
    ) -> None:
        api, report, kind = self._api, self._report, component.kind
        reason = f"version {was[0]} is {was[1]} and cannot be changed" if was else "it has no version"
        report.change(
            f"{name}: start a new version ({reason}); the owner sends it for review with a version of the app"
        )
        if not self.apply:
            self.words(component, name, None, was[2] if was else {}, was[3] if was else {})
            return
        body = {"data": {"type": kind.version_type, "relationships": {kind.owner: linked(kind.type, resource_id)}}}
        version_id = api.call("POST", kind.version_path, body)["data"]["id"]
        # Whether Apple starts a version empty or with the words of the one before is not written down, so it is read.
        words, images = self.read_words(kind, version_id)
        self.words(component, name, version_id, words, images)

    def read_words(self, kind: Kind, version_id: str) -> tuple[dict[str, dict[str, Any]], dict[str, dict[str, Any]]]:
        """The localizations of a version by locale, and the image of each by the identifier of its localization."""
        listed, included = self._api.every(f"{kind.version_path}/{version_id}/localizations", {"include": "image"})
        by_id = {resource["id"]: resource for resource in included if resource.get("type") == kind.image_type}
        words = {resource["attributes"]["locale"]: resource for resource in listed}
        images: dict[str, dict[str, Any]] = {}
        for resource in listed:
            image = ((resource.get("relationships") or {}).get("image") or {}).get("data")
            if image and image.get("id") in by_id:
                images[resource["id"]] = by_id[image["id"]]
        return words, images

    @staticmethod
    def same_image(wanted: Image, there: dict[str, Any] | None) -> bool:
        if there is None:
            return False
        attributes = there.get("attributes") or {}
        if (attributes.get("assetDeliveryState") or {}).get("state") in ("AWAITING_UPLOAD", "FAILED"):
            return False
        if attributes.get("fileName") == wanted.name:
            return True
        # An image that was not put there by this script (by hand, say) has another name: its size is all there is to
        # go by. One this script put there carries the start of its hash in its name, so a new picture always shows.
        ours = re.fullmatch(r".+-[0-9a-f]{12}\.png", attributes.get("fileName") or "")
        return ours is None and attributes.get("fileSize") == wanted.size

    def differences(
        self, component: Component, words: dict[str, dict[str, Any]], images: dict[str, dict[str, Any]]
    ) -> list[str]:
        found: list[str] = []
        for locale, wanted in component.words.items():
            there = words.get(locale)
            if there is None:
                found.append(f"{locale} is missing")
                continue
            for key, value in wanted.items():
                if there["attributes"].get(key) != value:
                    found.append(f"{locale} {key}")
            if component.image is not None and not self.same_image(component.image, images.get(there["id"])):
                found.append(f"{locale} image")
        return found

    def words(
        self,
        component: Component,
        name: str,
        version_id: str | None,
        words: dict[str, dict[str, Any]],
        images: dict[str, dict[str, Any]],
    ) -> None:
        """Brings the localizations of a version to the file. Without a version (a dry run of one not made yet) it only
        says."""
        api, report, kind = self._api, self._report, component.kind
        for locale, wanted in component.words.items():
            there = words.get(locale)
            if there is None:
                report.change(f'{name}: add {locale} "{wanted["name"]}"')
                if self.apply:
                    body = {
                        "data": {
                            "type": kind.words_type,
                            "attributes": {"locale": locale, **wanted},
                            "relationships": {"version": linked(kind.version_type, str(version_id))},
                        }
                    }
                    there = api.call("POST", kind.words_path, body)["data"]
            else:
                differs = {key: value for key, value in wanted.items() if there["attributes"].get(key) != value}
                if differs:
                    report.change(
                        f"{name}: {locale} " + ", ".join(f'{key} to "{value}"' for key, value in differs.items())
                    )
                    if self.apply and version_id is not None:
                        api.call(
                            "PATCH",
                            f"{kind.words_path}/{there['id']}",
                            {"data": {"type": kind.words_type, "id": there["id"], "attributes": differs}},
                        )
            if component.image is None:
                continue
            old = images.get(there["id"]) if there is not None else None
            if self.same_image(component.image, old):
                continue
            report.change(f"{name}: {locale} image {component.image.name}")
            if not self.apply or there is None or version_id is None:
                continue
            if old is not None:
                api.call("DELETE", f"{kind.image_path}/{old['id']}")
            body = {
                "data": {
                    "type": kind.image_type,
                    "attributes": {"fileName": component.image.name, "fileSize": component.image.size},
                    "relationships": {"localization": linked(kind.words_type, there["id"])},
                }
            }
            image = api.call("POST", kind.image_path, body)["data"]
            api.upload(image["attributes"].get("uploadOperations") or [], component.image.path.read_bytes())
            api.call(
                "PATCH",
                f"{kind.image_path}/{image['id']}",
                {"data": {"type": kind.image_type, "id": image["id"], "attributes": {"uploaded": True}}},
            )
        for locale in words:
            if locale not in component.words:
                report.leave(f"{name} has {locale} in App Store Connect, and components.json has no such language")


# The program ----------------------------------------------------------------------------------------------------------


def summary_of(report: Report, bundle_id: str) -> str:
    """The run as Markdown, for the summary of a workflow run."""
    title = "Game Center changes made" if report.apply else "Game Center changes that would be made (dry run)"
    lines = [
        f"## {title}",
        "",
        f"App: `{bundle_id}`. Record: `apps/snake/packaging/ios/game-center/components.json`.",
        "",
    ]
    if not report.lines:
        lines.append("None. App Store Connect matches the file.")
    lines += [f"- {line}" for line in report.lines]
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None, send: Send = send_over_http) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--apply", action="store_true", help="make the changes (without it nothing is changed)")
    parser.add_argument(
        "--validate", action="store_true", help="check components.json and the images, and ask Apple nothing"
    )
    parser.add_argument("--components", type=Path, default=COMPONENTS, help="the file that is the record")
    parser.add_argument(
        "--images", type=Path, default=None, help="where the achievement images are (default: images/ beside the file)"
    )
    parser.add_argument("--bundle-id", default=BUNDLE_ID, help="the bundle identifier of the app")
    parser.add_argument("--summary", type=Path, default=None, help="append the run as Markdown to this file")
    parser.add_argument(
        "--output", type=Path, default=None, help="append changes=<number> to this file (GITHUB_OUTPUT)"
    )
    arguments = parser.parse_args(argv)

    try:
        components = read_components(arguments.components, arguments.images)
    except ComponentsError as error:
        print(error, file=sys.stderr)
        return 1
    if arguments.validate:
        counts = {
            kind.word: sum(1 for component in components if component.kind is kind)
            for kind in (LEADERBOARD, ACHIEVEMENT)
        }
        print(
            f"{arguments.components.name} is in order: {counts['leaderboard']} leaderboards and "
            f"{counts['achievement']} achievements, each with its image."
        )
        return 0

    missing = [
        name
        for name in ("APPSTORE_API_KEY_ID", "APPSTORE_API_ISSUER_ID", "APPSTORE_API_PRIVATE_KEY")
        if not os.environ.get(name)
    ]
    if missing:
        print(
            "Not set: " + ", ".join(missing) + ". They are the App Store Connect API key (docs/ios-install.md).",
            file=sys.stderr,
        )
        return 1
    token = Token(
        os.environ["APPSTORE_API_KEY_ID"], os.environ["APPSTORE_API_ISSUER_ID"], os.environ["APPSTORE_API_PRIVATE_KEY"]
    )
    report = Report(apply=arguments.apply)
    failed = False
    try:
        Sync(Api(token, send), report).run(components, arguments.bundle_id)
    except (ApiError, ComponentsError, RuntimeError, ValueError) as error:
        failed = True
        report.say(f"stopped: {error}")

    for line in report.lines:
        print(line)
    if failed:
        print(
            "\nStopped at the fault above. What is marked as changed before it was changed; "
            "run again once it is put right.",
            file=sys.stderr,
        )
    elif report.changes == 0:
        print(
            "No changes: App Store Connect matches components.json."
            + (f" {len(report.left)} left alone, above." if report.left else "")
        )
    elif report.apply:
        print(
            f"\n{report.changes} changes made. "
            "Nothing was added to a version of the app or sent for review: that is the owner's."
        )
    else:
        print(f"\n{report.changes} changes would be made. Run with --apply to make them.")
    if arguments.summary is not None:
        with arguments.summary.open("a", encoding="utf-8") as file:
            file.write(summary_of(report, arguments.bundle_id))
    if arguments.output is not None:
        with arguments.output.open("a", encoding="utf-8") as file:
            file.write(f"changes={report.changes}\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
