import math
import os
import struct
import wave

import unreal


ROOT = "/Game/TraceMotiveTests"
MAP_PATH = ROOT + "/TM_FeatureLab"
ASSET_PATH = ROOT + "/Assets"
BLUEPRINT_PATH = ROOT + "/Blueprints"


def log(message):
    unreal.log("[TM Feature Lab Builder] " + message)


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def delete_if_exists(path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)


def create_material():
    asset_path = ASSET_PATH + "/M_TM_DemoAccent"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(
        "M_TM_DemoAccent", ASSET_PATH, unreal.Material, unreal.MaterialFactoryNew()
    )
    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -260, 0
    )
    color.set_editor_property("constant", unreal.LinearColor(0.025, 0.32, 0.72, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(
        color, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -260, 120
    )
    roughness.set_editor_property("r", 0.32)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def create_test_tone():
    asset_path = ASSET_PATH + "/S_TM_TestTone"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    saved_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
    wav_path = os.path.join(saved_dir, "TMFeatureLabTone.wav")
    sample_rate = 22050
    duration = 0.42
    frames = int(sample_rate * duration)
    with wave.open(wav_path, "wb") as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(sample_rate)
        samples = bytearray()
        for index in range(frames):
            envelope = min(1.0, index / 800.0) * max(0.0, 1.0 - index / frames)
            value = int(8000.0 * envelope * math.sin(2.0 * math.pi * 523.25 * index / sample_rate))
            samples.extend(struct.pack("<h", value))
        wav_file.writeframes(samples)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", wav_path)
    task.set_editor_property("destination_path", ASSET_PATH)
    task.set_editor_property("destination_name", "S_TM_TestTone")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.EditorAssetLibrary.load_asset(asset_path)


def create_blueprint(name):
    asset_path = BLUEPRINT_PATH + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.TraceMotiveFeatureLabActor)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, BLUEPRINT_PATH, unreal.Blueprint, factory
    )
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    return blueprint


def spawn_actor(actor_class, label, location, scenario, material, rotation=None):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        rotation or unreal.Rotator(0.0, 0.0, 0.0),
        transient=False,
    )
    actor.set_actor_label(label)
    actor.set_editor_property("scenario", scenario)
    actor.set_editor_property("demo_material", material)
    actor.tags = [unreal.Name("TraceMotiveDemo"), unreal.Name(label)]
    return actor


def spawn_text(label, text, location, size=46.0, color=None):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.TextRenderActor, unreal.Vector(*location)
    )
    actor.set_actor_label(label)
    component = actor.get_component_by_class(unreal.TextRenderComponent)
    component.set_editor_property("text", unreal.Text(text))
    component.set_editor_property("world_size", size)
    component.set_editor_property(
        "text_render_color", color or unreal.Color(90, 210, 255, 255)
    )
    actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    return actor


def spawn_floor(mesh, material):
    floor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(900.0, 500.0, -140.0)
    )
    floor.set_actor_label("TM_Lab_Floor")
    floor.set_actor_scale3d(unreal.Vector(44.0, 34.0, 0.25))
    component = floor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_material(0, material)
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)


