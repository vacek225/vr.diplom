"""Create persistent OpenXR actions before the XR session builds its action sets."""
import unreal

folder = '/Game/KUB/Input'
unreal.EditorAssetLibrary.make_directory(folder)
tools = unreal.AssetToolsHelpers.get_asset_tools()
def asset(name, cls):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    return unreal.load_asset(folder + '/' + name) or tools.create_asset(name, folder, cls, factory)

context = asset('IMC_KemVRUI', unreal.InputMappingContext)
context.set_editor_property('context_description', 'KUB VR menu and UI triggers')
context.unmap_all()
grip_context = asset('IMC_KemVRGrip', unreal.InputMappingContext)
grip_context.set_editor_property('context_description', 'KUB continuous object grip')
grip_context.unmap_all()
for name, keys, consume in [
    ('IA_KemMenu', ['Vive_Left_Menu_Click', 'Vive_Right_Menu_Click'], True),
    ('IA_KemUILeft', ['Vive_Left_Trigger_Axis', 'Vive_Left_Trigger_Click',
                      'OculusTouch_Left_Trigger_Axis', 'ValveIndex_Left_Trigger_Axis',
                      'MixedReality_Left_Trigger_Axis'], False),
    ('IA_KemUIRight', ['Vive_Right_Trigger_Axis', 'Vive_Right_Trigger_Click',
                       'OculusTouch_Right_Trigger_Axis', 'ValveIndex_Right_Trigger_Axis',
                       'MixedReality_Right_Trigger_Axis'], False),
    ('IA_KemGripLeft', ['Vive_Left_Grip_Click', 'OculusTouch_Left_Grip_Axis',
                       'ValveIndex_Left_Grip_Axis', 'MixedReality_Left_Grip_Click'], False),
    ('IA_KemGripRight', ['Vive_Right_Grip_Click', 'OculusTouch_Right_Grip_Axis',
                        'ValveIndex_Right_Grip_Axis', 'MixedReality_Right_Grip_Click'], False)]:
    action = asset(name, unreal.InputAction)
    action.set_editor_property('action_description', name)
    action.set_editor_property('value_type', unreal.InputActionValueType.AXIS1D)
    action.set_editor_property('consume_input', consume)
    for key in keys:
        input_key = unreal.Key()
        input_key.import_text(key)
        (grip_context if name.startswith('IA_KemGrip') else context).map_key(action, input_key)
    unreal.EditorAssetLibrary.save_loaded_asset(action, False)
unreal.EditorAssetLibrary.save_loaded_asset(context, False)
unreal.EditorAssetLibrary.save_loaded_asset(grip_context, False)
unreal.log('KUB VR UI: persistent Menu + left/right trigger actions saved')

