"""Author cookable PBR surfaces for the procedural VAZ-2112.

Run in a separate UE process with -run=pythonscript -script=<this file>.
Re-running updates only the solid material/stand instance. Training ghosts are
preserved. Object-local grain follows grabbed/rotated parts; no external images
or per-frame texture updates are required.
"""
import unreal

FOLDER = '/Game/KUB/Engine2112'
EDIT = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EXISTING = []


def asset(name, kind, factory):
    return unreal.load_asset(FOLDER + '/' + name) if ASSETS.does_asset_exist(FOLDER + '/' + name) else TOOLS.create_asset(name, FOLDER, kind, factory)


def node(material, kind, x, y, **properties):
    tag = f'KUB_PBR_{x}_{y}'
    result = next((item for item in EXISTING if isinstance(item, kind) and
                   (item.get_editor_property('desc') == tag or
                    ('parameter_name' in properties and str(item.get_editor_property('parameter_name')) == properties['parameter_name']))), None)
    if result is None:
        result = EDIT.create_material_expression(material, kind, x, y)
    result.set_editor_property('desc', tag)
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def wire(source, target, pin, output=''):
    if not EDIT.connect_material_expressions(source, output, target, pin):
        if pin == 'Input' and EDIT.connect_material_expressions(source, output, target, ''):
            return
        raise RuntimeError('Cannot connect material pin ' + pin)


