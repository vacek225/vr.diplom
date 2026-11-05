import unreal

OLD_MAP = "/Game/VRTemplate/Lobby"
NEW_MAP = "/Game/VRTemplate/3_Block"

SPAWN_TAG = "Kem_Auditorium_Spawn"


def log(message: str) -> None:
    unreal.log(f"[Setup3Block] {message}")


def make_vector(x: float, y: float, z: float) -> unreal.Vector:
    return unreal.Vector(float(x), float(y), float(z))


def make_rotator(pitch: float, yaw: float, roll: float = 0.0) -> unreal.Rotator:
    return unreal.Rotator(float(pitch), float(yaw), float(roll))


def get_all_actors():
    return unreal.EditorLevelLibrary.get_all_level_actors()


def find_actor_by_label(label: str):
    for actor in get_all_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def set_folder(actor, folder: str) -> None:
    if actor:
        actor.set_folder_path(folder)


def ensure_actor(label: str, actor_class, location, rotation, folder: str = ""):
    actor = find_actor_by_label(label)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, rotation)
        actor.set_actor_label(label)
    else:
        actor.set_actor_location(location, False, False)
        actor.set_actor_rotation(rotation, False)

    if folder:
        set_folder(actor, folder)
    return actor


def ensure_attachment(child, parent) -> None:
    if child is None or parent is None:
        return

    try:
        child.attach_to_actor(
            parent,
            "",
            unreal.AttachmentRule.KEEP_WORLD,
            unreal.AttachmentRule.KEEP_WORLD,
            unreal.AttachmentRule.KEEP_WORLD,
            False,
        )
    except Exception as exc:
        log(f"Attachment skipped for {child.get_actor_label()}: {exc}")


def set_actor_scale(actor, x: float, y: float, z: float) -> None:
    if actor:
        actor.set_actor_scale3d(make_vector(x, y, z))


def configure_directional_light(light_actor) -> None:
    light_component = light_actor.get_component_by_class(unreal.DirectionalLightComponent)
    if light_component is None:
        return

    light_component.set_editor_property("intensity", 6.5)
    light_component.set_editor_property("light_color", unreal.Color(255, 244, 230, 255))
    light_component.set_editor_property("cast_shadows", True)
    light_component.set_editor_property("dynamic_shadow_distance_movable_light", 25000.0)
    light_component.set_editor_property("atmosphere_sun_light", True)


def configure_sky_light(light_actor) -> None:
    light_component = light_actor.get_component_by_class(unreal.SkyLightComponent)
    if light_component is None:
        return

    light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    light_component.set_editor_property("intensity", 1.7)
    light_component.set_editor_property("real_time_capture", True)


def configure_rect_light(light_actor, intensity: float) -> None:
    light_component = light_actor.get_component_by_class(unreal.RectLightComponent)
    if light_component is None:
        return

    light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    light_component.set_editor_property("intensity", intensity)
    light_component.set_editor_property("light_color", unreal.Color(255, 237, 219, 255))
    light_component.set_editor_property("source_width", 240.0)
    light_component.set_editor_property("source_height", 120.0)
    light_component.set_editor_property("barn_door_angle", 75.0)
    light_component.set_editor_property("barn_door_length", 20.0)
    light_component.set_editor_property("cast_shadows", True)


def configure_fog(fog_actor) -> None:
    fog_component = fog_actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
    if fog_component is None:
        return

    fog_component.set_editor_property("fog_density", 0.006)
    fog_component.set_editor_property("fog_height_falloff", 0.22)
    fog_component.set_editor_property("directional_inscattering_exponent", 10.0)
    fog_component.set_editor_property("directional_inscattering_start_distance", 2500.0)


