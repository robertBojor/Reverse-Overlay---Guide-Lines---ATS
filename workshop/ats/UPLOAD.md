# Workshop upload: Reverse Overlay - Guide Lines - ATS

## Files in this folder

| Path | What it is | Upload? |
|---|---|---|
| `upload/` | **Select this folder** as "Mod data" in the uploader. It holds only `versions.sii` and the `universal` package, as the uploader requires. | yes |
| `mod_icon.jpg` | Mod Manager icon, **exactly 276×162 JPEG**, under 1 MB, cropped from the preview banner. After replacing it, run `python3 -I tools/make_assets.py workshop` to copy it into `upload/universal/`. | via `upload/` |
| `images/` | Your preview image and screenshots, **not** part of `upload/`. The preview is commonly 640×360 JPEG/PNG. Screenshots are added on the Workshop page after the first upload. | separately |
| `description.bbcode.txt` | Text for the uploader's description field (Steam BBCode). Its plugin link points to the latest GitHub release. | paste |

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
7. Description: paste `description.bbcode.txt`.
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

Publish the matching plugin release on GitHub **at the same time**. The Workshop description links to the latest release.

### 0.2.0 checklist

1. Replace the preview images in `assets/images/` and the icon `mod_icon.jpg`, then run `python3 -I tools/make_assets.py workshop`.
2. Uploader: select the existing item, paste the new `description.bbcode.txt`, set the new 640×360 preview.
3. Change note:

   ```
   v0.2.0: Forward dots for lane position (red = front-left corner, green = front-right, about 1.5 s ahead). Ctrl+Shift+F9 now cycles forward dots only / reverse lines only / both / nothing, and remembers the choice. Top-down panel removed. Update the plugin DLL too.
   ```

4. Replace the screenshots on the Workshop page with ones that show the dots.
