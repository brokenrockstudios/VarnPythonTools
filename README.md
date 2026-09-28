# Varn Python Tools

A small, source-only Unreal Engine editor plugin targeting UE 5.8. Adds
**Window > Varn Python Browser** with a searchable script list and one Play / Run
button per script. Includes Refresh, personal folder settings, source labels,
full-path tooltips, and a last-run status.

## Install

1. Add `VarnPythonTools` to `<YourProject>/Plugins/`.
2. Build your project.
3. Enable **Varn Python Tools** in Edit > Plugins if necessary, then restart.
4. Open **Window > Varn Python Browser** and run `hello_world.py`.
5. Open **Window > Developer Tools > Output Log**, or your existing Output Log
   tab, to see `Hello world from Varn Python Tools!`.

A C++ toolchain is required; this download does not contain precompiled binaries.

## Search locations

The browser recursively discovers `.py` files in these roots:

| Location                                  | Purpose                                                                   |
| ----------------------------------------- | ------------------------------------------------------------------------- |
| `<Project>/Content/Python`                | Scripts shared with the project                                           |
| `<Project>/Saved/Python`                  | Local, project-specific scratch scripts                                   |
| `<User Documents>/Unreal Projects/Python` | Personal scripts shared across projects on Windows                        |
| `<User Documents>/UnrealEngine/Python`    | Personal scripts shared across projects on Windows                        |
| Additional Script Directories             | Configurable personal folders, including locations outside the repository |
| `<EnabledPlugin>/Content/Python`          | Bundled scripts from this and other enabled plugins                       |

Click **Settings**, or visit **Editor Preferences > Plugins > Varn Python Browser**,
and add entries under **Additional Script Directories**. Absolute paths are
recommended; Click **Refresh** after changing folders or adding/deleting scripts. Reopening the panel also scans.

Folders need not exist; missing folders are skipped and never created by the browser.
Hover over the script count to see all resolved search locations.
Overlapping roots do not duplicate the same normalized absolute filename.
Different files with the same name remain separate; source labels and path
tooltips distinguish them. Symlink aliases are not resolved for deduplication.

`__init__.py` and `init_unreal.py` are omitted from the launch list. The browser scans only the specified locations.

## Shipping your own scripts

Put them beside the example:

    VarnPythonTools/
      Content/
        Python/
          hello_world.py
          Materials/
            fix_material_instance.py

Subfolders are supported. This same convention works in other enabled plugins.
`Config/FilterPlugin.ini` includes the Python directory when packaging with UAT
BuildPlugin. This is an editor-only plugin; scripts are not gameplay code.

## Execution behavior

- Run uses Epic's `IPythonScriptPlugin::ExecPythonCommandEx`, an absolute quoted
  filename, `ExecuteFile`, and a private file execution scope. It rereads the file
  on each run, so edits to the entry script take effect without restarting.
- Paths must end in lowercase `.py` and contain no earlier `.py`, double quotes, or line breaks.
  Unsupported paths are rejected with an explanation because UE 5.8
  can otherwise interpret the quoted filename as code and report false success.
- Private scope separates entry-script globals; imported modules still use
  Python's normal module cache. This is not a separate interpreter or sandbox.
- The two user folders and Additional Script Directories affect discovery only.
  The browser does not modify the interpreter's global import paths.
  For a larger tool package with shared imports, use the standard `Content/Python` layout or configure Python's Additional Paths.
- Output and exceptions go to Unreal's Output Log. The panel reports completion or failure.
  There is no automatic save or undo wrapper: each script owns its changes, transactions, and saving behavior.
- Execution is synchronous on the editor thread. Long-running scripts should use
  `unreal.ScopedSlowTask` where appropriate. Run is disabled while another browser
  execution is in progress, during PIE/simulation, or before Python is initialized.
- The browser respects Python availability and does not force-enable the interpreter.

## Included Scripts

- `hello_world.py`
- TODO

References:

- https://dev.epicgames.com/documentation/unreal-engine/scripting-the-unreal-editor-using-python
- https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/PythonScriptPlugin/IPythonScriptPlugin
- https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/PythonScriptPlugin/FPythonCommandEx
