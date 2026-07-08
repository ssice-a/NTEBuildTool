import json

import unreal


OUT = r"F:\NTE\NTEBuildTool\.scratch\lacrimosa_wbp_generated_inspection.json"
ASSET = "/Game/Characters/Player/004_lacrimosa/mod/Runtime/WBP_NTE_ModToggleMenu"


def get_name(obj):
    return obj.get_name() if obj else ""


def get_class_name(obj):
    return obj.get_class().get_name() if obj else ""


def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception as exc:
        return {"error": str(exc)}


def describe_widget(widget):
    entry = {
        "name": get_name(widget),
        "class": get_class_name(widget),
        "path": widget.get_path_name() if widget else "",
    }
    for key in ["visibility", "is_enabled", "render_transform", "render_translation", "slot"]:
        value = prop(widget, key)
        if not isinstance(value, dict):
            entry[key] = str(value)
    if get_class_name(widget) == "TextBlock":
        try:
            entry["text"] = str(widget.get_text())
        except Exception as exc:
            entry["text_error"] = str(exc)
    if get_class_name(widget) == "Button":
        for key in ["content", "background_color", "color_and_opacity", "style"]:
            value = prop(widget, key)
            if not isinstance(value, dict):
                entry[key] = get_name(value) if hasattr(value, "get_name") else str(value)
    return entry


asset = unreal.EditorAssetLibrary.load_asset(ASSET)
result = {
    "asset": ASSET,
    "asset_loads": asset is not None,
    "asset_class": get_class_name(asset),
}

if asset:
    generated_class = prop(asset, "generated_class")
    result["generated_class"] = str(generated_class)
    result["generated_class_class"] = get_class_name(generated_class) if not isinstance(generated_class, dict) else ""
    result["asset_widget_tree"] = str(prop(asset, "widget_tree"))
    tree = None
    if generated_class and not isinstance(generated_class, dict):
        tree = prop(generated_class, "widget_tree")
        result["generated_widget_tree"] = str(tree)
    if tree and not isinstance(tree, dict):
        widgets = []
        try:
            tree.get_all_widgets(widgets)
        except TypeError:
            try:
                widgets = list(tree.get_all_widgets())
            except Exception as exc:
                result["get_all_widgets_error"] = str(exc)
        except Exception as exc:
            result["get_all_widgets_error"] = str(exc)
        result["widgets"] = [describe_widget(w) for w in widgets]

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)

print(OUT)
