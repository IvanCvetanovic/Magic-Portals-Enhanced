"""make_mr_controls.py, on packages this file builds itself.

No Magic Rampage file is ever opened here and no image is stored anywhere: every sprite below is
made by the test from numpy arrays (a palette PNG with a tRNS chunk, a grey + alpha PNG, ...), in
a temporary folder, with the one shape the tool measures - a solid 100 x 100 body at (14, 14, 114,
114) inside a faint 8 px shadow, as the real pad sprites have. Run from the repository root:

    tools/parity/.venv/Scripts/python -I -B -m pytest -q -p no:cacheprovider tools/art/tests
"""

from __future__ import annotations

import ast
import hashlib
import importlib.util
import io
import json
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

import numpy as np
import pytest
from PIL import Image

TOOL = Path(__file__).resolve().parents[1] / "make_mr_controls.py"
_spec = importlib.util.spec_from_file_location("make_mr_controls", TOOL)
mr = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = mr                    # the tool's dataclasses look themselves up there
_spec.loader.exec_module(mr)
BODY = mr.BODY_RECT

# The stand-in palette: 0 transparent with a loud colour under it, 1 the shadow, 2 the rim,
# 3 the face, 4 ink, 5 exact magenta, 6 a near miss.
PALETTE = [(200, 30, 40), (10, 10, 10), (180, 180, 180), (100, 100, 100), (20, 20, 20),
           (255, 0, 255), (255, 0, 254)]
ALPHAS = [0, 2, 255, 255, 255, 255, 255]


# --- fixtures: a package built from arrays ---------------------------------------------------

def frame_indices() -> np.ndarray:
    index = np.zeros((128, 128), np.uint8)
    index[6:122, 6:122] = 1
    index[14:114, 14:114] = 2
    index[30:98, 30:98] = 3
    return index


