"""Repair known Lobby material overrides and ORM wiring. Run in a separate editor.
Backs up every affected package once before writing. Does not alter scene layout.
"""
from pathlib import Path
import shutil
import unreal

root = Path(unreal.Paths.project_dir())
backup = root / 'Saved' / 'Backups' / 'LobbySurfaceRepair'
def back_up(relative):
    source = root / relative
    target = backup / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists():
        shutil.copy2(source, target)

back_up('Content/VRTemplate/Lobby.umap')
back_up('Content/KUB/lobby/Roof_mat.uasset')
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/VRTemplate/Lobby')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
fixed = 0
for actor in actors:
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        if comp.static_mesh and comp.static_mesh.get_path_name() == '/Game/KUB/lobby/2.2':
            current = comp.get_material(0)
            expected = unreal.load_asset('/Game/KUB/lobby/WALL2_MAT')
            if current == expected:
                fixed += 1
                continue
            if not current or current.get_path_name() != '/Game/KUB/lobby/objects/PBR.PBR':
                raise RuntimeError('Unexpected wall override; inspect before replacing: ' + str(current))
            comp.set_material(0, expected)
            fixed += 1
if not fixed:
    raise RuntimeError('Expected wall mesh was not found')
lib = unreal.MaterialEditingLibrary
roof = unreal.load_asset('/Game/KUB/lobby/Roof_mat')
orm = lib.get_material_property_input_node(roof, unreal.MaterialProperty.MP_METALLIC)
assert orm.get_editor_property('texture').get_name().endswith('OcclusionRoughnessMetallic')
for channel, prop in [('R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),
                      ('G', unreal.MaterialProperty.MP_ROUGHNESS),
                      ('B', unreal.MaterialProperty.MP_METALLIC)]:
    assert lib.connect_material_property(orm, channel, prop)
lib.recompile_material(roof)
unreal.EditorAssetLibrary.save_loaded_asset(roof, False)
unreal.EditorLevelLibrary.save_current_level()
unreal.log(f'LOBBY_REPAIR: restored {fixed} wall material overrides and roof ORM channels. Backup: {backup}')