def create_level():
    ensure_directory(ROOT)
    ensure_directory(ASSET_PATH)
    ensure_directory(BLUEPRINT_PATH)

    material = create_material()
    sound = create_test_tone()
    blueprints = {
        "door": create_blueprint("BP_TM_SecurityDoor"),
        "controller": create_blueprint("BP_TM_ReferenceController"),
        "error": create_blueprint("BP_TM_DeliberateError"),
        "favorite": create_blueprint("BP_TM_FavoriteDebugActor"),
    }

    delete_if_exists(MAP_PATH)
    if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
        raise RuntimeError("Could not create " + MAP_PATH)

    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings()
    world_settings.set_editor_property("default_game_mode", unreal.TraceMotiveLabGameMode)

    cube_mesh = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube")
    spawn_floor(cube_mesh, material)

    directional = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 1400.0), unreal.Rotator(-45.0, -35.0, 0.0)
    )
    directional.set_actor_label("TM_Lab_DirectionalLight")
    directional.light_component.set_editor_property("intensity", 5.0)
    skylight = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(0.0, 0.0, 1000.0)
    )
    skylight.set_actor_label("TM_Lab_SkyLight")

    player_start = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.PlayerStart,
        unreal.Vector(-1500.0, -1300.0, 260.0),
        unreal.Rotator(-8.0, 42.0, 0.0),
    )
    player_start.set_actor_label("TM_Lab_PlayerStart")

    spawn_text(
        "TM_Lab_Title",
        "TRACEMOTIVE FEATURE LAB",
        (700.0, -1420.0, 420.0),
        82.0,
        unreal.Color(255, 190, 24, 255),
    )
    spawn_text(
        "TM_Lab_Subtitle",
        "Open a TraceMotive tool, select the labeled station, then Play In Editor",
        (700.0, -1415.0, 330.0),
        31.0,
    )

    scenario = unreal.TMFeatureLabScenario
    station_class = unreal.TraceMotiveFeatureLabActor
    stations = {}

    specs = [
        ("TM_01_Search_SecurityDoor", (-900, -800, 0), scenario.SEARCH_AND_REFERENCES),
        ("TM_02_AssetUsage_Wall", (0, -800, 0), scenario.ASSET_USAGE),
        ("TM_03_Collision_Mover", (900, -1040, 0), scenario.COLLISION_MOVER),
        ("TM_03_Collision_Obstacle", (900, -560, 0), scenario.COLLISION_OBSTACLE),
        ("TM_04_Click_Target", (1800, -800, 0), scenario.CLICK_TARGET),
        ("TM_05_Audio_Source", (-900, 250, 0), scenario.AUDIO_SOURCE),
        ("TM_06_Widget_Lifecycle", (0, 250, 0), scenario.WIDGET_LIFECYCLE),
        ("TM_07_Instance_Target", (900, 250, 0), scenario.INSTANCE_TARGET),
        ("TM_07_Instance_Controller", (1250, 250, 0), scenario.INSTANCE_CONTROLLER),
        ("TM_08_Runtime_Error", (1800, 250, 0), scenario.RUNTIME_ERROR),
        ("TM_09_Global_Speed_Target", (-450, 1250, 0), scenario.SPEED_TARGET),
        ("TM_10_Viewport_Move_Helper", (450, 1250, 0), scenario.VIEWPORT_HELPER),
        ("TM_11_Class_Favorite", (1350, 1250, 0), scenario.HUB),
    ]

    blueprint_classes = {
        "TM_01_Search_SecurityDoor": blueprints["door"].generated_class(),
        "TM_07_Instance_Controller": blueprints["controller"].generated_class(),
        "TM_08_Runtime_Error": blueprints["error"].generated_class(),
        "TM_11_Class_Favorite": blueprints["favorite"].generated_class(),
    }

    for label, location, role in specs:
        actor = spawn_actor(
            blueprint_classes.get(label, station_class),
            label,
            location,
            role,
            material,
        )
        stations[label] = actor

    stations["TM_01_Search_SecurityDoor"].set_editor_property("door_type", "Security")
    stations["TM_01_Search_SecurityDoor"].set_editor_property(
        "diagnostic_state", "Keypad -> ActivateFromKeypad -> OpenSecurityDoor"
    )
    stations["TM_02_AssetUsage_Wall"].set_editor_property(
        "diagnostic_state", "References M_TM_DemoAccent"
    )
    stations["TM_05_Audio_Source"].set_editor_property("demo_sound", sound)
    stations["TM_07_Instance_Controller"].set_editor_property(
        "target_actor", stations["TM_07_Instance_Target"]
    )
    stations["TM_07_Instance_Controller"].set_editor_property(
        "related_actors", [stations["TM_07_Instance_Target"]]
    )
    stations["TM_07_Instance_Target"].tags = [
        unreal.Name("TraceMotiveDemo"),
        unreal.Name("TMReferenceTarget"),
    ]
    stations["TM_04_Click_Target"].set_editor_property(
        "diagnostic_state", "Click this cube during PIE"
    )
    stations["TM_09_Global_Speed_Target"].set_editor_property(
        "diagnostic_state", "Skip log target: BossPhase2"
    )

    row_labels = [
        ("TM_Row_EditorAnalysis", "EDITOR ANALYSIS: Reference / Asset / Collision / Click", (450, -1210, 260)),
        ("TM_Row_RuntimeTrace", "RUNTIME TRACE: Audio / Widget / Instance / Error", (450, -160, 260)),
        ("TM_Row_Utilities", "UTILITIES: Speed / Viewport Move / Favorites", (450, 840, 260)),
    ]
    for label, text, position in row_labels:
        spawn_text(label, text, position, 38.0, unreal.Color(255, 190, 24, 255))

    instructions = [
        ("TM_Note_Collision", "Select Mover + Obstacle\\nCollision Pair Analyzer", (900, -800, 310)),
        ("TM_Note_Click", "Arm Click Diagnostics\\nthen click cube or UI button", (1800, -800, 310)),
        ("TM_Note_Widget", "Open Widget Lifecycle / Click Flow\\nbefore PIE", (0, 250, 310)),
        ("TM_Note_Instance", "Trace Instance Target\\ncontroller toggles collision + visibility", (1075, 250, 345)),
        ("TM_Note_Error", "Intentional error every 6 sec\\nBP_TM_DeliberateError", (1800, 250, 310)),
        ("TM_Note_Speed", "Speed ruler + Skip target\\nLog contains BossPhase2", (-450, 1250, 310)),
        ("TM_Note_Package", "Package Progress: package this project\\nShortcut Guide: click any graph\\nOutliner Search: query DoorType=Security", (1750, 1250, 330)),
    ]
    for label, text, position in instructions:
        spawn_text(label, text, position, 25.0, unreal.Color(205, 225, 240, 255))

    unreal.EditorLevelLibrary.save_current_level()
    unreal.EditorAssetLibrary.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
    log("Created and saved " + MAP_PATH)
    log("Stations: " + str(len(stations)))


create_level()
