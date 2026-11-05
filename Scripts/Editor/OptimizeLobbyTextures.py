"""Keep 4K albedo detail; stream normal/mask atlases at up to 2K, with linear ORM."""
from pathlib import Path
import shutil
import unreal

root = Path(unreal.Paths.project_dir())
backup = root / 'Saved' / 'Backups' / 'LobbySurfaceRepair'
def back_up(asset):
    relative = Path('Content') / (asset.get_path_name().split('.')[0].removeprefix('/Game/') + '.uasset')
    target = backup / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists():
        shutil.copy2(root / relative, target)

lib = unreal.MaterialEditingLibrary
changed = set()
for name in ('WALL2_MAT', 'wall3_mat', 'BASE_MAT', 'Floor_LP_mat', 'Roof_mat'):
    mat = unreal.load_asset('/Game/KUB/lobby/' + name)
    back_up(mat)
    for prop, sampler in [(unreal.MaterialProperty.MP_METALLIC, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS),
                          (unreal.MaterialProperty.MP_NORMAL, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)]:
        node = lib.get_material_property_input_node(mat, prop)
        tex = node.get_editor_property('texture')
        back_up(tex)
        tex.set_editor_property('max_texture_size', 2048)
        if prop == unreal.MaterialProperty.MP_METALLIC:
            tex.set_editor_property('srgb', False)
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        node.set_editor_property('sampler_type', sampler)
        unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
        changed.add(tex.get_path_name())
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
unreal.log(f'LOBBY_TEXTURES: optimized {len(changed)} atlases, albedo resolution preserved.')
