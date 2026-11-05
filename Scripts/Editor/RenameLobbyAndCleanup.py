import unreal

OLD_MAP = "/Game/VRTemplate/VRTemplateMap"
NEW_MAP = "/Game/VRTemplate/Lobby"
OLD_BUILDDATA = "/Game/VRTemplate/VRTemplateMap_BuiltData"
NEW_BUILDDATA = "/Game/VRTemplate/Lobby_BuiltData"
OLD_AUDITORIUM = "/Game/VRTemplate/Auditorium"
OLD_NEW_WORLD = "/Game/VRTemplate/NewWorld"

LOBBY_HUB_LABEL = "Kem_Lobby_Hub"
LEGACY_HUB_LABEL = "Kem_VRTemplate_Hub"
PREVIEW_LABEL = "Kem_3Block_HubPreview"


def log(message: str) -> None:
    unreal.log(f"[RenameLobbyAndCleanup] {message}")


def make_vector(x: float, y: float, z: float) -> unreal.Vector:
    return unreal.Vector(float(x), float(y), float(z))


def make_rotator(pitch: float, yaw: float, roll: float = 0.0) -> unreal.Rotator:
    return unreal.Rotator(float(pitch), float(yaw), float(roll))


def ensure_asset_under_new_name(old_path: str, new_path: str) -> None:
    if unreal.EditorAssetLibrary.does_asset_exist(new_path):
        log(f"Asset already present: {new_path}")
        return

    if not unreal.EditorAssetLibrary.does_asset_exist(old_path):
        log(f"Asset not found, skipping rename: {old_path}")
        return

    if unreal.EditorAssetLibrary.rename_asset(old_path, new_path):
        log(f"Renamed {old_path} -> {new_path}")
        return

    if unreal.EditorAssetLibrary.duplicate_asset(old_path, new_path):
        log(f"Duplicated {old_path} -> {new_path}")
        return

    raise RuntimeError(f"Failed to create {new_path} from {old_path}")


def delete_asset_if_exists(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        log(f"Asset already absent: {path}")
        return

    if not unreal.EditorAssetLibrary.delete_asset(path):
        raise RuntimeError(f"Failed to delete asset: {path}")

    log(f"Deleted asset: {path}")


def find_actor_by_label(label: str):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def reposition_actor(label: str, location, rotation=None, rename_to: str = "") -> None:
    actor = find_actor_by_label(label)
    if actor is None:
        return

    actor.set_actor_location(location, False, False)
    if rotation is not None:
        actor.set_actor_rotation(rotation, False)
    if rename_to:
        actor.set_actor_label(rename_to)


def update_lobby_level() -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level_subsystem.load_level(NEW_MAP)

    reposition_actor(LEGACY_HUB_LABEL, make_vector(160.0, 0.0, 90.0), make_rotator(0.0, 180.0, 0.0), LOBBY_HUB_LABEL)
    reposition_actor(LOBBY_HUB_LABEL, make_vector(160.0, 0.0, 90.0), make_rotator(0.0, 180.0, 0.0))

    unreal.EditorLevelLibrary.save_current_level()
    unreal.EditorAssetLibrary.save_asset(NEW_MAP, False)
    log("Lobby level updated.")


def update_block3_preview() -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level_subsystem.load_level("/Game/VRTemplate/3_Block")

    reposition_actor(PREVIEW_LABEL, make_vector(-180.0, 2062.0, 90.0), make_rotator(0.0, 0.0, 0.0))

    unreal.EditorLevelLibrary.save_current_level()
    unreal.EditorAssetLibrary.save_asset("/Game/VRTemplate/3_Block", False)
    log("3_Block hub preview updated.")


def main() -> None:
    ensure_asset_under_new_name(OLD_MAP, NEW_MAP)
    ensure_asset_under_new_name(OLD_BUILDDATA, NEW_BUILDDATA)
    update_lobby_level()
    update_block3_preview()
    delete_asset_if_exists(OLD_MAP)
    delete_asset_if_exists(OLD_BUILDDATA)
    delete_asset_if_exists(OLD_AUDITORIUM)
    delete_asset_if_exists(OLD_NEW_WORLD)


if __name__ == "__main__":
    main()
