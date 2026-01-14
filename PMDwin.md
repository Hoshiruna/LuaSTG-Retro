# PMDwin: Potential Risks and Notes

- **Input trust**: PMD/PPZ/P86 assets are parsed by legacy code; treat files as untrusted input and prefer sandboxed loading paths.
- **Stability**: Decoder uses global state and a recursive mutex; bugs could impact other audio decoders if it misbehaves.
- **Performance**: 55k modes resample to 44.1k; heavy playback can add CPU cost on weaker hardware.
- **Platform/toolchain limits**: pmdwin is tuned for Windows/MSVC; other toolchains may need tweaks.

GPLv2 considerations (not legal advice):
- pmdwin is GPLv2-only; linking it (static or dynamic) into your binary makes the combined work a GPLv2 derivative when distributed.
- GPLv2 distribution duties include providing corresponding source for the whole derivative work, build scripts, and license text to recipients.
- Proprietary or closed-source distribution is incompatible with GPLv2 obligations; publishing binaries without meeting these terms can create legal risk.
- If you do not want to comply with GPLv2 for your whole project, keep `LUASTG_AUDIO_pmdwin_ENABLE=OFF` for release builds and avoid shipping pmdwin code or binaries.
- For internal/non-distributed builds, GPLv2 obligations do not trigger, but turning on the option for any public release requires full compliance.

Operational guidance:
- Keep PMD/PPZ/P86 assets in read-only locations to reduce tampering risk.
- When disabled, confirm your packaging/installer does not ship `pmdwin` artifacts (should be excluded automatically).
