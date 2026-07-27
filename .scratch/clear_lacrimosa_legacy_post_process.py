import unreal

asset_path = "/Game/Characters/Player/004_lacrimosa/player_004_lacrimosa_skin"
mesh = unreal.load_asset(asset_path)
if not mesh:
    raise RuntimeError("Could not load " + asset_path)

old_value = mesh.get_editor_property("post_process_anim_blueprint")
unreal.log("Clearing legacy post-process AnimBP: {}".format(old_value))
mesh.set_editor_property("post_process_anim_blueprint", None)
if not unreal.EditorAssetLibrary.save_loaded_asset(mesh):
    raise RuntimeError("Could not save " + asset_path)

new_value = mesh.get_editor_property("post_process_anim_blueprint")
if new_value:
    raise RuntimeError("Post-process AnimBP remained assigned: {}".format(new_value))

unreal.log("Cleared legacy post-process AnimBP from " + asset_path)
