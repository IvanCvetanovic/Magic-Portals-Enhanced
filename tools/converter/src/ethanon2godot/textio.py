"""Reading Ethanon's on-disk text files with the right encoding.

This is separated out because getting it wrong is silent: a UTF-16 file read as
UTF-8 either raises deep inside an XML parser or, worse, decodes into plausible
mojibake. The engine sniffs, so we sniff.

What the engine does (``docs/ethanon-formats.md`` §1, "Encoding sniff"):

* ``.ent`` / ``.esc`` / ``.par`` go through ``FileManager::GetUTFFileString``
  (``gs2d/src/Platform/FileManager.cpp:106-120``, BOM test at ``:69-78``):
  **UTF-16LE if and only if the first two bytes are ``FF FE``**, otherwise UTF-8
  with an optional BOM. In the shipped Ethanon samples, 88 of 89 ``.ent`` files
  are UTF-16LE-with-BOM — so UTF-16 is the common case, not the exception.

* ``.enml`` goes through a *different* function, ``GetAnsiFileString``
  (``FileManager.cpp:122-142``), and is NOT BOM-sniffed. See ``enml.py``.
"""

from __future__ import annotations

from pathlib import Path

# The engine tests for this exact byte pair and nothing else — notably it does
# NOT accept UTF-16 big-endian (FE FF). Mirror that: a BE file is not something
# the original engine could have loaded, so treating it as UTF-8 and failing
# loudly is more honest than silently accepting something Ethanon never would.
UTF16LE_BOM = b"\xff\xfe"


def read_ethanon_xml(path: str | Path) -> str:
    """Read a ``.ent`` / ``.esc`` / ``.par`` file as text, sniffing the BOM.

    Returns the decoded text with any BOM stripped, ready for an XML parser.
    """
    data = Path(path).read_bytes()
    if data[:2] == UTF16LE_BOM:
        # Slice the BOM off explicitly rather than relying on the "utf-16"
        # codec's endianness detection — this way the branch we took is obvious
        # from the code.
        return data[2:].decode("utf-16-le")
    return data.decode("utf-8-sig")
