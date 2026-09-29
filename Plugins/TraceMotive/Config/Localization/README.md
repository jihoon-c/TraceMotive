# TraceMotive Localization Workflow

TraceMotive uses its project-plugin CSV at runtime so the language can be changed
inside TraceMotive without changing the Unreal Editor culture. The generated
manifest/archive/PO/`.locres` files are also shipped for Unreal localization
tooling and future culture-based integration.

## Source of truth

- C++ static UI text: `TMLoc::Text` / `TMLoc::String` call sites.
- Generated gather bridge: `Source/TraceMotive/Private/TMLocTextKeys.cpp`. Generate it with `GenerateTMLocTextKeys.py`; do not edit it.
- Runtime Korean translations: `Config/Localization/TMLocalization.ko.csv`.
- Unreal localization resources: `Content/Localization/TraceMotive/ko/TraceMotive.po`
  and the generated `.locres`.
- Runtime resources: generated `TraceMotive.locres` files.

## Update flow

1. Add UI text through `TMLoc::Text`/`String`, supplying a Korean fallback when practical.
2. Add or update the same English key in `TMLocalization.ko.csv`.
3. Run `Tools/Localization/UpdateTMLocalization.ps1` to refresh the Unreal resources.
4. Run `ValidateTMLocalization.ps1` and test both language buttons in the editor.

Validation fails on raw static `FText::FromString(TEXT(...))` calls and common mojibake markers, preventing new UI text from bypassing localization. Dynamic object names, asset paths, values, and logs remain non-localized by design.
