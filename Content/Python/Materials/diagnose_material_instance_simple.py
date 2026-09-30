"""Minimal read-only material-instance parameter report.

Select a single Material Instance in Content Browser and run this script.
For every parameter overridden anywhere in the parent chain it reports:
  - name and type
  - the current value (number, asset path, ...); the override closest to the
    selected instance wins
  - the base material's default value, when available
  - "suspicious": only present when a check trips (see flag_suspicious)
  - the raw engine struct text (shows fields the editor UI may hide,
    e.g. the scalar atlas-position flag)

Writes <Project>/Saved/MaterialDiagnostics/material_summary_<timestamp>.json.
Nothing is modified.
"""

import datetime
import json
import math
import os
import re

import unreal

ASSET_PATH = ""  # e.g. /Game/MyPack/Materials/MI_PostProcess; empty = use selection
MAX_LOG_LINES = 40  # first N lines of the JSON echoed to the Output Log; 0 = none
OPEN_REPORT = False  # also open the JSON in the OS default app after writing

# kind -> (override array on the instance, MaterialEditingLibrary default getter)
KINDS = {
    "scalar": ("scalar_parameter_values", "get_material_default_scalar_parameter_value"),
    "texture": ("texture_parameter_values", "get_material_default_texture_parameter_value"),
}
NAME_GETTERS = {"scalar": "get_scalar_parameter_names", "texture": "get_texture_parameter_names"}


def raw(value):
    try:
        return value.export_text()
    except Exception:
        return str(value)


def serial(value):
    if value is None or isinstance(value, (str, bool, int)):
        return value
    if isinstance(value, float):
        return value if math.isfinite(value) else str(value)
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    return raw(value)


def get(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None


def parent_chain(instance):
    """Selected instance first, base material last."""
    chain, seen, current = [], set(), instance
    while current is not None and current.get_path_name() not in seen:
        seen.add(current.get_path_name())
        chain.append(current)
        current = get(current, "parent") if isinstance(current, unreal.MaterialInstance) else None
    return chain


def collect_parameters(chain):
    params = {}
    for asset in chain:  # closest to the selected instance first
        if not isinstance(asset, unreal.MaterialInstance):
            continue
        for kind, (array_name, _) in KINDS.items():
            for entry in get(asset, array_name) or []:
                info = get(entry, "parameter_info")
                name = str(get(info, "name"))
                association = getattr(get(info, "association"), "name", None)
                index = get(info, "index")
                key = (kind, name, association, index)
                if key in params:
                    continue  # already overridden by a closer instance
                param = {"name": name, "kind": kind,
                         "value": serial(get(entry, "parameter_value"))}
                if association != "GLOBAL_PARAMETER":
                    param["association"], param["index"] = association, index
                param["raw"] = raw(entry)
                params[key] = param
    return params


def add_base_defaults(params, base):
    if not isinstance(base, unreal.Material):
        return
    library = unreal.MaterialEditingLibrary
    for param in params.values():
        getter = getattr(library, KINDS[param["kind"]][1], None)
        try:
            # Global-association lookup only; failure just means no default is shown.
            value = getter(base, param["name"])
        except Exception:
            continue
        if isinstance(value, tuple):  # (value, success) in some builds
            value = value[0]
        param["default"] = serial(value)


def flag_suspicious(params, instance):
    """Add a "suspicious" list to parameters that trip a check. Leads, not proof."""
    library = unreal.MaterialEditingLibrary
    known = {}
    for kind, getter in NAME_GETTERS.items():
        try:
            known[kind] = {str(n).casefold() for n in getattr(library, getter)(instance)}
        except Exception:
            pass  # inventory unavailable: skip the name check for this kind
    for param in params.values():
        text, reasons = param["raw"], []
        if re.search(r"ExpressionGUID=0{32}", text):
            reasons.append("zero_expression_guid")  # override not bound to a parent expression
        if (param["kind"] == "scalar" and re.search(r"bIsUsedAsAtlasPosition=True", text)
                and re.search(r"Curve=None|Atlas=None", text)):
            reasons.append("atlas_flag_without_curve")
        if param["kind"] in known and param["name"].casefold() not in known[param["kind"]]:
            reasons.append("not_in_parameter_names")  # possibly orphaned override
        if reasons:
            param["suspicious"] = reasons


def get_target_instance():
    selected = ([unreal.load_asset(ASSET_PATH)] if ASSET_PATH
                else unreal.EditorUtilityLibrary.get_selected_assets())
    if len(selected) > 1:
        unreal.log_warning("[MI summary] Multiple assets selected (%d); only a single "
                           "Material Instance is supported. Select one and rerun." % len(selected))
        return None
    if not selected or not isinstance(selected[0], unreal.MaterialInstanceConstant):
        raise RuntimeError("Select a MaterialInstanceConstant in Content Browser, or set ASSET_PATH.")
    return selected[0]


def main():
    instance = get_target_instance()
    if instance is None:
        return None
    chain = parent_chain(instance)
    params = collect_parameters(chain)
    add_base_defaults(params, chain[-1])
    flag_suspicious(params, instance)
    for param in params.values():
        param["raw"] = param.pop("raw")  # keep raw last in the output
    report = {"asset": instance.get_path_name(),
              "parameters": sorted(params.values(), key=lambda p: (p["name"].casefold(), p["kind"]))}

    folder = os.path.abspath(os.path.join(unreal.Paths.project_saved_dir(), "MaterialDiagnostics"))
    os.makedirs(folder, exist_ok=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%d_%H%M%S_%f")
    filename = os.path.join(folder, "material_summary_" + stamp + ".json")
    text = json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False)
    with open(filename, "w", encoding="utf-8") as handle:
        handle.write(text)
    lines = text.splitlines()
    if MAX_LOG_LINES > 0:
        shown = lines[:MAX_LOG_LINES]
        if len(lines) > len(shown):
            shown.append("... (%d more lines)" % (len(lines) - len(shown)))
        unreal.log("[MI summary] Report preview:\n" + "\n".join(shown))
    suspicious = sum("suspicious" in p for p in report["parameters"])
    unreal.log("[MI summary] %d parameter(s), %d suspicious -> %s"
               % (len(report["parameters"]), suspicious, filename))
    if OPEN_REPORT:
        os.startfile(filename)  # Windows only
    return filename


if __name__ == "__main__":
    main()
