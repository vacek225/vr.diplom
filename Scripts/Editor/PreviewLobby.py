"""Capture the saved Lobby in a separate editor process without saving its map."""
import os
import time
import unreal

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/VRTemplate/Lobby')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(100, 0, 175))
camera.camera_component.set_editor_property('field_of_view', 90)
out = os.path.join(unreal.Paths.project_saved_dir(), 'Screenshots', 'Lobby')
os.makedirs(out, exist_ok=True)
suffix = os.environ.get('KUB_LOBBY_CAPTURE', 'before')
state = {'index': 0, 'last': time.monotonic(), 'task': None, 'busy': False}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def tick(dt):
    if state['busy']:
        return
    if time.monotonic() - state['last'] < 12:
        return
    if state['task'] and not state['task'].is_task_done():
        return
    if state['index'] == 4:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
        return
    yaw = state['index'] * 90
    camera.set_actor_rotation(unreal.Rotator(pitch=0, yaw=yaw, roll=0), False)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera.get_actor_location(), camera.get_actor_rotation())
    state['busy'] = True
    state['index'] += 1
    state['last'] = time.monotonic()
    state['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1280, 720, os.path.join(out, f'{suffix}_{yaw}.png'), camera=camera, delay=1)
    state['busy'] = False
handle = unreal.register_slate_post_tick_callback(tick)
