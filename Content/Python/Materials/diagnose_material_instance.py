"""Read-only Unreal Editor material-instance diagnostic.

Select a single Material Instance (MI) in Content Browser and run this script.
Selecting multiple assets is unsupported: a warning is logged and nothing runs.
Outputs a timestamped JSON report to <Project>/Saved/MaterialDiagnostics.

No setters, asset saves, recompiles, resets, imports, or repair operations.
Loading assets can still invoke Unreal's normal load-time processing.
This script inspects loaded UObject state, not the original binary .uasset serialization.

Reports each instance's stored parameter overrides (including scalar atlas
flags) along the parent chain, plus a list of potential problems ("findings").
Python exposure varies by engine build. Failed optional probes are recorded;
unavailable atlas data is UNKNOWN, never assumed false.
"""

import datetime
import json
import math
import os
import re
import traceback

import unreal

ASSET_PATH = ""  # e.g. /Game/MyPack/Materials/MI_PostProcess
MISSING = object()
ARRAYS = {
    "scalar": "scalar_parameter_values",
    "vector": "vector_parameter_values",
    "texture": "texture_parameter_values",
    "double_vector": "double_vector_parameter_values",
    "font": "font_parameter_values",
    "runtime_virtual_texture": "runtime_virtual_texture_parameter_values",
    "sparse_volume_texture": "sparse_volume_texture_parameter_values",
}


def prop(obj, name):
    try:
        return obj.get_editor_property(name), None
    except Exception as exc:
        return MISSING, str(exc)


def raw(value):
    try:
        return value.export_text()
    except Exception:
        return str(value)


def serial(value):
    if value is MISSING:
        return {"unavailable": True}
    if value is None or isinstance(value, (str, bool, int)):
        return value
    if isinstance(value, float):
        return value if math.isfinite(value) else str(value)
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    return raw(value)


def fields(obj, names):
    result = {}
    for name in names:
        value, error = prop(obj, name)
        result[name] = {"unavailable": error} if error else serial(value)
    return result


def finding(report, level, code, message, **evidence):
    report["findings"].append(dict(level=level, code=code,
                                   message=message, evidence=evidence))


def atlas_flag(text):
    match = re.search(r"\b(?:bIsUsedAsAtlasPosition|is_used_as_atlas_position)"
                      r"\s*=\s*(True|False|1|0)\b", text, re.IGNORECASE)
    return None if not match else match.group(1).lower() in ("true", "1")


def read_override(entry, kind):
    data = fields(entry, ("parameter_info", "parameter_value", "expression_guid"))
    data["raw_struct"] = raw(entry)
    info, error = prop(entry, "parameter_info")
    data["identity"] = (fields(info, ("name", "association", "index"))
                        if not error else {"unavailable": error})
    if kind == "scalar":
        atlas, error = prop(entry, "atlas_data")
        data["atlas_data"] = ({"unavailable": error} if error else
                              fields(atlas, ("is_used_as_atlas_position", "atlas", "curve")))
        data["atlas_flag"] = atlas_flag(data["raw_struct"])
        data["atlas_flag_source"] = ("struct text" if data["atlas_flag"] is not None
                                     else "unavailable")
        if not error:
            flag, flag_error = prop(atlas, "is_used_as_atlas_position")
            if not flag_error and isinstance(flag, bool):
                data["atlas_flag"] = flag
                data["atlas_flag_source"] = "atlas_data property"
            data["atlas_raw_struct"] = raw(atlas)
    return data


def snapshot_instance(instance, report):
    node = {"path": instance.get_path_name(),
            "class": instance.get_class().get_name(), "overrides": {}}
    for kind, property_name in ARRAYS.items():
        entries, error = prop(instance, property_name)
        if error:
            node["overrides"][kind] = {"unavailable": error}
            continue
        rows = [read_override(entry, kind) for entry in entries]
        node["overrides"][kind] = rows
        seen = set()
        for row in rows:
            identity = row["identity"]
            # Full identity preserves association/index; FName is case-insensitive.
            key = json.dumps(identity, sort_keys=True).casefold()
            if key in seen and "unavailable" not in identity:
                finding(report, "warning", "DUPLICATE_OVERRIDE",
                        "Repeated parameter identity in the same typed override array.",
                        asset=node["path"], kind=kind, identity=identity)
            seen.add(key)
            if kind == "scalar" and row.get("atlas_flag") is True:
                finding(report, "review", "SCALAR_ATLAS_FLAG",
                        "Scalar is marked as an atlas position. Legitimate for curve rows; "
                        "suspicious for an ordinary numeric parameter.",
                        asset=node["path"], identity=identity,
                        atlas_data=row["atlas_data"], raw=row["raw_struct"])
            if kind == "scalar" and row.get("parameter_value") in ("nan", "inf", "-inf"):
                finding(report, "warning", "NONFINITE_SCALAR",
                        "Scalar contains a non-finite value.", asset=node["path"], row=row)
    editor_data, error = prop(instance, "editor_only_data")
    node["editor_only_data"] = serial(editor_data)
    if error:
        node["editor_only_data_error"] = error
    else:
        # These are probes, not assumptions about Python exposure in this build.
        node["editor_metadata_probes"] = fields(editor_data, (
            "scalar_parameter_values", "scalar_parameter_atlas_data", "static_parameters"))
    return node


