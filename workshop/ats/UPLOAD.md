# Workshop upload: Reverse Overlay - Guide Lines - ATS

## Files in this folder

| Path | What it is | Upload? |
|---|---|---|
| `upload/` | **Select this folder** as "Mod data" in the uploader. It holds only `versions.sii` and the `universal` package, as the uploader requires. | yes |
| `mod_icon.jpg` | Mod Manager icon, **exactly 276×162 JPEG**, under 1 MB. **You supply it.** Run `python3 -I tools/make_assets.py workshop` afterwards to copy it into `upload/universal/`. | via `upload/` |
| `images/` | Your preview image and screenshots, **not** part of `upload/`. The preview is commonly 640×360 JPEG/PNG. Screenshots are added on the Workshop page after the first upload. | separately |
| `description.bbcode.txt` | Text for the uploader's description field (Steam BBCode). Replace `RELEASE_URL_HERE` with the plugin download link first. | paste |

Generated files (`upload/`) are rebuilt by `tools/make_assets.py workshop`. Don't edit them by hand.

## First upload

1. Steam → Library → filter **Tools** → install and start **SCS Workshop Uploader**.
2. Game: **American Truck Simulator**. Item: **New**.
3. Mod data: `workshop/ats/upload` (on Windows, use the copy under
   `\\wsl.localhost\Ubuntu\home\rb\work\ats-reverse-overlay\workshop\ats\upload`,
   or copy the folder somewhere on `C:` first).
4. Preview image: from `images/`.
5. Title: **Reverse Overlay - Guide Lines - ATS**
6. Visibility: **Private** for the first upload.
7. Description: paste `description.bbcode.txt`, with the release URL filled in.
8. Tags: type tag (e.g. *Others* or *Interior*, whatever fits best).
9. Change note: `Initial release v<VERSION>`.
10. Upload. If validation fails, read
    `Documents\SCS Workshop Uploader\validation.log.txt`.
    First-time uploaders must accept the Steam Workshop agreement in the browser window that opens.

## Test before going public

1. Subscribe to your private item.
2. Remove the local copy (`Documents\American Truck Simulator\mod\ats_reverse_overlay.scs`) or disable it. Don't run both.
3. Enable the Workshop item in the Mod Manager and reverse. The log should show
   "In-world guide lines rendering".
4. Add screenshots on the Workshop page, then set visibility to **Public**.

## Updating

Run `tools/make_assets.py workshop`, then in the uploader select the existing item, check the folder and preview paths, write a change note, and upload. Subscribers get it automatically. Bump `VERSION` for every release so the package version and the DLL log line match.
