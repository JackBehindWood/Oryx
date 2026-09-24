from build_system.setup import stale_objects


def test_removed_oryx_python_source_clears_its_binaries(tmp_path, monkeypatch):
    build_dir = tmp_path / "build"
    monkeypatch.setattr(stale_objects, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(stale_objects, "BUILD_DIR", build_dir)
    monkeypatch.setattr(stale_objects, "SOURCE_MANIFEST_FILE", build_dir / ".sources")

    source = tmp_path / "OryxPython" / "src" / "Module.cpp"
    source.parent.mkdir(parents=True)
    source.touch()
    stale_objects.record_source_manifest()
    assert "OryxPython/src/Module.cpp" in stale_objects.source_manifest()

    output_dir = build_dir / "bin" / "Debug-macosx-ARM64" / "OryxPython"
    output_dir.mkdir(parents=True)
    source.unlink()

    assert stale_objects.clear_outputs_of_removed_sources() == ["OryxPython"]
    assert not output_dir.exists()