def update_sky_sphere(direction_light_actor) -> None:
    sky_sphere = find_actor_by_label("SkySphere")
    if sky_sphere is None:
        return

    try:
        sky_sphere.set_editor_property("Directional light actor", direction_light_actor)
    except Exception:
        try:
            sky_sphere.set_editor_property("directional_light_actor", direction_light_actor)
        except Exception as exc:
            log(f"Sky sphere directional light binding skipped: {exc}")
            return

    try:
        unreal.EditorLevelLibrary.set_selected_level_actors([sky_sphere])
        unreal.EditorLevelLibrary.editor_invalidate_viewports()
        sky_sphere.call_method("RefreshMaterial")
    except Exception:
        pass


def rename_map_if_needed() -> None:
    if unreal.EditorAssetLibrary.does_asset_exist(NEW_MAP):
        log("Map 3_Block already exists.")
        return

    if not unreal.EditorAssetLibrary.does_asset_exist(OLD_MAP):
        raise RuntimeError("Source map /Game/VRTemplate/Lobby was not found.")

    if unreal.EditorAssetLibrary.duplicate_asset(OLD_MAP, NEW_MAP):
        log("Duplicated map Lobby -> 3_Block.")
        return

    if unreal.EditorAssetLibrary.rename_asset(OLD_MAP, NEW_MAP):
        log("Renamed map Lobby -> 3_Block.")
        return

    raise RuntimeError("Failed to create 3_Block from Auditorium.")


