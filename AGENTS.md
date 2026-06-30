# Repository Guidelines

## Project Structure & Module Organization

YiCAD is a Windows-first C++23/Qt 5.15 CAD application built with CMake and Conan. The application target lives under `YiCAD/`. Source code is organized by subsystem: `YiCAD/src/actions/` for user interaction actions, `YiCAD/src/cmd/` for command parsing, `YiCAD/src/kernel/` for data model, geometry, rendering, persistence, history, and GUI abstractions, `YiCAD/src/ui/` for Qt widgets and `.ui` forms, `YiCAD/src/ai/` for assistant features, and `YiCAD/src/main/` for application entry points. Runtime assets are in `YiCAD/res/`, translations in `YiCAD/ts/`, support files in `YiCAD/support/`, build helpers in `cmake/`, Conan profiles in `profiles/`, and license texts in `licenses/`. External dependency sources and build outputs belong in `external/` and `build/` and should not be committed.

## Build, Test, and Development Commands

Install Qt 5.15 and set `Qt5_DIR`, then build SARibbonBar and CDT into the install paths expected by `CMakePresets.json`.

```powershell
conan install . --output-folder=build/conan-release --profile=profiles/windows-msvc-release --build=never --lockfile=conan.lock
cmake --preset Release "-DCMAKE_PREFIX_PATH=$env:Qt5_DIR"
cmake --build --preset Release
cmake --install build/Release --config Release
```

Use the matching `Debug` preset and `profiles/windows-msvc-debug` for debug builds. The installed runnable binary is `build/<config>/bin/YiCAD.exe`.

## Coding Style & Naming Conventions

Follow the existing C++ style: 4-space indentation, braces on their own lines for namespaces/classes/functions, Qt idioms, and concise comments only where they clarify non-obvious behavior. Write new or updated comments in Chinese and use Doxygen-style documentation comments, for example `/// @brief 功能说明`; add tags such as `@param` and `@return` when they provide useful interface information. Keep source files encoded as UTF-8; the build passes `/utf-8` on MSVC.

Name classes, structs, enums, and other user-defined types in PascalCase with the subsystem prefix attached directly to the semantic name, as in `DmArc`. Use `Dm*` for data-model and geometry types, `Action*` for interaction commands, `UI*` for widgets and dialogs, `GL*` for OpenGL helpers, `Meta*` for serialization metadata, and `Filter*` for file formats. The suffix should be a clear, usually singular description of the type's responsibility, such as `Arc`, `DrawArc`, or `BlockDialog`; preserve established abbreviations and do not introduce generic, unprefixed type names where a subsystem prefix applies. Member variables follow the existing `m_` convention, including established ownership/type hints such as `m_p...` for pointers and `m_sp...` for shared pointers.

## Testing Guidelines

There is currently no committed CTest or unit-test tree. For changes, at minimum verify `cmake --build --preset <config>` and run the installed application. When adding tests, place them in a clear `tests/` tree or subsystem-specific test directory, name binaries/files with a `test_` prefix, and register them with CMake/CTest so CI can run them later.

## Commit & Pull Request Guidelines

Recent history uses short imperative subjects such as `add license` and `init`; keep commit subjects concise and focused. Pull requests should describe the user-visible change, list build/test commands run, link related issues, and include screenshots or recordings for UI changes. Note any dependency, license, resource, or packaging impact explicitly.

## Security & Configuration Tips

Do not commit local Qt paths, generated Conan files, binaries, credentials, API keys, or user-provided `*.pat`, `*.lin`, and `*.shx` resources. Keep third-party version changes synchronized across `README.md`, `conanfile.py`, `conan.lock`, CMake presets, and CI.
