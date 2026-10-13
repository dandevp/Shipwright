#!/usr/bin/env python3
"""Compare the apworld's location and item names with the ones the game knows.

The client matches Archipelago locations and items by name (StaticData::locationNameToEnum / itemNameToEnum), so
an apworld name that differs from the game's by a single character silently breaks that check.

The game builds a location's name at runtime: "<area prefix> <short name>", where the area is either passed
explicitly (RCAREA_*) or derived from the scene (GetAreaFromScene). This script rebuilds those names from the C++
sources.

Usage: check_names.py [--missing]    (--missing also lists the game's locations the apworld doesn't have, by kind)
"""

import ast
import re
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
RANDO = REPO / "soh/soh/Enhancements/randomizer"
APWORLD = REPO / "archipelago/oot_soh"

STRING = r'"(?:[^"\\]|\\.)*"'

# Items with an id that only exist for logic (can_use), never placed or sent
LOGIC_ONLY_ITEMS = {"Scarecrow", "Distant Scarecrow"}


def area_prefixes() -> dict[str, str]:
    enum = (RANDO / "randomizerEnums/RandomizerMiscEnums.h").read_text()
    block = enum[enum.index("RANDO_ENUM_BEGIN(RandomizerCheckArea)"):]
    block = block[: block.index("RANDO_ENUM_END")]
    areas = re.findall(r"RANDO_ENUM_ITEM\((RCAREA_\w+)\)", block)
    util = (REPO / "soh/soh/util.cpp").read_text()
    table = util[util.index("rcareaPrefixes = {"):]
    table = table[: table.index("};")]
    prefixes = [ast.literal_eval(s) for s in re.findall(STRING, table)]
    return dict(zip(areas, prefixes))


def scene_areas() -> dict[str, str]:
    src = (RANDO / "location.cpp").read_text()
    body = src[src.index("RandomizerCheckArea GetAreaFromScene"):]
    body = body[: body.index("\n}\n")]
    result, pending = {}, []
    for token in re.finditer(r"case (SCENE_\w+):|return (RCAREA_\w+);", body):
        if token.group(1):
            pending.append(token.group(1))
        else:
            for scene in pending:
                result[scene] = token.group(2)
            pending = []
    return result


def split_args(args: str) -> list[str]:
    parts, depth, current, in_string = [], 0, "", False
    i = 0
    while i < len(args):
        c = args[i]
        if in_string:
            current += c
            if c == "\\":
                current += args[i + 1]
                i += 1
            elif c == '"':
                in_string = False
        elif c == '"':
            in_string = True
            current += c
        elif c in "({":
            depth += 1
            current += c
        elif c in ")}":
            depth -= 1
            current += c
        elif c == "," and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += c
        i += 1
    if current.strip():
        parts.append(current.strip())
    return parts


def game_locations() -> dict[str, tuple[str, str]]:
    """Location name -> (RC enum, Location:: kind)."""
    prefixes, scenes = area_prefixes(), scene_areas()
    names = {}
    statement = re.compile(r"locationTable\[(RC_\w+)\]\s*=\s*Location::(\w+)\(", re.S)
    for path in sorted(RANDO.glob("*.cpp")):
        src = path.read_text()
        for m in statement.finditer(src):
            # the arguments run to the matching parenthesis
            depth, i = 1, m.end()
            while depth:
                c = src[i]
                if c == '"':
                    i = re.compile(STRING).match(src, i).end()
                    continue
                depth += c == "("
                depth -= c == ")"
                i += 1
            args = split_args(src[m.end(): i - 1])
            strings = [j for j, a in enumerate(args) if re.fullmatch(STRING, a)]
            if not strings:
                continue
            short = ast.literal_eval(args[strings[0]])
            if len(strings) > 1 and strings[1] == strings[0] + 1:
                name = ast.literal_eval(args[strings[1]])  # explicit spoiler name
            else:
                area = next((a for a in args if a.startswith("RCAREA_")), None)
                if area is None:
                    scene = next((a for a in args if a.startswith("SCENE_")), None)
                    area = scenes.get(scene, "RCAREA_INVALID")
                name = short if area not in prefixes else f"{prefixes[area]} {short}"
            names[name] = (m.group(1), m.group(2))
    return names


def game_items() -> set[str]:
    names = set()
    for path in sorted(RANDO.glob("*.cpp")):
        for m in re.finditer(r"itemTable\[RG_\w+\]\s*=\s*Item\(\s*RG_\w+,\s*Text\{\s*(" + STRING + ")", path.read_text()):
            names.add(ast.literal_eval(m.group(1)))
    return names


def apworld_enum(cls: str) -> dict[str, str]:
    src = (APWORLD / "Enums.py").read_text()
    body = src[src.index(f"class {cls}(StrEnum):"):]
    body = body[body.index("\n") + 1:]
    body = body[: re.search(r"^\S", body, re.M).start()]
    return {k: ast.literal_eval(v) for k, v in re.findall(r"^\s+(\w+)\s*=\s*(" + STRING + ")", body, re.M)}


def apworld_table(path: str, data: str, enum: str) -> list[str]:
    """Enum members used as keys of the file's data tables, skipping commented-out lines."""
    src = (APWORLD / path).read_text()
    return re.findall(rf"^\s*{enum}\.(\w+)\s*:\s*{data}\((?!None)", src, re.M)


def main() -> int:
    locations = game_locations()
    items = game_items()
    ap_locations = apworld_enum("Locations")
    ap_items = apworld_enum("Items")
    used_locations = apworld_table("Locations.py", "SohLocData", "Locations")
    used_items = apworld_table("Items.py", "SohItemData", "Items")

    status = 0
    bad = [ap_locations[m] for m in used_locations if ap_locations[m] not in locations]
    print(f"apworld locations: {len(used_locations)}, game locations: {len(locations)}")
    if bad:
        status = 1
        print(f"\napworld locations the game doesn't know ({len(bad)}):")
        for name in bad:
            print(f"  {name}")

    bad = [ap_items[m] for m in used_items if ap_items[m] not in items | LOGIC_ONLY_ITEMS]
    print(f"\napworld items: {len(used_items)}, game items: {len(items)}")
    if bad:
        status = 1
        print(f"\napworld items the game doesn't know ({len(bad)}):")
        for name in bad:
            print(f"  {name}")

    if "--missing" in sys.argv:
        have = {ap_locations[m] for m in used_locations}
        by_kind = defaultdict(list)
        for name, (rc, kind) in locations.items():
            if name not in have:
                by_kind[kind].append(name)
        print("\ngame locations the apworld doesn't have, by kind:")
        for kind, names in sorted(by_kind.items(), key=lambda kv: -len(kv[1])):
            print(f"  {kind}: {len(names)}")
            for name in sorted(names):
                print(f"    {name}")
    return status


if __name__ == "__main__":
    sys.exit(main())
