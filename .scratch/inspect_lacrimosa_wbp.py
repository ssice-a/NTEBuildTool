import json
import sys

import unreal


OUT = r"F:\NTE\NTEBuildTool\.scratch\lacrimosa_wbp_python_inspection.json"
ASSETS = [
    "/Game/Characters/Player/004_lacrimosa/mod/Runtime/WBP_NTE_ModToggleMenu",
    "/Game/Characters/Player/004_lacrimosa/mod/Runtime/ABP_NTE_ModToggle_PostProcess",
    "/Game/Characters/Player/004_lacrimosa/player_004_lacrimosa_skin",
]


def get_name(obj):
    return obj.get_name() if obj else ""


def get_class_name(obj):
    return obj.get_class().get_name() if obj else ""


def try_prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def widget_entry(widget):
    entry = {
        "name": get_name(widget),
        "class": get_class_name(widget),
    }
    if get_class_name(widget) == "TextBlock":
        try:
            entry["text"] = str(widget.get_text())
        except Exception as exc:
            entry["text_error"] = str(exc)
    if get_class_name(widget) == "Button":
        child = try_prop(widget, "content", None)
        entry["content"] = get_name(child)
        entry["content_class"] = get_class_name(child)
    try:
        entry["visibility"] = str(widget.get_visibility())
    except Exception:
        pass
    return entry


result = []
for asset_path in ASSETS:
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    info = {
        "path": asset_path,
        "loads": asset is not None,
        "class": get_class_name(asset),
        "name": get_name(asset),
    }
    if asset and get_class_name(asset) == "WidgetBlueprint":
        tree = try_prop(asset, "widget_tree", None)
        info["has_widget_tree"] = tree is not None
        widgets = []
        if tree:
            try:
                widgets = list(tree.get_all_widgets())
            except Exception as exc:
                info["get_all_widgets_error"] = str(exc)
        info["widgets"] = [widget_entry(widget) for widget in widgets]
    if asset and get_class_name(asset) == "SkeletalMesh":
        pp = try_prop(asset, "post_process_anim_blueprint", None)
        info["post_process_anim_blueprint"] = str(pp.get_path_name()) if pp else ""
    result.append(info)

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)

print(OUT)
