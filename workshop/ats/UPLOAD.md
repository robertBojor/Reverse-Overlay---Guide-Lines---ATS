# Workshop upload: Backing & Lane Prediction (ATS and ETS2)

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
5. Title: **Backing & Lane Prediction - ATS**
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

### 0.5.0 checklist (ETS2 first upload)

The ETS2 item is a separate Workshop item (Euro Truck Simulator 2 is its own
Steam app). Its files are in `workshop/ets2/`: `description.bbcode.txt` and
`mod_icon.jpg`; the preview is `assets/images/ets2-preview-image.jpg`.

1. Run `python3 -I tools/make_assets.py workshop ets2` (writes `workshop/ets2/upload`).
2. Uploader: Game **Euro Truck Simulator 2**, Item **New**. Mod data:
   `\\wsl.localhost\Ubuntu\home\rb\work\ats-reverse-overlay\workshop\ets2\upload`.
   Preview: `assets/images/ets2-preview-image.jpg`. Title:
   **Backing & Lane Prediction - ETS2**. Visibility **Private** first.
   Description: paste `workshop/ets2/description.bbcode.txt`. Change note:
   `Initial release v0.5.0`.
3. Note the new item's ID from its Workshop URL and replace every
   `ETS2_ITEM_ID` placeholder with it: `grep -rn ETS2_ITEM_ID README.md docs workshop promo`.
   Then paste the ATS description again (it links the ETS2 item) and re-paste
   the ETS2 one.
4. Test the private item in ETS2 with the local `.scs` removed, add
   screenshots, then set it **Public** and publish the GitHub release.
5. ATS item: no new upload is needed (the models are unchanged); only paste
   the updated `workshop/ats/description.bbcode.txt`.

### 0.4.0 checklist

1. Run `python3 -I tools/make_assets.py workshop` (the package gains `line_blue`; the dot models stay for older plugins).
2. Uploader: select the existing item, check the folder and preview paths, paste the new `description.bbcode.txt`.
3. Change note:

   ```
   v0.4.0: The red and green forward markers are replaced by one light, see-through blue line from the front axle along the path the truck will take. At speed it follows how the truck is actually turning, so it stays true on long bends. Update the plugin DLL too: download it from the GitHub releases page.
   ```

4. Replace the screenshots on the Workshop page with ones that show the blue line.

### 0.2.0 checklist

1. Replace the preview images in `assets/images/` and the icon `mod_icon.jpg`, then run `python3 -I tools/make_assets.py workshop`.
2. Uploader: select the existing item, change the **title** to **Backing & Lane Prediction - ATS**, paste the new `description.bbcode.txt`, set the new 640×360 preview.
3. Change note:

   ```
   v0.2.0: Renamed to Backing & Lane Prediction. Forward dots for lane position (red = front-left corner, green = front-right, about 1.5 s ahead). Ctrl+Shift+F9 now cycles forward dots only / reverse lines only / both / nothing, and remembers the choice. Top-down panel removed. Update the plugin DLL too.
   ```

4. Replace the screenshots on the Workshop page with ones that show the dots.
5. Optional: rename the GitHub repo (Settings → General). GitHub redirects the old URL, so the links in the Workshop description keep working; update them on the next release.
