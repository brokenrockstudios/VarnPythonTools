"""Clear misconfigured scalar atlas-position flags on selected Material Instances.

By default only scalars that look misconfigured are fixed: the atlas flag is
True but no Curve or Atlas is assigned, so the flag can do nothing useful.
Parent Material Instances in each selection's parent chain are checked too,
since overrides are often stored there. Pass -noparents to check only the
selected assets.

Flags (as script args, e.g. `py fix_material_instance_atlas_flags.py -force`):
  -force      clear the flag on every scalar override, even with a Curve/Atlas
  -dry        only log what would change
  -noparents  do not walk parent instances
DEBUG (constant below) logs one line per scalar override with the decision.
Changed assets are modified but not saved.
"""

import sys

import unreal

ARGS = [a.lower() for a in sys.argv[1:]]
FORCE = "-force" in ARGS
DRY_RUN = "-dry" in ARGS
WALK_PARENTS = "-noparents" not in ARGS
DEBUG = False
FLAG_TRUE = "bIsUsedAsAtlasPosition=True"
FLAG_FALSE = "bIsUsedAsAtlasPosition=False"


def log(message):
    unreal.log("[Atlas flag fix] " + message)


def curve_and_atlas(param, text):
    """Return (curve, atlas) as display strings, from the property or struct text."""
    try:
        atlas_data = param.get_editor_property("atlas_data")
        return (str(atlas_data.get_editor_property("curve")),
                str(atlas_data.get_editor_property("atlas")))
    except Exception:  # atlas_data isn't exposed to Python on some versions; parse the struct text instead
        return ("None" if "Curve=None" in text else "set?",
                "None" if "Atlas=None" in text else "set?")


def decide(param, text):
    """Return (should_fix, reason, curve, atlas)."""
    curve, atlas = curve_and_atlas(param, text)
    if FLAG_TRUE not in text:
        return False, "flag off", curve, atlas
    if FORCE:
        return True, "flag on, -force", curve, atlas
    if curve == "None" and atlas == "None":
        return True, "flag on, no curve/atlas", curve, atlas
    return False, "flag on, curve/atlas assigned", curve, atlas


def fix_instance(mi):
    params = list(mi.get_editor_property("scalar_parameter_values"))
    fixed = 0
    for p in params:
        text = p.export_text()
        name = str(p.parameter_info.name)
        value = p.parameter_value
        do_fix, reason, curve, atlas = decide(p, text)
        if DEBUG:
            log("%s: value=%s curve=%s atlas=%s -> %s (%s)" % (
                name, value, curve, atlas, "FIX" if do_fix else "skip", reason))
        if not do_fix:
            continue
        # The flag isn't settable directly, so round-trip the struct through its text form.
        assert p.import_text(text.replace(FLAG_TRUE, FLAG_FALSE)), \
            "Could not update parameter " + name
        p.parameter_value = value  # import_text can reset the value, so restore it
        fixed += 1

    if fixed and not DRY_RUN:  # write back once per instance, undoable as a single step
        with unreal.ScopedEditorTransaction("Fix scalar atlas flags"):
            mi.modify()
            mi.set_editor_property("scalar_parameter_values", params)
    return fixed


def instance_chain(mi):
    """The instance followed by its parent MaterialInstances (stops at the Material)."""
    chain, seen = [], set()
    while isinstance(mi, unreal.MaterialInstance) and mi.get_path_name() not in seen:
        seen.add(mi.get_path_name())
        chain.append(mi)
        mi = mi.get_editor_property("parent")
    return chain


def main():
    selected = unreal.EditorUtilityLibrary.get_selected_assets()
    roots = [a for a in selected if isinstance(a, unreal.MaterialInstanceConstant)]
    assert roots, "Select one or more MaterialInstanceConstants in Content Browser."

    targets = {}  # path -> instance, preserving order and de-duplicating shared parents
    for root in roots:
        for mi in (instance_chain(root) if WALK_PARENTS else [root]):
            targets.setdefault(mi.get_path_name(), mi)

    total = 0
    for path, mi in targets.items():
        if not isinstance(mi, unreal.MaterialInstanceConstant):
            continue
        if DEBUG:
            log("== %s" % path)
        total += fix_instance(mi)
    log("%d parameter(s) %s across %d instance(s)%s." % (
        total, "would be fixed" if DRY_RUN else "fixed", len(targets),
        " (force)" if FORCE else ""))


if __name__ == "__main__":
    main()
