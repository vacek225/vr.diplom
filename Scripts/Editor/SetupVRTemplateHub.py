import unreal

MAP_PATH = "/Game/VRTemplate/Lobby"
HUB_LABEL = "Kem_Lobby_Hub"
LEGACY_HUB_LABEL = "Kem_VRTemplate_Hub"
HUB_FOLDER = "Kem/Lobby"


def log(message: str) -> None:
    unreal.log(f"[SetupVRTemplateHub] {message}")


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


def ensure_actor(label: str, actor_class, location, rotation, folder: str = ""):
    actor = find_actor_by_label(label)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, rotation)
        actor.set_actor_label(label)
    else:
        actor.set_actor_location(location, False, False)
        actor.set_actor_rotation(rotation, False)

    if folder:
        actor.set_folder_path(folder)

    return actor


def find_hub_actor():
    actor = find_actor_by_label(HUB_LABEL)
    if actor is not None:
        return actor

    actor = find_actor_by_label(LEGACY_HUB_LABEL)
    if actor is not None:
        actor.set_actor_label(HUB_LABEL)
        return actor

    return None


def main() -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level_subsystem.load_level(MAP_PATH)

    hub_class = unreal.load_class(None, "/Script/KemSUinVR.KemHubActor")
    if hub_class is None:
        raise RuntimeError("Failed to load /Script/KemSUinVR.KemHubActor")

    hub_actor = find_hub_actor()
    if hub_actor is None:
        hub_actor = ensure_actor(
            HUB_LABEL,
            hub_class,
            make_vector(160.0, 0.0, 90.0),
            make_rotator(0.0, 180.0, 0.0),
            HUB_FOLDER,
        )
    else:
        hub_actor.set_actor_location(make_vector(160.0, 0.0, 90.0), False, False)
        hub_actor.set_actor_rotation(make_rotator(0.0, 180.0, 0.0), False)
        hub_actor.set_folder_path(HUB_FOLDER)

    hub_actor.set_actor_scale3d(make_vector(1.0, 1.0, 1.0))

    unreal.EditorLevelLibrary.save_current_level()
    unreal.EditorAssetLibrary.save_asset(MAP_PATH, False)
    log("Lobby hub actor ensured and level saved.")


if __name__ == "__main__":
    main()
