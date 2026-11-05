"""Standalone preview process only: creates an unsaved blank world, captures
assembled/disassembled views in Saved/Screenshots/Engine2112, then exits.
Do not run in an editor session with unsaved work.
"""
import os
import time
import unreal

unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
engine = actors.spawn_actor_from_class(unreal.KemEngineAssembly, unreal.Vector(0, 0, 80))
engine.set_actor_scale3d(unreal.Vector(1.35, 1.35, 1.35))
engine.ensure_parts()
metals = nonmetals = 0
for part in actors.get_all_level_actors():
    if not isinstance(part, unreal.KemEnginePart):
        continue
    components = part.get_components_by_class(unreal.StaticMeshComponent)
    surface = components[0].get_material(0)
    metallic = surface.get_scalar_parameter_value('Metallic')
    roughness = surface.get_scalar_parameter_value('Roughness')
    if not 0 <= metallic <= 1 or not .08 <= roughness <= .98:
        raise RuntimeError('Invalid engine PBR surface parameters')
    metals += metallic > .8
    nonmetals += metallic < .1
if not metals or not nonmetals:
    raise RuntimeError('Engine must have distinct metal and nonmetal materials')
unreal.log(f'Engine PBR profiles verified: {metals} metallic, {nonmetals} nonmetallic nodes')
for component in engine.get_components_by_class(unreal.WidgetComponent):
    component.set_visibility(False)

floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 60, 0))
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.set_actor_scale3d(unreal.Vector(8, 8, 0.05))
for rotation, intensity in [((-40, -40, 0), 5.0), ((-30, 140, 0), 3.0), ((-65, 40, 0), 1.5)]:
    light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 300), unreal.Rotator(*rotation))
    light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity', intensity)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 300))
sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
sky_component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky_component.set_editor_property('cubemap', unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
sky_component.set_editor_property('intensity', 1.5)

camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(-190, -240, 175))
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(), unreal.Vector(0, 0, 85)), False)
camera.camera_component.set_editor_property('field_of_view', 45.0)
camera.camera_component.set_editor_property('aspect_ratio', 4.0 / 3.0)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera.get_actor_location(), camera.get_actor_rotation())
out_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Screenshots', 'Engine2112')
os.makedirs(out_dir, exist_ok=True)
started = time.monotonic()
state = {'stage': 0, 'task': None, 'next': started + 20}


def tick(delta):
    if state['stage'] == 6:
        return
    now = time.monotonic()
    if now - started > 240:
        unreal.log_error('Engine preview timed out')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
        return
    if now < state['next']:
        return
    if state['task'] and not state['task'].is_task_done():
        return
    if state['stage'] == 0:
        state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1440, 1080, os.path.join(out_dir, 'assembled.png'), camera=camera, delay=1.0)
        state['stage'] = 1
        state['next'] = now + 4
    elif state['stage'] == 1:
        engine.configure_lesson(unreal.KemEngineLesson.ASSEMBLE, True)
        camera.set_actor_location(unreal.Vector(-370, -380, 530), False, False)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(), unreal.Vector(0, 135, 70)), False)
        camera.camera_component.set_editor_property('field_of_view', 55.0)
        state['next'] = now + 3
        state['task'] = None
        state['stage'] = 2
    elif state['stage'] == 2:
        state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1440, 1080, os.path.join(out_dir, 'disassembled.png'), camera=camera, delay=1.0)
        state['stage'] = 3
        state['next'] = now + 4
    elif state['stage'] == 3:
        engine.configure_lesson(unreal.KemEngineLesson.FREE, True)
        camera.set_actor_location(unreal.Vector(-105, -130, 165), False, False)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(), unreal.Vector(0, 0, 115)), False)
        camera.camera_component.set_editor_property('field_of_view', 42.0)
        state['task'] = None
        state['next'] = now + 3
        state['stage'] = 4
    elif state['stage'] == 4:
        state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1440, 1080, os.path.join(out_dir, 'materials_closeup.png'), camera=camera, delay=1.0)
        state['stage'] = 5
        state['next'] = now + 4
    else:
        state['stage'] = 6
        unreal.unregister_slate_post_tick_callback(handle)
        # Keep the menu catalogue consistent with the actual assembled model.
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', os.path.join(out_dir, 'assembled.png'))
        task.set_editor_property('destination_path', '/Game/KUB/UI/ObjectPreviews')
        task.set_editor_property('destination_name', 'T_engine')
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        preview = unreal.load_asset('/Game/KUB/UI/ObjectPreviews/T_engine')
        preview.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        preview.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        preview.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        unreal.EditorAssetLibrary.save_loaded_asset(preview)
        unreal.log('Engine2112 previews saved: ' + out_dir)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

