import json
from pathlib import Path
import unreal

lib = unreal.MaterialEditingLibrary
result = {}
for name in ('WALL2_MAT', 'wall3_mat', 'BASE_MAT', 'Floor_LP_mat', 'Roof_mat'):
    mat = unreal.load_asset('/Game/KUB/lobby/' + name)
    entry = {'inputs': {}, 'textures': []}
    for prop in (unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialProperty.MP_METALLIC,
                 unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialProperty.MP_NORMAL,
                 unreal.MaterialProperty.MP_AMBIENT_OCCLUSION):
        entry['inputs'][str(prop)] = str(lib.get_material_property_input_node(mat, prop)) + ':' + str(lib.get_material_property_input_node_output_name(mat, prop))
    pending = [lib.get_material_property_input_node(mat, p) for p in
               (unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialProperty.MP_NORMAL,
                unreal.MaterialProperty.MP_METALLIC)]
    seen = set()
    textures = []
    while pending:
        node = pending.pop()
        if not node or node.get_path_name() in seen:
            continue
        seen.add(node.get_path_name())
        if isinstance(node, unreal.MaterialExpressionTextureSample):
            textures.append(node.get_editor_property('texture'))
        pending.extend(lib.get_inputs_for_material_expression(mat, node))
    for tex in textures:
        entry['textures'].append({'path': tex.get_path_name(), 'width': tex.blueprint_get_size_x(),
                                  'height': tex.blueprint_get_size_y(), 'srgb': tex.get_editor_property('srgb'),
                                  'compression': str(tex.get_editor_property('compression_settings')),
                                  'max_texture_size': tex.get_editor_property('max_texture_size'),
                                  'never_stream': tex.get_editor_property('never_stream')})
    result[name] = entry
for name in ('2', '3', 'BASE'):
    mesh = unreal.load_asset('/Game/KUB/lobby/' + name)
    result[name] = {'materials': str(mesh.get_editor_property('static_materials')),
                    'light_map_coordinate_index': mesh.get_editor_property('light_map_coordinate_index'),
                    'light_map_resolution': mesh.get_editor_property('light_map_resolution')}
out = Path(unreal.Paths.project_saved_dir()) / 'Diagnostics' / 'LobbySurfaces.json'
out.write_text(json.dumps(result, indent=2), encoding='utf-8')