def inspect(instance):
    report = {"asset": instance.get_path_name(), "findings": [], "parent_chain": [],
              "limitations": [
                  "Loaded UObject state only; no binary package/version audit.",
                  "Unavailable metadata is unknown, not a clean bill of health.",
                  "The base material graph is not inspected."]}
    current = instance
    visited = set()
    while current is not None:
        path = current.get_path_name()
        if path in visited:
            finding(report, "warning", "PARENT_CYCLE", "Cycle in parent chain.", asset=path)
            break
        visited.add(path)
        if isinstance(current, unreal.Material):
            report["parent_chain"].append({"path": path, "class": "Material"})
            break
        if not isinstance(current, unreal.MaterialInstance):
            finding(report, "warning", "UNEXPECTED_PARENT", "Unexpected parent class.", asset=path)
            break
        report["parent_chain"].append(snapshot_instance(current, report))
        parent, error = prop(current, "parent")
        if error or parent is None:
            finding(report, "warning", "PARENT_UNAVAILABLE", "Could not reach a base material.",
                    asset=path, reason=error)
            break
        current = parent

    library = unreal.MaterialEditingLibrary
    inventory = report["parameter_name_inventory"] = {}
    for kind in ("scalar", "vector", "texture", "static_switch"):
        try:
            names = getattr(library, "get_" + kind + "_parameter_names")(instance)
            inventory[kind] = sorted(str(n) for n in names)
        except Exception as exc:
            inventory[kind] = {"unavailable": str(exc)}

    # Name collisions are leads, not proof: association and layer index matter.
    types_by_name = {}
    for kind, names in inventory.items():
        if isinstance(names, list):
            for name in names:
                types_by_name.setdefault(name.casefold(), []).append(kind)
    for name, kinds in sorted(types_by_name.items()):
        if len(kinds) > 1:
            finding(report, "review", "MULTIPLE_REPORTED_TYPES",
                    "Name appears in multiple type inventories; check association/scope.",
                    name=name, types=kinds)
    return report


def get_target_instance():
    """Return the single MaterialInstanceConstant to inspect, or None if unsupported."""
    if ASSET_PATH:
        selected = [unreal.load_asset(ASSET_PATH)]
    else:
        selected = unreal.EditorUtilityLibrary.get_selected_assets()
    if len(selected) > 1:
        unreal.log_warning("[MI diagnostic] Multiple assets selected (%d); this script "
                           "supports a single Material Instance only. Select one and rerun."
                           % len(selected))
        return None
    if not selected or not isinstance(selected[0], unreal.MaterialInstanceConstant):
        raise RuntimeError("Select a MaterialInstanceConstant in Content Browser, or set ASSET_PATH.")
    return selected[0]


def main():
    instance = get_target_instance()
    if instance is None:
        return None
    output = {"script_version": 2, "engine_version": unreal.SystemLibrary.get_engine_version(),
              "timestamp_utc": datetime.datetime.now(datetime.timezone.utc).isoformat()}
    unreal.log("[MI diagnostic] Inspecting " + instance.get_path_name())
    try:
        report = inspect(instance)
        output["report"] = report
        for item in report["findings"]:
            unreal.log("[MI diagnostic] " + item["code"] + ": " + item["message"])
        unreal.log("[MI diagnostic] %d finding(s)." % len(report["findings"]))
    except Exception:
        output["report"] = {"asset": instance.get_path_name(),
                            "inspection_error": traceback.format_exc()}
        unreal.log_warning("[MI diagnostic] Inspection incomplete; see report for traceback.")
    folder = os.path.abspath(os.path.join(unreal.Paths.project_saved_dir(), "MaterialDiagnostics"))
    os.makedirs(folder, exist_ok=True)
    timestamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d_%H%M%S_%f")
    filename = os.path.join(folder, "material_diagnostic_" + timestamp + ".json")
    with open(filename, "w", encoding="utf-8") as handle:
        json.dump(output, handle, indent=2, ensure_ascii=False, allow_nan=False)
    unreal.log("[MI diagnostic] Report: " + filename)
    unreal.log("[MI diagnostic] No asset writes or repairs requested by this script.")
    return filename


if __name__ == "__main__":
    main()
