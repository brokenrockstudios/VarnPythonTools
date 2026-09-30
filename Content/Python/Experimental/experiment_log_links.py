"""Experiment: which log-line formats become clickable in Unreal's Output Log?

Run in the editor, open Window > Output Log, and try clicking each "[link test N]"
line (also hover for a hand cursor / underline). Report which N worked.
Also check the Python console / terminal you launched the editor from, if any.
Writes a tiny test file; nothing else is touched.
"""

import os

import unreal

folder = os.path.abspath(os.path.join(unreal.Paths.project_saved_dir(), "Temp"))
os.makedirs(folder, exist_ok=True)
path = os.path.join(folder, "link_test.json")
with open(path, "w", encoding="utf-8") as handle:
    handle.write('{"hello": "link test"}\n')

fwd = path.replace("\\", "/")
variants = [
    ("plain backslash path", path),
    ("plain forward-slash path", fwd),
    ("quoted path", '"%s"' % path),
    ("file:/// url", "file:///" + fwd),
    ("file:// url", "file://" + fwd),
    ("MSVC style path(1)", "%s(1)" % path),
    ("colon line style path:1", "%s:1" % path),
    ("colon line+col style path:1:1", "%s:1:1" % path),
    ("markdown link", "[open report](file:///%s)" % fwd),
    ("html anchor", '<a href="file:///%s">open report</a>' % fwd),
    ("slate rich text anchor", '<a id="browser" href="file:///%s">open report</a>' % fwd),
    ("http url (control)", "https://www.unrealengine.com"),
    ("asset path (control)", "/Game/Materials/Example.Example"),
]
for number, (label, text) in enumerate(variants, 1):
    unreal.log("[link test %d] %s: %s" % (number, label, text))

# Same format as variant 1 but at different severities; some views style these differently.
unreal.log_warning("[link test W] warning severity: " + path)
unreal.log_error("[link test E] error severity: " + path)
