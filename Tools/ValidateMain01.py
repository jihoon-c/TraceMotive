import unreal


def log(message):
    unreal.log("[CPB_VALIDATE] " + str(message))


def warn(message):
    unreal.log_warning("[CPB_VALIDATE] " + str(message))


def asset(path):
    loaded = unreal.EditorAssetLibrary.load_asset(path)
    if not loaded:
        warn(f"Missing asset: {path}")
    return loaded


def class_asset(path):
    loaded_class = unreal.load_class(None, path + "_C")
    if loaded_class:
        return loaded_class

    loaded = asset(path)
    if not loaded:
        return None

    return loaded.get_editor_property("generated_class")


def describe_class(value):
    if not value:
        return "None"
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def inspect_game_settings():
    maps = unreal.GameMapsSettings.get_game_maps_settings()
    log(f"EditorStartupMap = {maps.get_editor_property('editor_startup_map')}")
    log(f"GameDefaultMap = {maps.get_editor_property('game_default_map')}")
    log(f"GameInstanceClass = {describe_class(maps.get_editor_property('game_instance_class'))}")
    log(f"GlobalDefaultGameMode = {describe_class(maps.get_editor_property('global_default_game_mode'))}")


def inspect_game_mode():
    bp_game_mode_class = class_asset("/cppBuilderC/BP/BP_GameMode.BP_GameMode")
    if not bp_game_mode_class:
        return
    cdo = unreal.get_default_object(bp_game_mode_class)
    log(f"BP_GameMode class = {describe_class(bp_game_mode_class)}")
    log(f"DefaultPawnClass = {describe_class(cdo.get_editor_property('default_pawn_class'))}")
    log(f"PlayerControllerClass = {describe_class(cdo.get_editor_property('player_controller_class'))}")


def inspect_player_controller():
    pc_class = class_asset("/cppBuilderC/BP/PlayerController.PlayerController")
    if not pc_class:
        return
    cdo = unreal.get_default_object(pc_class)
    log(f"PlayerController class = {describe_class(pc_class)}")
    components = cdo.get_components_by_class(unreal.ActorComponent)
    for component in components:
        class_name = component.get_class().get_name()
        if "UIRoot" in component.get_name() or "CPBUIRoot" in class_name:
            log(f"Found UI root component: {component.get_name()} / {class_name}")
            try:
                log(f"  MainWidgetClass = {describe_class(component.get_editor_property('main_widget_class'))}")
                log(f"  bCreateOnBeginPlay = {component.get_editor_property('b_create_on_begin_play')}")
            except Exception as exc:
                warn(f"  Could not inspect UI root properties: {exc}")


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def inspect_main_level():
    if not unreal.EditorLevelLibrary.load_level("/Game/Main01"):
        warn("Could not load /Game/Main01")
        return

    world = unreal.EditorLevelLibrary.get_editor_world()
    log(f"Loaded world = {world.get_name() if world else 'None'}")
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    log(f"Actor count = {len(actors)}")

    log("All actors:")
    for actor in actors:
        log(f"  {actor.get_actor_label()} :: {actor.get_class().get_path_name()}")

    interesting_terms = [
        "Bootstrap",
        "Sequence",
        "Interactor",
        "PlayerStart",
        "GameMode",
        "Pawn",
        "Widget",
    ]

    interesting = []
    for actor in actors:
        class_path = actor.get_class().get_path_name()
        name = actor.get_actor_label()
        if any(term.lower() in name.lower() or term.lower() in class_path.lower() for term in interesting_terms):
            interesting.append((name, class_path))

    if not interesting:
        warn("No obvious CPB bootstrap/sequence/interactor actors found in Main01.")
    else:
        log("Interesting actors:")
        for name, class_path in interesting:
            log(f"  {name} :: {class_path}")

    sequence_managers = [
        actor for actor in actors
        if "SequenceManager" in actor.get_class().get_path_name() or "SequenceManager" in actor.get_actor_label()
    ]
    if not sequence_managers:
        warn("No BP_SequenceManager actor found in Main01.")
    for manager in sequence_managers:
        log(f"Inspect SequenceManager: {manager.get_actor_label()}")
        registered = prop(manager, "registered_sequences", [])
        active = prop(manager, "active_sequence", None)
        log(f"  ActiveSequence = {active.get_actor_label() if active else 'None'}")
        log(f"  RegisteredSequences count = {len(registered) if registered else 0}")
        for idx, sequence in enumerate(registered or []):
            if not sequence:
                warn(f"  [{idx}] None")
                continue
            log(f"  [{idx}] {sequence.get_actor_label()} SequenceId={prop(sequence, 'sequence_id', '<?>')} SortOrder={prop(sequence, 'sort_order', '<?>')} StepCount={len(prop(sequence, 'steps', []) or [])}")

    bootstrap_actors = [
        actor for actor in actors
        if "Bootstrap" in actor.get_class().get_path_name() or "Bootstrap" in actor.get_actor_label()
    ]
    if not bootstrap_actors:
        warn("No Bootstrap actor found in Main01. Auto-start setup must be done elsewhere or it will not start.")

    player_starts = [
        actor for actor in actors
        if actor.get_class().get_name() == "PlayerStart" or "PlayerStart" in actor.get_class().get_path_name()
    ]
    if not player_starts:
        warn("No PlayerStart actor found in Main01.")


def main():
    inspect_game_settings()
    inspect_game_mode()
    inspect_player_controller()
    inspect_main_level()


main()