def main() -> None:
    rename_map_if_needed()

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level_subsystem.load_level(NEW_MAP)

    folder_root = "Kem/3_Block"
    folder_stage = f"{folder_root}/Stage"
    folder_lighting = f"{folder_root}/Lighting"
    folder_spawn = f"{folder_root}/Spawn"
    folder_system = f"{folder_root}/System"

    bootstrap_class = unreal.load_class(None, "/Script/KemSUinVR.KemClassroomBootstrapActor")
    shared_screen_class = unreal.load_class(None, "/Script/KemSUinVR.KemSharedLectureScreenActor")
    browser_desk_class = unreal.load_class(None, "/Script/KemSUinVR.KemBrowserDeskActor")
    teacher_panel_class = unreal.load_class(None, "/Script/KemSUinVR.KemTeacherControlPanelActor")
    room_code_class = unreal.load_class(None, "/Script/KemSUinVR.KemRoomCodeDisplayActor")
    hub_class = unreal.load_class(None, "/Script/KemSUinVR.KemHubActor")

    stage_root = ensure_actor(
        "Kem_3Block_StageRoot",
        unreal.Actor,
        make_vector(990.0, 2060.0, 0.0),
        make_rotator(0.0, 180.0, 0.0),
        folder_stage,
    )

    lighting_root = ensure_actor(
        "Kem_3Block_LightingRoot",
        unreal.Actor,
        make_vector(500.0, 2060.0, 0.0),
        make_rotator(0.0, 0.0, 0.0),
        folder_lighting,
    )

    spawn_root = ensure_actor(
        "Kem_3Block_SpawnRoot",
        unreal.Actor,
        make_vector(320.0, 2062.0, 0.0),
        make_rotator(0.0, 0.0, 0.0),
        folder_spawn,
    )

    bootstrap_actor = ensure_actor(
        "Kem_3Block_Bootstrap",
        bootstrap_class,
        make_vector(960.0, 2060.0, 0.0),
        make_rotator(0.0, 180.0, 0.0),
        folder_system,
    )

    teleport_actor = ensure_actor(
        "Kem_3Block_TeleportSpawn",
        unreal.TargetPoint,
        make_vector(320.0, 2062.0, 12.0),
        make_rotator(0.0, 0.0, 0.0),
        folder_spawn,
    )

    teleport_actor.tags = [tag for tag in teleport_actor.tags if str(tag) != SPAWN_TAG]
    teleport_actor.tags.append(SPAWN_TAG)

    shared_screen = ensure_actor(
        "Kem_3Block_SharedScreen",
        shared_screen_class,
        make_vector(1140.0, 2060.0, 175.0),
        make_rotator(0.0, 180.0, 0.0),
        folder_stage,
    )

    browser_desk = ensure_actor(
        "Kem_3Block_BrowserDesk",
        browser_desk_class,
        make_vector(900.0, 1960.0, 115.0),
        make_rotator(0.0, 205.0, 0.0),
        folder_stage,
    )

    teacher_panel = ensure_actor(
        "Kem_3Block_TeacherPanel",
        teacher_panel_class,
        make_vector(875.0, 2190.0, 135.0),
        make_rotator(0.0, 160.0, 0.0),
        folder_stage,
    )

    room_code_panel = ensure_actor(
        "Kem_3Block_RoomCode",
        room_code_class,
        make_vector(980.0, 1885.0, 190.0),
        make_rotator(0.0, 195.0, 0.0),
        folder_stage,
    )

    hub_preview = ensure_actor(
        "Kem_3Block_HubPreview",
        hub_class,
        make_vector(-180.0, 2062.0, 90.0),
        make_rotator(0.0, 0.0, 0.0),
        folder_stage,
    )

    sun_light = ensure_actor(
        "Kem_3Block_SunLight",
        unreal.DirectionalLight,
        make_vector(400.0, 1700.0, 520.0),
        make_rotator(-38.0, -22.0, 0.0),
        folder_lighting,
    )
    configure_directional_light(sun_light)

    sky_light = ensure_actor(
        "Kem_3Block_SkyLight",
        unreal.SkyLight,
        make_vector(500.0, 2060.0, 320.0),
        make_rotator(0.0, 0.0, 0.0),
        folder_lighting,
    )
    configure_sky_light(sky_light)

    sky_atmosphere = ensure_actor(
        "Kem_3Block_SkyAtmosphere",
        unreal.SkyAtmosphere,
        make_vector(500.0, 2060.0, 0.0),
        make_rotator(0.0, 0.0, 0.0),
        folder_lighting,
    )

    height_fog = ensure_actor(
        "Kem_3Block_Fog",
        unreal.ExponentialHeightFog,
        make_vector(500.0, 2060.0, 0.0),
        make_rotator(0.0, 0.0, 0.0),
        folder_lighting,
    )
    configure_fog(height_fog)

    stage_fill_left = ensure_actor(
        "Kem_3Block_StageFillLeft",
        unreal.RectLight,
        make_vector(930.0, 1930.0, 245.0),
        make_rotator(-65.0, 180.0, 0.0),
        folder_lighting,
    )
    configure_rect_light(stage_fill_left, 4200.0)

    stage_fill_right = ensure_actor(
        "Kem_3Block_StageFillRight",
        unreal.RectLight,
        make_vector(930.0, 2190.0, 245.0),
        make_rotator(-65.0, 180.0, 0.0),
        folder_lighting,
    )
    configure_rect_light(stage_fill_right, 4200.0)

    ensure_attachment(shared_screen, stage_root)
    ensure_attachment(browser_desk, stage_root)
    ensure_attachment(teacher_panel, stage_root)
    ensure_attachment(room_code_panel, stage_root)
    ensure_attachment(teleport_actor, spawn_root)
    ensure_attachment(hub_preview, stage_root)
    ensure_attachment(sun_light, lighting_root)
    ensure_attachment(sky_light, lighting_root)
    ensure_attachment(sky_atmosphere, lighting_root)
    ensure_attachment(height_fog, lighting_root)
    ensure_attachment(stage_fill_left, lighting_root)
    ensure_attachment(stage_fill_right, lighting_root)

    set_actor_scale(shared_screen, 1.1, 1.1, 1.1)
    set_actor_scale(browser_desk, 1.0, 1.0, 1.0)
    set_actor_scale(teacher_panel, 1.0, 1.0, 1.0)
    set_actor_scale(room_code_panel, 1.0, 1.0, 1.0)
    set_actor_scale(hub_preview, 1.0, 1.0, 1.0)
    update_sky_sphere(sun_light)

    unreal.EditorLevelLibrary.save_current_level()
    unreal.EditorAssetLibrary.save_asset(NEW_MAP, False)
    log("3_Block setup complete.")


if __name__ == "__main__":
    main()