def write_palette_png(path: Path, index: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image = Image.frombytes("P", (index.shape[1], index.shape[0]), index.tobytes())
    image.putpalette([channel for colour in PALETTE for channel in colour])
    image.save(path, transparency=bytes(ALPHAS))


def left_indices(stripe: int = 4) -> np.ndarray:
    index = frame_indices()
    index[40:90, 40:90:2] = stripe          # one-pixel stripes: any resampling would blur them
    return index


def right_indices() -> np.ndarray:
    index = frame_indices()
    index[40:90:2, 40:90] = 4
    return index


def restart_icon_array(halo: bool = True) -> np.ndarray:
    """Grey + alpha, 64 x 64: a white ring, with a grey of 77 under the alpha-0 background and,
    like the real icon, a soft dark shadow (grey 0, low alpha) round the ring."""
    yy, xx = np.mgrid[0:64, 0:64]
    distance = np.hypot(yy - 31.5, xx - 31.5)
    ring = (distance >= 14) & (distance <= 21)
    array = np.zeros((64, 64, 2), np.uint8)
    array[:, :, 0] = 77
    if halo:
        shadow = ((distance >= 10) & (distance < 14)) | ((distance > 21) & (distance <= 26))
        array[shadow] = (0, 40)
    array[ring] = 255
    return array


def write_package(root: Path, *, left: np.ndarray | None = None, halo: bool = True) -> Path:
    write_palette_png(root / "sprites/dpad-frame.png", frame_indices())
    write_palette_png(root / "sprites/dpad-left.png", left_indices() if left is None else left)
    write_palette_png(root / "sprites/dpad-right.png", right_indices())
    Image.fromarray(restart_icon_array(halo)).save(root / "sprites/icon-restart-64.png")
    for sprite in mr.SPRITES:
        path = root / sprite.member
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            Image.new("RGBA", (4, 4), (1, 2, 3, 4)).save(path)
    return root


def write_xapk(path: Path, package: Path) -> Path:
    inner = io.BytesIO()
    with zipfile.ZipFile(inner, "w") as apk:
        for member in sorted(package.rglob("*.png")):
            apk.write(member, "assets/" + member.relative_to(package).as_posix())
    with zipfile.ZipFile(path, "w") as outer:
        outer.writestr(mr.BASE_APK, inner.getvalue())
        outer.writestr("manifest.json",
                       json.dumps({"version_name": "7.8.7", "version_code": "1214"}))
    return path


@pytest.fixture
def package(tmp_path: Path) -> Path:
    return write_package(tmp_path / "package")


@pytest.fixture
def repo(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    """A git repository, outside this one, that ignores reference/ the way the real one does."""
    if shutil.which("git") is None:
        pytest.skip("git is not on PATH")
    root = tmp_path / "repo"
    root.mkdir()
    subprocess.run(["git", "init", "-q"], cwd=root, check=True)
    (root / ".gitignore").write_text("reference/\n")
    monkeypatch.chdir(root)
    return root.resolve()


def listing(root: Path) -> list[str]:
    return sorted(path.relative_to(root).as_posix() for path in root.rglob("*")
                  if ".git" not in path.relative_to(root).parts)


def fill_pixels(image: Image.Image) -> np.ndarray:
    """The glyph's grey-215 fill (its edge pixels read about 200): lighter than the stand-in
    frame's rim, 180, and than everything else in it."""
    array = np.asarray(image)
    return (array[:, :, :3].min(axis=2) >= 195) & (array[:, :, 3] == 255)


def centre_of(mask: np.ndarray) -> tuple[float, float]:
    ys, xs = np.nonzero(mask)
    return float((xs + 0.5).mean()), float((ys + 0.5).mean())


# --- pixels ----------------------------------------------------------------------------------

def test_straight_rgba_from_palette_and_trns(tmp_path: Path) -> None:
    index = np.array([[2, 1, 0, 3]], np.uint8)
    write_palette_png(tmp_path / "p.png", index)
    on_disk = Image.open(tmp_path / "p.png")
    assert on_disk.mode == "P" and "transparency" in on_disk.info     # a palette PNG with a tRNS
    out = np.asarray(mr.decode((tmp_path / "p.png").read_bytes()))
    assert out.shape == (1, 4, 4)
    assert out[0, :, 3].tolist() == [255, 2, 0, 255]                  # alpha is the tRNS entry's
    assert out[0, 0, :3].tolist() == list(PALETTE[2])                 # colour untouched: straight
    assert out[0, 1, :3].tolist() == list(PALETTE[1])                 # not scaled by alpha 2


def test_straight_rgba_from_la(tmp_path: Path) -> None:
    array = np.array([[[200, 100], [31, 255]]], np.uint8)
    Image.fromarray(array).save(tmp_path / "la.png")
    assert Image.open(tmp_path / "la.png").mode == "LA"
    out = np.asarray(mr.decode((tmp_path / "la.png").read_bytes()))
    assert out[0, 0].tolist() == [200, 200, 200, 100]
    assert out[0, 1].tolist() == [31, 31, 31, 255]


def test_colour_under_alpha_zero_is_black(tmp_path: Path, package: Path) -> None:
    # The fixtures really do carry a colour under alpha 0, or this test could not fail.
    plain = np.asarray(Image.open(package / "sprites/dpad-frame.png").convert("RGBA"))
    assert (plain[:, :, 3] == 0).any() and (plain[:, :, :3][plain[:, :, 3] == 0] != 0).any()
    plain_la = np.asarray(Image.open(package / "sprites/icon-restart-64.png").convert("RGBA"))
    assert (plain_la[:, :, :3][plain_la[:, :, 3] == 0] != 0).any()

    for member in ("sprites/dpad-frame.png", "sprites/icon-restart-64.png"):
        out = np.asarray(mr.decode((package / member).read_bytes()))
        assert not out[:, :, :3][out[:, :, 3] == 0].any()
    for output in mr.derive(mr.FolderPackage(package)):
        array = np.asarray(output.image)
        assert (array[:, :, 3] == 0).any(), output.name                # there is an alpha 0 to test
        assert not array[:, :, :3][array[:, :, 3] == 0].any(), output.name


def test_an_exact_magenta_pixel_is_refused(tmp_path: Path) -> None:
    magenta = left_indices()
    magenta[64, 64] = 5
    write_package(tmp_path / "bad", left=magenta)
    with pytest.raises(mr.ToolError, match="magenta"):
        mr.derive(mr.FolderPackage(tmp_path / "bad"))
    near = left_indices()
    near[64, 64] = 6                                                  # (255, 0, 254): not magenta
    write_package(tmp_path / "near", left=near)
    assert len(mr.derive(mr.FolderPackage(tmp_path / "near"))) == len(mr.OUTPUT_NAMES)


def test_as_shipped_sprites_are_not_resampled(package: Path) -> None:
    outputs = {output.name: output for output in mr.derive(mr.FolderPackage(package))}
    for name, member in (("walk_left.png", "dpad-left"), ("walk_right.png", "dpad-right")):
        expected = np.array(Image.open(package / f"sprites/{member}.png").convert("RGBA"))
        expected[expected[:, :, 3] == 0, :3] = 0                       # the only change allowed
        assert outputs[name].image.size == (128, 128)
        assert np.array_equal(np.asarray(outputs[name].image), expected)
    stripes = np.asarray(outputs["walk_left.png"].image)[60, 40:90, 0]
    assert len(set(stripes.tolist())) == 2                           # the 1 px stripes survive
    # A sprite of another size is refused, never scaled to fit.
    odd = Image.new("RGBA", (130, 130), (0, 0, 0, 255))
    odd.save(package / "sprites/dpad-left.png")
    with pytest.raises(mr.ToolError, match="never resampled"):
        mr.derive(mr.FolderPackage(package))


# --- the hard rule ---------------------------------------------------------------------------

def test_refuses_an_out_that_git_does_not_ignore(repo: Path, package: Path, tmp_path: Path,
                                                 capsys: pytest.CaptureFixture[str]) -> None:
    (repo / "docs").mkdir()
    (repo / "docs" / "keep.txt").write_text("x")
    before = listing(repo)

    assert mr.main(["--assets", str(package), "--out", "docs"]) == mr.EXIT_FAILED
    assert "ignored" in capsys.readouterr().err
    # nothing at all: no PNG, no manifest, and no extracted sprites anywhere
    assert listing(repo) == before

    elsewhere = tmp_path / "elsewhere" / "controls"
    assert mr.main(["--assets", str(package), "--out", str(elsewhere)]) == mr.EXIT_FAILED
    assert "outside the repository" in capsys.readouterr().err
    assert not elsewhere.parent.exists() and listing(repo) == before

    # An ignored folder is accepted ...
    assert mr.main(["--assets", str(package), "--out", "reference/magic-rampage/controls"]) == 0
    assert (repo / "reference/magic-rampage/controls/walk_left.png").is_file()
    # ... unless git tracks a file in it (an ignored folder does not make a tracked file ignored).
    subprocess.run(["git", "add", "-f", "reference/magic-rampage/controls/walk_left.png"],
                   cwd=repo, check=True)
    capsys.readouterr()
    assert mr.main(["--assets", str(package), "--out", "reference/magic-rampage/controls"]) \
        == mr.EXIT_FAILED
    assert "walk_left.png" in capsys.readouterr().err


def test_a_refusal_happens_before_the_package_is_opened(repo: Path, tmp_path: Path,
                                                        capsys: pytest.CaptureFixture[str]) -> None:
    # Not even a package that does not exist is looked at: the destination is judged first.
    assert mr.main(["--assets", str(tmp_path / "nowhere"), "--out", "docs"]) == mr.EXIT_FAILED
    assert "ignored" in capsys.readouterr().err


def test_the_sprites_go_to_the_fixed_assets_folder_whatever_out_is(repo: Path,
                                                                   package: Path) -> None:
    # The real repository ignores out/ too (the converter's output and the port's read folder), so
    # an --out there is accepted: it must still not receive the sprites, nor sprites beside it.
    (repo / ".gitignore").write_text("reference/\nout/\n")
    assert mr.main(["--assets", str(package), "--out", "out/x/controls"]) == 0
    written = sorted(path.relative_to(repo / "out").as_posix()
                     for path in (repo / "out").rglob("*") if path.is_file())
    assert written == sorted(f"x/controls/{name}" for name in (*mr.OUTPUT_NAMES, mr.MANIFEST_NAME))
    assert not list((repo / "out").rglob("assets"))
    dest = repo / "reference/magic-rampage/assets"
    assert (dest / "sprites/dpad-frame.png").is_file() and (dest / "extract.json").is_file()

    # The port's _ORIGINAL folder is reference/extracted/assets: an --out beside it leaves it alone.
    assert mr.main(["--assets", str(package), "--out", "reference/extracted/controls"]) == 0
    assert not (repo / "reference/extracted/assets").exists()
    assert (repo / "reference/extracted/controls" / mr.MANIFEST_NAME).is_file()


def test_the_gate_covers_the_extracted_sprites_too(repo: Path, package: Path,
                                                   capsys: pytest.CaptureFixture[str]) -> None:
    # Only the controls folder is ignored: the five images and the manifest could be written, the
    # sprites could not, and a run that would put them in an un-ignored folder writes nothing.
    (repo / ".gitignore").write_text("reference/magic-rampage/controls/\n")
    before = listing(repo)
    assert mr.main(["--assets", str(package)]) == mr.EXIT_FAILED
    assert "reference/magic-rampage/assets" in capsys.readouterr().err
    assert listing(repo) == before

    # Without the extract the same destination is fine: it was the sprites that were refused.
    assert mr.main(["--assets", str(package), "--no-extract"]) == 0
    assert (repo / "reference/magic-rampage/controls" / mr.MANIFEST_NAME).is_file()
    assert not (repo / "reference/magic-rampage/assets").exists()


def test_without_git_on_the_path_nothing_is_written(repo: Path, package: Path, tmp_path: Path,
                                                    monkeypatch: pytest.MonkeyPatch,
                                                    capsys: pytest.CaptureFixture[str]) -> None:
    # The repo fixture has run `git init` already: PATH is emptied only now, in the test body.
    before = listing(repo)
    empty = tmp_path / "an_empty_path_entry"
    empty.mkdir()
    monkeypatch.setenv("PATH", str(empty))
    try:
        subprocess.run(["git", "--version"], capture_output=True, check=False)
    except FileNotFoundError:
        pass
    else:
        pytest.skip("this platform finds git with an empty PATH")

    for argv in (["--assets", str(package)],
                 ["--assets", str(package), "--out", "docs"],
                 ["--check"],
                 ["--check", "--assets", str(package)]):
        assert mr.main(argv) == mr.EXIT_FAILED, argv
        assert "not on PATH" in capsys.readouterr().err, argv
        assert listing(repo) == before, argv


def test_the_default_destination_is_the_ignored_reference_folder(repo: Path, package: Path) -> None:
    assert mr.main(["--assets", str(package)]) == 0
    assert sorted(path.name for path in (repo / "reference/magic-rampage/controls").iterdir()) \
        == sorted([*mr.OUTPUT_NAMES, mr.MANIFEST_NAME])
    # the sprites went to the fixed assets folder, which is the default package of the next run
    assert (repo / "reference/magic-rampage/assets/sprites/dpad-frame.png").is_file()
    status = subprocess.run(["git", "status", "--porcelain"], cwd=repo, capture_output=True,
                            text=True, check=True).stdout
    assert "reference" not in status


def test_the_repository_root_comes_from_the_working_directory(
        repo: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    assert mr.repo_root(repo) == repo                                  # not this file's repository
    tree = ast.parse(TOOL.read_text("utf-8"))
    assert not [node for node in ast.walk(tree)
                if isinstance(node, ast.Name) and node.id == "__file__"]
    bare = tmp_path / "bare"
    bare.mkdir()
    monkeypatch.setenv("GIT_CEILING_DIRECTORIES", str(tmp_path))
    with pytest.raises(mr.ToolError, match="not inside a git repository"):
        mr.repo_root(bare)


def test_the_tool_imports_nothing_from_penumbra() -> None:
    tree = ast.parse(TOOL.read_text("utf-8"))
    modules = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            modules += [alias.name for alias in node.names]
        elif isinstance(node, ast.ImportFrom):
            modules.append(node.module or "")
    assert modules
    assert not [name for name in modules if "penumbra" in name.lower()]
    allowed = set(sys.stdlib_module_names) | {"numpy", "PIL"}
    assert {name.split(".")[0] for name in modules} <= allowed

    docstrings = {id(node.body[0].value) for node in ast.walk(tree)
                  if isinstance(node, (ast.Module, ast.FunctionDef, ast.ClassDef))
                  and node.body and isinstance(node.body[0], ast.Expr)
                  and isinstance(node.body[0].value, ast.Constant)}
    literals = [node.value for node in ast.walk(tree) if isinstance(node, ast.Constant)
                and isinstance(node.value, str) and id(node) not in docstrings]
    assert literals
    assert not [text for text in literals if "penumbra" in text.lower() or "mr_extract" in text]
    assert "penumbra" not in repr(vars(mr.build_parser().parse_args([]))).lower()


def test_the_tool_its_tests_and_its_doc_hold_no_image() -> None:
    art = Path(__file__).resolve().parents[1]
    suffixes = {".png", ".bmp", ".jpg", ".jpeg", ".gif", ".webp", ".ico"}
    assert not [path for path in art.rglob("*") if path.suffix.lower() in suffixes]
    needles = ["\x89" + "PNG", "iVBOR" + "w0KGgo", "data:" + "image", "/9j/" + "4AAQ"]
    texts = [path for path in art.rglob("*.py")]
    doc = art.parents[1] / "docs" / "magic-rampage-controls.md"
    if doc.is_file():
        texts.append(doc)
    for path in texts:
        text = path.read_text("utf-8")
        assert not [needle for needle in needles if needle in text], path


# --- the manifest, --check, the package ------------------------------------------------------

def test_manifest_records_sha256_size_and_body_rect(repo: Path, package: Path) -> None:
    assert mr.main(["--assets", str(package)]) == 0
    out = repo / "reference/magic-rampage/controls"
    manifest = json.loads((out / "manifest.json").read_text("utf-8"))
    assert manifest["tool_version"] == mr.TOOL_VERSION and manifest["stage"] == "controls"
    assert manifest["body_rect"] == [14, 14, 114, 114]
    assert manifest["package"]["kind"] == "assets-folder"
    assert sorted(manifest["outputs"]) == sorted(mr.OUTPUT_NAMES)
    for name, entry in manifest["outputs"].items():
        data = (out / name).read_bytes()
        assert entry["sha256"] == hashlib.sha256(data).hexdigest()
        assert entry["bytes"] == len(data)
        assert entry["size"] == [128, 128] and entry["mode"] == "RGBA"
        assert entry["body"] == [14, 14, 114, 114]
        assert entry["sources"] and entry["recipe"] and entry["recipe_id"] in mr.RECIPES
        pixels = np.asarray(Image.open(out / name))
        assert entry["pixels_sha256"] == hashlib.sha256(pixels.tobytes()).hexdigest()
        assert not (pixels[:, :, 3] == 0).all()
    for member, entry in manifest["sources"].items():
        assert entry["sha256"] == hashlib.sha256((package / member).read_bytes()).hexdigest()
    assert manifest["outputs"]["restart.png"]["sources"] == ["sprites/dpad-frame.png",
                                                             "sprites/icon-restart-64.png"]


def test_check_reproduces_the_bytes(repo: Path, package: Path,
                                    capsys: pytest.CaptureFixture[str]) -> None:
    assert mr.main(["--assets", str(package)]) == 0
    assert mr.main(["--assets", str(package), "--out", "reference/second/controls"]) == 0
    for name in (*mr.OUTPUT_NAMES, mr.MANIFEST_NAME):                 # a second run: the same bytes
        assert (repo / "reference/second/controls" / name).read_bytes() \
            == (repo / "reference/magic-rampage/controls" / name).read_bytes()
    before = listing(repo)
    assert mr.main(["--check", "--assets", str(package)]) == 0
    assert mr.main(["--check"]) == 0                                   # the extract is the package
    assert listing(repo) == before                                     # --check writes nothing

    # One pixel changed on disk is found, and named.
    path = repo / "reference/magic-rampage/controls/restart.png"
    pixels = np.array(Image.open(path))
    pixels[64, 64, 0] ^= 1
    Image.fromarray(pixels).save(path)
    capsys.readouterr()
    assert mr.main(["--check", "--assets", str(package)]) == mr.EXIT_FAILED
    assert "restart.png" in capsys.readouterr().err

    # A source that is not the one the outputs came from is found too.
    assert mr.main(["--assets", str(package)]) == 0
    write_package(package, left=left_indices(stripe=3))
    capsys.readouterr()
    assert mr.main(["--check", "--assets", str(package)]) == mr.EXIT_FAILED
    assert "dpad-left" in capsys.readouterr().err


def test_check_tells_a_reencoded_file_from_a_tampered_one(
        repo: Path, package: Path, capsys: pytest.CaptureFixture[str]) -> None:
    assert mr.main(["--assets", str(package)]) == 0
    out = repo / "reference/magic-rampage/controls"
    path = out / "restart.png"
    original = path.read_bytes()

    # The same pixels in other bytes: the file is not the one the manifest names.
    Image.open(path).save(path, compress_level=1)
    assert path.read_bytes() != original
    capsys.readouterr()
    assert mr.main(["--check", "--assets", str(package)]) == mr.EXIT_FAILED
    assert "file bytes differ" in capsys.readouterr().err

    # Other pixels whose hash the manifest was made to agree with: only the pixels give it away.
    path.write_bytes(original)
    pixels = np.array(Image.open(path))
    pixels[40, 40, 3] ^= 1
    Image.fromarray(pixels).save(path)
    manifest_path = out / "manifest.json"
    manifest = json.loads(manifest_path.read_text("utf-8"))
    manifest["outputs"]["restart.png"]["sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
    manifest_path.write_text(json.dumps(manifest))
    capsys.readouterr()
    assert mr.main(["--check", "--assets", str(package)]) == mr.EXIT_FAILED
    assert "pixels differ" in capsys.readouterr().err


def test_check_without_a_manifest_fails(repo: Path, package: Path) -> None:
    assert mr.main(["--check", "--assets", str(package)]) == mr.EXIT_FAILED


def test_an_xapk_gives_the_same_outputs_as_its_folder(
        repo: Path, package: Path, tmp_path: Path) -> None:
    assert mr.main(["--assets", str(package)]) == 0
    xapk = write_xapk(tmp_path / "Magic+Rampage_7.8.7_APKPure.xapk", package)
    assert mr.main(["--xapk", str(xapk), "--out", "reference/from_xapk/controls"]) == 0
    folder_manifest = repo / "reference/magic-rampage/controls/manifest.json"
    folder = json.loads(folder_manifest.read_text("utf-8"))
    packed = json.loads((repo / "reference/from_xapk/controls/manifest.json").read_text("utf-8"))
    assert packed["package"]["kind"] == "xapk" and packed["package"]["version_name"] == "7.8.7"
    assert packed["package"]["sha256"] == hashlib.sha256(xapk.read_bytes()).hexdigest()
    assert {name: entry["sha256"] for name, entry in packed["outputs"].items()} \
        == {name: entry["sha256"] for name, entry in folder["outputs"].items()}
    assert mr.main(["--check", "--xapk", str(xapk)]) == 0
    assert mr.main(["--xapk", str(tmp_path / "missing.xapk"), "--out", "reference/x/controls"]) == 1


def test_extract_copies_the_sprites_verbatim_and_the_copy_is_a_package(
        repo: Path, package: Path, capsys: pytest.CaptureFixture[str]) -> None:
    assert mr.main(["--assets", str(package)]) == 0
    dest = repo / "reference/magic-rampage/assets"
    record = json.loads((dest / "extract.json").read_text("utf-8"))
    assert sorted(record["files"]) == sorted(sprite.member for sprite in mr.SPRITES)
    for member, entry in record["files"].items():
        assert (dest / member).read_bytes() == (package / member).read_bytes()
        assert entry["sha256"] == hashlib.sha256((dest / member).read_bytes()).hexdigest()
    roles = {sprite.role for sprite in mr.SPRITES}
    assert roles == {mr.ROLE_CONTROL, mr.ROLE_EDITOR, mr.ROLE_SPARE}
    # the extract alone re-derives the same controls (the original package is needed once)
    shutil.rmtree(package)
    capsys.readouterr()
    assert mr.main(["--check"]) == 0
    # The default package is read, and it is the extract's own folder whatever --out is: nothing
    # is copied onto itself.
    assert mr.main(["--out", "reference/again/controls"]) == 0
    assert "nothing to extract" in capsys.readouterr().out
    assert mr.main(["--assets", str(dest)]) == 0
    assert "nothing to extract" in capsys.readouterr().out


def test_the_extract_list_covers_the_controls_and_the_editor_stage() -> None:
    members = {sprite.member for sprite in mr.SPRITES}
    controls = {"sprites/dpad-frame.png", "sprites/dpad-left.png", "sprites/dpad-right.png",
                "sprites/icon-restart-64.png"}
    editor = {"sprites/smaller-frame-bg.png", "sprites/smaller-frame-red-bg.png",
              "sprites/resize-symbol-shrink.png", "sprites/resize-symbol-enlarge.png",
              "sprites/adjust-symbol-transparent.png", "sprites/adjust-symbol-opaque.png",
              "sprites/lock-locked.png", "sprites/lock-unlocked.png",
              "sprites/inventory-back-button.png"}
    assert controls | editor <= members
    assert {sprite.member for sprite in mr.SPRITES if sprite.role == mr.ROLE_CONTROL} == controls
    assert {sprite.member for sprite in mr.SPRITES if sprite.role == mr.ROLE_EDITOR} == editor
    assert len(members) == len(mr.SPRITES)


def test_no_extract_writes_no_sprites(repo: Path, package: Path) -> None:
    assert mr.main(["--assets", str(package), "--no-extract"]) == 0
    assert not (repo / "reference/magic-rampage/assets").exists()
    assert mr.main(["--check"]) == mr.EXIT_FAILED                      # no default package yet
    assert mr.main(["--check", "--assets", str(package)]) == 0


def test_a_missing_source_is_reported_and_nothing_is_written(
        repo: Path, package: Path, capsys: pytest.CaptureFixture[str]) -> None:
    (package / "sprites/icon-restart-64.png").unlink()
    assert mr.main(["--assets", str(package)]) == mr.EXIT_FAILED
    assert "icon-restart-64.png" in capsys.readouterr().err
    assert not (repo / "reference").exists()


def test_the_editor_stage_is_not_yet(repo: Path, package: Path,
                                     capsys: pytest.CaptureFixture[str]) -> None:
    for stage in ("editor", "all"):
        assert mr.main(["--assets", str(package), "--stage", stage]) == mr.EXIT_USAGE
        assert "CTL-11" in capsys.readouterr().err
    assert not (repo / "reference").exists()


# --- the glyphs ------------------------------------------------------------------------------

def frame_image() -> Image.Image:
    return mr.decode(frame_png_bytes())


def frame_png_bytes() -> bytes:
    buffer = io.BytesIO()
    image = Image.frombytes("P", (128, 128), frame_indices().tobytes())
    image.putpalette([channel for colour in PALETTE for channel in colour])
    image.save(buffer, format="PNG", transparency=bytes(ALPHAS))
    return buffer.getvalue()


def test_composing_a_glyph_on_the_frame_is_centred_on_the_body() -> None:
    frame = frame_image()
    # A glyph that is not symmetric (an L: a bar and a foot): its extent is centred, to the pixel.
    def paint(draw, s):
        draw.rectangle([0, 0, 10 * s - 1, 40 * s - 1], fill=255)
        draw.rectangle([0, 30 * s, 30 * s - 1, 40 * s - 1], fill=255)

    composed = mr.place_on_body(frame, mr.styled(mr.paint_mask(30, 40, paint)))
    ys, xs = np.nonzero(fill_pixels(composed))
    assert xs.min() >= 14 and xs.max() < 114 and ys.min() >= 14 and ys.max() < 114
    assert abs((xs.min() + xs.max() + 1) / 2 - 64.0) <= 0.6            # the fill's extent: 30 x 40
    assert abs((ys.min() + ys.max() + 1) / 2 - 64.0) <= 0.6
    assert (xs.max() + 1 - xs.min(), ys.max() + 1 - ys.min()) == (30, 40)

    # The body is what it is centred on, not the 128 px image: a body off to one side takes it.
    moved = mr.place_on_body(frame, mr.styled(mr.paint_mask(30, 40, paint)), (20, 14, 114, 114))
    moved_xs = np.nonzero(fill_pixels(moved))[1]
    assert abs((moved_xs.min() + moved_xs.max() + 1) / 2 - 67.0) <= 0.6
    # A glyph that cannot be centred on a whole pixel is refused, not rounded.
    with pytest.raises(mr.ToolError, match="whole pixel"):
        mr.place_on_body(frame, Image.new("RGBA", (31, 30)))


def test_the_drawn_glyphs_are_centred_on_the_body(package: Path) -> None:
    outputs = {output.name: output.image for output in mr.derive(mr.FolderPackage(package))}
    for name in ("pause.png", "clear_portals.png", "restart.png"):
        cx, cy = centre_of(fill_pixels(outputs[name]))
        assert abs(cx - 64.0) <= 0.3 and abs(cy - 64.0) <= 0.3, (name, cx, cy)
    # Concentric: the ring and the X inside it are each centred on the body, not the X at a corner.
    mask = fill_pixels(outputs["clear_portals.png"])
    yy, xx = np.mgrid[0:128, 0:128]
    radius = np.hypot(xx + 0.5 - 64, yy + 0.5 - 64)
    inner = mr.RING_DIAMETER / 2 - mr.RING_STROKE
    for part in (mask & (radius <= inner - 3), mask & (radius >= inner)):
        assert part.sum() > 100
        cx, cy = centre_of(part)
        assert abs(cx - 64.0) <= 0.3 and abs(cy - 64.0) <= 0.3, (cx, cy)
    # Two bars: nothing in the gap between them, fill on both sides of it.
    row = fill_pixels(outputs["pause.png"])[64]
    assert not row[62:67].any()
    assert row[:62].any() and row[67:].any()


def test_every_output_is_128_rgba_with_its_body_in_place(package: Path) -> None:
    outputs = mr.derive(mr.FolderPackage(package))
    assert [output.name for output in outputs] == list(mr.OUTPUT_NAMES)
    for output in outputs:
        array = np.asarray(output.image)
        assert output.image.mode == "RGBA" and output.image.size == (128, 128)
        assert mr.body_rect(array) == BODY
        assert not mr.has_magenta(array)
        assert mr.RECIPES[output.recipe]
    shipped = {output.name for output in outputs if output.recipe == mr.RECIPE_AS_SHIPPED}
    assert shipped == {"walk_left.png", "walk_right.png"}


def test_a_dark_shadow_in_a_glyphs_alpha_is_not_part_of_the_glyph(tmp_path: Path) -> None:
    shadowed = write_package(tmp_path / "shadowed", halo=True)
    bare = write_package(tmp_path / "bare", halo=False)
    # the fixtures differ, and the shadow does widen the plain alpha's box ...
    def alpha_of(root: Path) -> np.ndarray:
        return np.asarray(Image.open(root / "sprites/icon-restart-64.png").convert("RGBA"))[:, :, 3]

    assert (alpha_of(shadowed) > 0).sum() > (alpha_of(bare) > 0).sum()
    # ... but the restart button made from each is the same picture
    a = {output.name: output for output in mr.derive(mr.FolderPackage(shadowed))}["restart.png"]
    b = {output.name: output for output in mr.derive(mr.FolderPackage(bare))}["restart.png"]
    assert np.array_equal(np.asarray(a.image), np.asarray(b.image))
    # and the ring fills its box: 60 px with the outline, not the shadow's width
    xs = np.nonzero(fill_pixels(a.image))[1]
    assert xs.max() + 1 - xs.min() >= mr.RESTART_BOX - 2 * mr.OUTLINE - 2


def test_validate_refuses_a_wrong_size_a_halo_and_a_wrong_body() -> None:
    good = frame_image()
    mr.validate(mr.Output("good.png", mr.RECIPE_AS_SHIPPED, ("x",), good))
    with pytest.raises(mr.ToolError, match="128"):
        mr.validate(mr.Output("small.png", mr.RECIPE_AS_SHIPPED, ("x",), good.resize((64, 64))))
    halo = np.array(good)
    halo[0, 0] = (9, 9, 9, 0)
    with pytest.raises(mr.ToolError, match="alpha 0"):
        mr.validate(mr.Output("halo.png", mr.RECIPE_AS_SHIPPED, ("x",), Image.fromarray(halo)))
    shifted = np.roll(np.array(good), 3, axis=1)
    with pytest.raises(mr.ToolError, match="solid body"):
        mr.validate(mr.Output("shifted.png", mr.RECIPE_AS_SHIPPED, ("x",),
                              Image.fromarray(shifted)))
