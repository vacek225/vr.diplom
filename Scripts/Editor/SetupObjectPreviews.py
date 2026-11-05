"""Separate editor processes only; never saves a map.
First run PreviewEngine2112.py, then this script with -KemPreviewCube,
then this script without that flag for the sphere and texture import.
"""
import os, time, shutil
import unreal
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
out=os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'UI', 'ObjectPreviews')
os.makedirs(out, exist_ok=True)
source=os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Screenshots', 'Engine2112', 'assembled.png')
if not os.path.isfile(source):
    raise RuntimeError('Generate the real engine screenshot with PreviewEngine2112.py first')
shutil.copyfile(source, os.path.join(out,'engine.png'))
target='cube' if 'KemPreviewCube' in unreal.SystemLibrary.get_command_line() else 'sphere'
target_path=os.path.join(out,target+'.png')
if os.path.isfile(target_path): os.remove(target_path)
model=actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0,0,65))
model.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/'+('Cube' if target=='cube' else 'Sphere')))
for rotation, intensity in [((-35,-40,0),5.0),((-45,140,0),3.0)]:
    light=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,300),unreal.Rotator(*rotation))
    light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity',intensity)
sky=actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,300)).get_component_by_class(unreal.SkyLightComponent)
sky.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
sky.set_editor_property('intensity',1.5)
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(-190,-240,200))
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(0,0,65)),False)
camera.camera_component.set_editor_property('field_of_view',40.0)
camera.camera_component.set_editor_property('aspect_ratio',4.0/3.0)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera.get_actor_location(),camera.get_actor_rotation())
state={'step':0,'next':time.monotonic()+15,'task':None}
started=time.monotonic()
def tick(dt):
    now=time.monotonic()
    if now-started>200:
        unreal.log_error('Thumbnail generation timed out')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
        return
    if now<state['next'] or (state['task'] and not state['task'].is_task_done()): return
    step=state['step']
    if step==0:
        state['task']=unreal.AutomationLibrary.take_high_res_screenshot(640,480,os.path.join(out,target+'.png'),camera=camera,delay=1.0)
        state['next']=now+5
    else:
        if not os.path.isfile(os.path.join(out,target+'.png')): return
        unreal.unregister_slate_post_tick_callback(handle)
        if not all(os.path.isfile(os.path.join(out,n+'.png')) for n in ['cube','sphere']):
            unreal.log('Thumbnail saved; run the script for the other primitive next')
            unreal.SystemLibrary.quit_editor()
            return
        tasks=[]
        for name in ['engine','cube','sphere']:
            task=unreal.AssetImportTask()
            task.set_editor_property('filename',os.path.join(out,name+'.png'))
            task.set_editor_property('destination_path','/Game/KUB/UI/ObjectPreviews')
            task.set_editor_property('destination_name','T_'+name)
            task.set_editor_property('automated',True)
            task.set_editor_property('replace_existing',True)
            task.set_editor_property('save',True)
            tasks.append(task)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        for name in ['engine','cube','sphere']:
            path='/Game/KUB/UI/ObjectPreviews/T_'+name
            texture=unreal.load_asset(path)
            if not texture: raise RuntimeError('Failed to import '+path)
            texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
            texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            unreal.EditorAssetLibrary.save_loaded_asset(texture)
        unreal.log('Catalog previews imported successfully')
        unreal.SystemLibrary.quit_editor()
        return
    state['step']+=1
handle=unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
