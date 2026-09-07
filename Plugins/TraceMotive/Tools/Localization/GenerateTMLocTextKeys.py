from pathlib import Path
import re

PLUGIN_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PLUGIN_ROOT / 'Source/TraceMotive'
OUT_PATH = SOURCE_ROOT / 'Private/TMLocTextKeys.cpp'
CALL_PATTERN = re.compile(r'TMLoc::(?:Text|String)\(\s*TEXT\("((?:\\.|[^"\\])*)"\)')


def decode_cpp_string(value: str) -> str:
    value = value.replace(r'\\', '\\')
    return value.replace(r'\n', '\n').replace(r'\r', '\r').replace(r'\t', '\t').replace(r'\"', '"')


def cpp_escape(value: str) -> str:
    return value.replace('\\', '\\\\').replace('"', '\\"').replace('\r', '').replace('\n', '\\n')


def collect_keys() -> list[str]:
    keys: set[str] = set()
    for path in SOURCE_ROOT.rglob('*'):
        if path.suffix not in {'.cpp', '.h'} or path.name == OUT_PATH.name:
            continue
        text = path.read_text(encoding='utf-8')
        keys.update(decode_cpp_string(match.group(1)) for match in CALL_PATTERN.finditer(text))
    return sorted(key for key in keys if key)


def main() -> None:
    keys = collect_keys()
    lines = [
        '// Copyright Epic Games, Inc. All Rights Reserved.\n',
        '// Generated from TMLoc call sites. Do not edit manually.\n\n',
        '#include "TMLocalization.h"\n\n',
        'namespace\n{\n',
        '    void RegisterTMLocalizationTextKeys()\n    {\n',
    ]
    for key in keys:
        escaped = cpp_escape(key)
        lines.append(f'        (void)NSLOCTEXT("TraceMotive", "{escaped}", "{escaped}");\n')
    lines.extend([
        '    }\n\n',
        '    struct FTMLocalizationTextKeyAutoRegister\n    {\n',
        '        FTMLocalizationTextKeyAutoRegister() { RegisterTMLocalizationTextKeys(); }\n',
        '    };\n\n',
        '    static FTMLocalizationTextKeyAutoRegister GTMLocalizationTextKeyAutoRegister;\n',
        '}\n',
    ])
    OUT_PATH.write_text(''.join(lines), encoding='utf-8')
    print(f'generated {OUT_PATH} with {len(keys)} keys from TMLoc call sites')


if __name__ == '__main__':
    main()