def scalar(material, name, value, x, y):
    return node(material, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def custom(material, description, code, inputs, output_type, x, y):
    expression = node(material, unreal.MaterialExpressionCustom, x, y,
                      description=description, code=code, output_type=output_type)
    pins = []
    for name, source in inputs.items():
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        pins.append(entry)
    expression.set_editor_property('inputs', pins)
    for name, source in inputs.items():
        wire(source, expression, name)
    return expression


material = asset('M_EngineTraining', unreal.Material, unreal.MaterialFactoryNew())
# Constructor-loaded materials can root their existing expressions in UE 5.7.
# Reuse our connected graph instead of deleting rooted objects or replacing assets.
def collect(expression):
    if expression is None or expression in EXISTING:
        return
    EXISTING.append(expression)
    for source in EDIT.get_inputs_for_material_expression(material, expression):
        collect(source)
for prop in [unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialProperty.MP_METALLIC,
             unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialProperty.MP_NORMAL]:
    collect(EDIT.get_material_property_input_node(material, prop))
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
material.set_editor_property('tangent_space_normal', False)
material.set_editor_property('two_sided', False)
tint = node(material, unreal.MaterialExpressionVectorParameter, -1100, -450,
            parameter_name='Tint', default_value=unreal.LinearColor(.58, .60, .62, 1))
metal = scalar(material, 'Metallic', .95, -250, 500)
rough = scalar(material, 'Roughness', .48, -1000, 350)
variation = scalar(material, 'Variation', .08, -1000, 500)
bump = scalar(material, 'BumpStrength', .5, -1000, 650)
brushed = scalar(material, 'Brushed', 0, -1400, 220)
local = node(material, unreal.MaterialExpressionPreSkinnedPosition, -1800, -100)
scale = node(material, unreal.MaterialExpressionMaterialFunctionCall, -1800, 80)
scale.set_material_function(unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/WorldPositionOffset/ObjectScale'))
scaled = node(material, unreal.MaterialExpressionMultiply, -1600, -100)
wire(local, scaled, 'A'); wire(scale, scaled, 'B')
interpolated = node(material, unreal.MaterialExpressionVertexInterpolator, -1400, -100)
wire(scaled, interpolated, 'Input')
# One smooth value-noise octave for casting mottling; analytic fine grain and
# machining lines fade with pixel footprint to limit distant VR shimmer.
pattern = custom(material, 'Casting grain and machining', r'''
float3 q=P*4.0;
float3 cell=floor(q), f=frac(q); f=f*f*(3.0-2.0*f);
float n=0;
[unroll] for(int i=0;i<8;++i) {
    float3 c=float3(i&1,(i>>1)&1,(i>>2)&1);
    float3 w=lerp(1-f,f,c);
    float h=frac(sin(dot(cell+c,float3(127.1,311.7,74.7)))*43758.5453);
    n+=h*w.x*w.y*w.z;
}
float footprint=max(length(ddx(P)),length(ddy(P)));
float fade=1-saturate(footprint*12);
float fine=(sin(dot(P,float3(43.1,61.7,37.3)))+sin(dot(P,float3(79.4,-53.2,91.8))))*.25;
float lines=sin(P.z*115+sin(P.x*8)*.3);
float lineFade=1-saturate(footprint*40);
float detail=lerp(fine*fade,lines*.5*lineFade,Brushed);
float shade=.98+n*.04+detail*.008;
float height=lerp((n-.5)*.0015+detail*.001,detail*.0005,Brushed);
return float3(shade,height,saturate(.5+(n-.5)*.75+detail*.2));
''', {'P':interpolated, 'Brushed':brushed}, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -1100, -150)
shade = node(material, unreal.MaterialExpressionComponentMask, -750, -350, r=True, g=False, b=False, a=False)
height = node(material, unreal.MaterialExpressionComponentMask, -750, -100, r=False, g=True, b=False, a=False)
rough_detail = node(material, unreal.MaterialExpressionComponentMask, -750, 130, r=False, g=False, b=True, a=False)
for target in [shade,height,rough_detail]: wire(pattern,target,'Input')
base = node(material, unreal.MaterialExpressionMultiply, -450, -400)
wire(tint,base,'A'); wire(shade,base,'B')
EDIT.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
roughness = custom(material, 'Micro roughness', 'return clamp(R+(D-.5)*V,.08,.98);',
                   {'R':rough,'D':rough_detail,'V':variation}, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -450, 150)
EDIT.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
EDIT.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
world = node(material, unreal.MaterialExpressionWorldPosition, -750, 750)
normal = node(material, unreal.MaterialExpressionVertexNormalWS, -750, 900)
normal_map = custom(material, 'World-space surface microrelief', r'''
float3 Nn=normalize(N);
float3 dx=ddx(W), dy=ddy(W);
float3 rx=cross(dy,Nn), ry=cross(Nn,dx);
float det=dot(dx,rx);
float3 gradient=sign(det)*(ddx(H)*rx+ddy(H)*ry)*Strength;
float3 slope=gradient/max(abs(det),1e-8);
slope*=min(1.0,.06/max(length(slope),1e-6));
return normalize(Nn-slope);
''', {'W':world,'N':normal,'H':height,'Strength':bump}, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -250, 750)
EDIT.connect_material_property(normal_map, '', unreal.MaterialProperty.MP_NORMAL)
EDIT.layout_material_expressions(material)
EDIT.recompile_material(material)
ASSETS.save_loaded_asset(material)

stand = asset('MI_EngineStand', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
EDIT.set_material_instance_parent(stand, material)
EDIT.set_material_instance_vector_parameter_value(stand, 'Tint', unreal.LinearColor(.025,.04,.055,1))
for name,value in {'Metallic':.05,'Roughness':.52,'Variation':.05,'BumpStrength':.18,'Brushed':0}.items():
    EDIT.set_material_instance_scalar_parameter_value(stand,name,value)
EDIT.update_material_instance(stand)
ASSETS.save_loaded_asset(stand)

if not ASSETS.does_asset_exist(FOLDER + '/M_EngineTarget'):
    ghost=asset('M_EngineTarget',unreal.Material,unreal.MaterialFactoryNew())
    ghost.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    ghost.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    ghost.set_editor_property('two_sided',True)
    ghost.set_editor_property('wireframe',True)
    color=node(ghost,unreal.MaterialExpressionVectorParameter,-400,0,parameter_name='Tint',default_value=unreal.LinearColor(.1,.8,1,1))
    opacity=node(ghost,unreal.MaterialExpressionConstant,-400,140,r=.65)
    EDIT.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    EDIT.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    EDIT.recompile_material(ghost); ASSETS.save_loaded_asset(ghost)
# A separate inexpensive local-view reveal surface; never change the shared
# opaque PBR parent or the installation wireframe.
if not ASSETS.does_asset_exist(FOLDER + '/M_EngineAssemblyXRay'):
    reveal=asset('M_EngineAssemblyXRay',unreal.Material,unreal.MaterialFactoryNew())
    reveal.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    reveal.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    reveal.set_editor_property('two_sided',True)
    color=EDIT.create_material_expression(reveal,unreal.MaterialExpressionVectorParameter,-400,0)
    color.set_editor_property('parameter_name','Tint')
    color.set_editor_property('default_value',unreal.LinearColor(.12,.32,.40,1))
    opacity=EDIT.create_material_expression(reveal,unreal.MaterialExpressionScalarParameter,-400,140)
    opacity.set_editor_property('parameter_name','RevealOpacity')
    opacity.set_editor_property('default_value',.14)
    EDIT.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    EDIT.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    EDIT.recompile_material(reveal); ASSETS.save_loaded_asset(reveal)
unreal.log('Engine2112 PBR materials updated successfully')

